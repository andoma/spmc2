#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <archive.h>
#include <archive_entry.h>

#include <curl/curl.h>

#include "libsvc/http.h"
#include "libsvc/htsmsg_json.h"
#include "libsvc/misc.h"
#include "libsvc/trace.h"
#include "libsvc/cfg.h"
#include "libsvc/db.h"
#include "libsvc/cmd.h"

#include "sql_statements.h"
#include "spmc.h"
#include "ingest.h"
#include "stash.h"
#include "events.h"

TAILQ_HEAD(file_queue, file);


/**
 *
 */
typedef struct file {
  TAILQ_ENTRY(file) link;
  char *path;
  char *data;
  size_t size;
  int type;
  const char *name;
} file_t;


/**
 *
 */
static void
release_fq(struct file_queue *fq)
{
  file_t *f, *next;

  for(f = TAILQ_FIRST(fq); f != NULL; f = next) {
    next = TAILQ_NEXT(f, link);
    free(f->path);
    free(f->data);
    free(f);
  }
}


/**
 *
 */
static file_t *
find_name(struct file_queue *fq, const char *name)
{
  file_t *f;
  TAILQ_FOREACH(f, fq, link)
    if(!strcmp(f->name, name))
      return f;
  return NULL;
}

/**
 *
 */
static int
ensure_plugin(conn_t *c, const char *id, int userid, const char *origin)
{
  int current_user_id;
  MYSQL_STMT *s;

  s = db_stmt_get(c, "SELECT userid FROM plugin WHERE id=?");
  if(db_stmt_exec(s, "s", id))
    return -1;

  int r = db_stream_row(0, s,
                        DB_RESULT_INT(current_user_id),
                        NULL);

  mysql_stmt_reset(s);

  if(r < 0)
    return -1;

  if(r) {
    s = db_stmt_get(c, "INSERT INTO plugin (id, userid,downloadurl) VALUES (?,?,?)");
    if(db_stmt_exec(s, "sis", id, userid, origin))
      return -1;
    mysql_stmt_reset(s);
    event_add(c, id, userid, "Plugin created");
    return 0;
  }
  return userid != current_user_id;
}


/**
 *
 */
static int
ingest_zip(struct archive *a,
           void (*msg)(void *opaque, const char *fmt, ...),
           void *opaque, int userid, int flags,
           ingest_result_t *result, const char *origin)
{
  htsmsg_t *manifest = NULL;
  char errbuf[512];
  struct archive_entry *entry;
  struct file_queue fq;
  int in_transaction = 0;
  char tstr[64];
  struct tm tm;

  TAILQ_INIT(&fq);

  conn_t *c = db_get_conn();
  if(c == NULL) {
    msg(opaque, "Database connection problems");
    goto fail;
  }

  msg(opaque, "---- Archive contents ---------------");
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {

    file_t *f = calloc(1, sizeof(file_t));
    f->size = archive_entry_size(entry);
    f->data = malloc(f->size + 1);
    f->data[f->size] = 0; // Null terminate all content internally
    f->type = archive_entry_filetype(entry);

    int64_t r = archive_read_data(a, f->data, f->size);

    if(r != f->size) {
      msg(opaque, "%-50s %6d bytes *** FAILED TO EXTRACT FILE ***",
          archive_entry_pathname(entry), (int)f->size);
      free(f->data);
      free(f);
      goto fail;
    } else {
      msg(opaque, "%-50s %6d bytes",
          archive_entry_pathname(entry), (int)f->size);

      f->path = strdup(archive_entry_pathname(entry));
      f->name = f->path;
      TAILQ_INSERT_TAIL(&fq, f, link);
    }
  }

  msg(opaque, "-----------------------------------");

  // --- Try to find the manifest file (plugin.json)

  int strip_path_prefix = 0;
  file_t *json = find_name(&fq, "plugin.json");

  if(json == NULL) {
    file_t *f;

    // plugin.json not found, check if it's in a subdir

    TAILQ_FOREACH(f, &fq, link) {
      const char *x = strchr(f->path, '/');
      if(x != NULL && !strcmp(x + 1, "plugin.json"))
        break;
    }

    if(f == NULL) {
      msg(opaque, "plugin.json was not found in root or in a sub-directory");
      goto fail;
    }

    json = f;

    // Make sure all files are in the same subdir

    const char *x = strchr(f->path, '/');
    assert(x != NULL);
    strip_path_prefix = x - f->path + 1;
    const char *y = f->path;

    TAILQ_FOREACH(f, &fq, link) {
      if(strncmp(f->path, y, strip_path_prefix)) {
        msg(opaque, "%s is not in same sub-directory as %s", f->path, y);
        goto fail;
      }
    }
    msg(opaque, "Stripping %d leading characters from all path names",
        strip_path_prefix);
  }

  file_t *f;
  TAILQ_FOREACH(f, &fq, link)
    f->name = f->path + strip_path_prefix;

  manifest = htsmsg_json_deserialize(json->data, errbuf, sizeof(errbuf));
  if(manifest == NULL) {
    msg(opaque, "Unable to decode plugin.json -- %s", errbuf);
    goto fail;
  }

  const char *type    = htsmsg_get_str(manifest, "type");
  if(type == NULL) {
    msg(opaque, "'type' missing from plugin.json");
    goto fail;
  }

  const char *id      = htsmsg_get_str(manifest, "id");
  if(id == NULL) {
    msg(opaque, "'id' missing from plugin.json");
    goto fail;
  }

  const char *version = htsmsg_get_str(manifest, "version");
  if(version == NULL) {
    msg(opaque, "'version' missing from plugin.json");
    goto fail;
  }

  //
  //  Start actual ingest inside a transaction
  //

  in_transaction = 1;
  db_begin(c);
  MYSQL_STMT *s;

  s = db_stmt_get(c, SQL_CHECK_VERSION);
  if(db_stmt_exec(s, "ss", id, version)) {
    msg(opaque, "Database query problems");
    goto fail;
  }

  time_t created;
  int r = db_stream_row(0, s,
                        DB_RESULT_TIME(created),
                        NULL);

  mysql_stmt_reset(s);

  if(r < 0) {
    msg(opaque, "Database query problems");
    goto fail;
  }

  if(!r) {
    gmtime_r(&created, &tm);
    strftime(tstr, sizeof(tstr), "%d-%b-%Y %T UTC", &tm);
    msg(opaque, "%s %s already ingested at %s", id, version, tstr);
    goto fail;
  }

  r = ensure_plugin(c, id, userid, origin);
  if(r < 0) {
    msg(opaque, "Database query problems");
    goto fail;
  }

  if(r && !(flags & SPMC_USER_ADMIN)) {
    msg(opaque, "Not owner of plugin, ingest denied");
    goto fail;
  }

  //
  // Write out icon
  //

  const char *icon_digest = NULL;
  char pkg_digest[41];
  char icon_digest_str[41];

  const char *iconname = htsmsg_get_str(manifest, "icon");
  if(iconname != NULL) {
    file_t *icon = find_name(&fq, iconname);
    if(icon != NULL) {
      msg(opaque, "Using '%s' as icon", iconname);

      if(stash_write(icon->data, icon->size, icon_digest_str)) {
        msg(opaque, "ERROR: Unable to write icon to disk");
        goto fail;
      }
      icon_digest = icon_digest_str;
    } else {
      msg(opaque, "WARNING: Icon '%s' not found", iconname);
    }
  } else {
    msg(opaque, "NOTICE: No icon specified in plugin.json");
  }

  //
  // Write out package
  //

  char *out = NULL;
  size_t outlen = 0;

  FILE *memfile = open_memstream(&out, &outlen);

  struct archive *aw = archive_write_new();
  archive_write_set_bytes_per_block(aw, 0);
  archive_write_set_format_zip(aw);

  // We don't want to compress stuff as it defeats the binary diff protocol
  // used for sending upgrades
  archive_write_set_format_option(aw, "zip", "compression", "store");

  archive_write_open_FILE(aw, memfile);

  TAILQ_FOREACH(f, &fq, link) {
    if(f->name[0] == 0 || f->name[0] == '.')
      continue;

    struct archive_entry *ae = archive_entry_new();
    archive_entry_set_pathname(entry, f->name);
    archive_entry_set_size(entry, f->size);
    archive_entry_set_filetype(entry, f->type);
    archive_write_header(aw, entry);
    archive_write_data(aw, f->data, f->size);
    archive_entry_free(ae);
  }
  archive_write_free(aw);
  fclose(memfile);

  if(stash_write(out, outlen, pkg_digest)) {
    msg(opaque, "ERROR: Unable to write pkt to disk");
    free(out);
    goto fail;
  }
  free(out);

  //
  // Ok, do the actual insert
  //

  const char *author               = htsmsg_get_str(manifest, "author") ?: "";
  const char *showtime_min_version = htsmsg_get_str(manifest, "showtimeVersion") ?: "";
  const char *title                = htsmsg_get_str(manifest, "title") ?: "";
  const char *category             = htsmsg_get_str(manifest, "category") ?: "";
  const char *synopsis             = htsmsg_get_str(manifest, "synopsis") ?: "";
  const char *description          = htsmsg_get_str(manifest, "description") ?: "";
  const char *homepage             = htsmsg_get_str(manifest, "homepage") ?: "";
  const char *comment              = htsmsg_get_str(manifest, "comment") ?: "";

  const char *status = "p";

  if(flags & SPMC_USER_AUTOAPPROVE)
    status = "a";

  s = db_stmt_get(c, SQL_INSERT_VERSION);
  if(db_stmt_exec(s, "ssssssssssssss",
                  id,
                  version,
                  type,
                  author,
                  showtime_min_version,
                  title,
                  category,
                  synopsis,
                  description,
                  homepage,
                  pkg_digest,
                  icon_digest,
                  comment,
                  status)) {
    msg(opaque, "Database query problems");
    goto fail;
  }

  const char *statustxt = *status == 'p' ? "Pending" : "Auto-approved";

  event_add(c, id, userid, "Ingested version '%s' status: %s", version, statustxt);
  msg(opaque, "OK, Ingested %s version %s  status: %s", id, version, statustxt);

  if(result != NULL) {
    snprintf(result->pluginid, sizeof(result->pluginid), "%s", id);
    snprintf(result->version,  sizeof(result->version),  "%s", version);
  }

  htsmsg_destroy(manifest);

  db_commit(c);

  release_fq(&fq);

  return 0;

 fail:
  if(in_transaction)
    db_rollback(c);

  if(manifest != NULL)
    htsmsg_destroy(manifest);
  release_fq(&fq);
  return 1;
}


/**
 *
 */
static struct archive *
make_archive(void)
{
  struct archive *a = archive_read_new();
  archive_read_support_compression_all(a);
  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);
  return a;
}


/**
 *
 */
int
ingest_zip_from_memory(const void *data, size_t datalen,
                       void (*msg)(void *opaque, const char *fmt, ...),
                       void *opaque, int userid, int flags,
                       ingest_result_t *result, const char *origin)
{
  struct archive *a = make_archive();
  int r = archive_read_open_memory(a, (void *)data, datalen);
  if(r) {
    msg(opaque, "%s", archive_error_string(a));
  } else {
    r = ingest_zip(a, msg, opaque, userid, flags, result, origin);
  }
  archive_read_free(a);
  return r;
}


/**
 *
 */
int
ingest_zip_from_url(const char *url,
                    void (*msg)(void *opaque, const char *fmt, ...),
                    void *opaque, int userid, int flags,
                    ingest_result_t *result)
{
  if(strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
    msg(opaque, "Invalid protocol: %s", url);
    return 1;
  }

  char *out = NULL;
  size_t outlen = 0;

  CURL *curl = curl_easy_init();

  FILE *f = open_memstream(&out, &outlen);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  CURLcode r = curl_easy_perform(curl);
  fclose(f);
  curl_easy_cleanup(curl);

  if(r) {
    msg(opaque, "Unable to download %s -- %s", url, curl_easy_strerror(r));
    return 1;
  }

  int x = ingest_zip_from_memory(out, outlen, msg, opaque, userid, flags,
                                 result, url);
  free(out);
  return x;
}


/**
 *
 */
int
ingest_zip_from_path(const char *path,
                     void (*msg)(void *opaque, const char *fmt, ...),
                     void *opaque, int userid, int flags,
                     ingest_result_t *result)
{

  if(!strncmp(path, "http://", 7) || !strncmp(path, "https://", 8))
    return ingest_zip_from_url(path, msg, opaque, userid, flags, result);

  struct archive *a = make_archive();
  int r = archive_read_open_filename(a, path, 8192);

  if(r) {
    msg(opaque, "%s", archive_error_string(a));
  } else {
    r = ingest_zip(a, msg, opaque, userid, flags, result, NULL);
  }
  archive_read_free(a);
  return r;
}


/**
 *
 */
static int
ingest_file(const char *user,
            int argc, const char **argv, int *intv,
            void (*msg)(void *opaque, const char *fmt, ...),
            void *opaque)
{
  ingest_zip_from_path(argv[0], msg, opaque, 1, SPMC_USER_ADMIN, NULL);
  return 0;
}


CMD(ingest_file,
    CMD_LITERAL("ingest"),
    CMD_LITERAL("file"),
    CMD_VARSTR("path")
    );
