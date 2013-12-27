#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "libsvc/http.h"
#include "libsvc/htsmsg_json.h"
#include "libsvc/misc.h"
#include "libsvc/trace.h"
#include "libsvc/cfg.h"
#include "libsvc/db.h"
#include "libsvc/utf8.h"

#include "restapi.h"
#include "spmc.h"
#include "ingest.h"
#include "events.h"

#define API_NO_DATA ((htsmsg_t *)-1)
#define API_ERROR   NULL

#define PUBLIC_PLUGIN_FIELDS "plugin_id, version.created,version,type,author,downloads,showtime_min_version,title,category,synopsis,description,homepage,pkg_digest,icon_digest,published,comment,status,plugin.userid"

#define VERSION_FIELDS "plugin_id, version.created,version,type,author,downloads,showtime_min_version,title,category,synopsis,description,homepage,pkg_digest,icon_digest,published,comment,status"


/**
 *
 */
static htsmsg_t *
public_plugin_to_htsmsg(MYSQL_STMT *q, const char *baseurl)
{
  char plugin_id[128];
  time_t created;
  char version[32];
  char type[128];
  char author[256];
  int downloads;
  char showtime_min_version[32];
  char title[256];
  char category[64];
  char synopsis[512];
  char description[4096];
  char homepage[512];
  char pkg_digest[64];
  char icon_digest[64];
  int published;
  char comment[4096];
  char status[4];
  int userid;
  char url[1024];

  int r = db_stream_row(0, q,
                        DB_RESULT_STRING(plugin_id),
                        DB_RESULT_TIME(created),
                        DB_RESULT_STRING(version),
                        DB_RESULT_STRING(type),
                        DB_RESULT_STRING(author),
                        DB_RESULT_INT(downloads),
                        DB_RESULT_STRING(showtime_min_version),
                        DB_RESULT_STRING(title),
                        DB_RESULT_STRING(category),
                        DB_RESULT_STRING(synopsis),
                        DB_RESULT_STRING(description),
                        DB_RESULT_STRING(homepage),
                        DB_RESULT_STRING(pkg_digest),
                        DB_RESULT_STRING(icon_digest),
                        DB_RESULT_INT(published),
                        DB_RESULT_STRING(comment),
                        DB_RESULT_STRING(status),
                        DB_RESULT_INT(userid));

  if(r < 0)
    return NULL;

  if(r)
    return API_NO_DATA;

  utf8_cleanup_inplace(author,      sizeof(author));
  utf8_cleanup_inplace(title,       sizeof(title));
  utf8_cleanup_inplace(synopsis,    sizeof(synopsis));
  utf8_cleanup_inplace(description, sizeof(description));
  utf8_cleanup_inplace(comment,     sizeof(comment));

  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "id",            plugin_id);
  htsmsg_add_str(m, "version",       version);
  htsmsg_add_u32(m, "created",       created);
  htsmsg_add_str(m, "type",          type);
  htsmsg_add_str(m, "author",        author);
  htsmsg_add_u32(m, "downloads",     downloads);
  htsmsg_add_str(m, "showtime_min_version", showtime_min_version);
  htsmsg_add_str(m, "title",         title);
  htsmsg_add_str(m, "category",      category);
  htsmsg_add_str(m, "synopsis",      synopsis);
  htsmsg_add_str(m, "description",   description);
  htsmsg_add_str(m, "homepage",      homepage);

  if(*icon_digest) {
    snprintf(url, sizeof(url), "%s/data/%s", baseurl, icon_digest);
    htsmsg_add_str(m, "icon", url);
  }

  htsmsg_add_u32(m, "published",     published);
  htsmsg_add_str(m, "comment",       comment);
  htsmsg_add_str(m, "status",        status);
  htsmsg_add_u32(m, "userid",        userid);
  return m;
}



/**
 *
 */
static htsmsg_t *
version_to_htsmsg(MYSQL_STMT *q, const char *baseurl)
{
  char plugin_id[128];
  time_t created;
  char version[32];
  char type[128];
  char author[256];
  int downloads;
  char showtime_min_version[32];
  char title[256];
  char category[64];
  char synopsis[512];
  char description[4096];
  char homepage[512];
  char pkg_digest[64];
  char icon_digest[64];
  int published;
  char comment[4096];
  char status[4];
  char url[1024];

  int r = db_stream_row(0, q,
                        DB_RESULT_STRING(plugin_id),
                        DB_RESULT_TIME(created),
                        DB_RESULT_STRING(version),
                        DB_RESULT_STRING(type),
                        DB_RESULT_STRING(author),
                        DB_RESULT_INT(downloads),
                        DB_RESULT_STRING(showtime_min_version),
                        DB_RESULT_STRING(title),
                        DB_RESULT_STRING(category),
                        DB_RESULT_STRING(synopsis),
                        DB_RESULT_STRING(description),
                        DB_RESULT_STRING(homepage),
                        DB_RESULT_STRING(pkg_digest),
                        DB_RESULT_STRING(icon_digest),
                        DB_RESULT_INT(published),
                        DB_RESULT_STRING(comment),
                        DB_RESULT_STRING(status));

  if(r < 0)
    return NULL;

  if(r)
    return API_NO_DATA;

  utf8_cleanup_inplace(author,      sizeof(author));
  utf8_cleanup_inplace(title,       sizeof(title));
  utf8_cleanup_inplace(synopsis,    sizeof(synopsis));
  utf8_cleanup_inplace(description, sizeof(description));
  utf8_cleanup_inplace(comment,     sizeof(comment));

  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "id",            version);
  htsmsg_add_str(m, "version",       version);
  htsmsg_add_u32(m, "created",       created);
  htsmsg_add_str(m, "type",          type);
  htsmsg_add_str(m, "author",        author);
  htsmsg_add_u32(m, "downloads",     downloads);
  htsmsg_add_str(m, "showtime_min_version", showtime_min_version);
  htsmsg_add_str(m, "title",         title);
  htsmsg_add_str(m, "category",      category);
  htsmsg_add_str(m, "synopsis",      synopsis);
  htsmsg_add_str(m, "description",   description);
  htsmsg_add_str(m, "homepage",      homepage);

  if(*icon_digest) {
    snprintf(url, sizeof(url), "%s/data/%s", baseurl, icon_digest);
    htsmsg_add_str(m, "icon", url);
  }

  htsmsg_add_u32(m, "published",     published);
  htsmsg_add_str(m, "comment",       comment);
  htsmsg_add_str(m, "status",        status);
  return m;
}


/**
 *
 */
static int
do_plugins(http_connection_t *hc, int qtype)
{
  int offset = http_arg_get_int(&hc->hc_req_args, "offset", 0);
  int limit  = http_arg_get_int(&hc->hc_req_args, "limit", 10);
  int userid = http_arg_get_int(&hc->hc_req_args, "userid", 0);
  int admin  = http_arg_get_int(&hc->hc_req_args, "admin", 0);
  char query[1024];
  char buf1[512];
  cfg_root(root);

  conn_t *c = db_get_conn();
  if(c == NULL)
    return 500;

  const char *baseurl = cfg_get_str(root, CFG("baseurl"), NULL);
  if(baseurl == NULL)
    return 500;

  const char *filter = "(plugin_id, version.created) IN (SELECT plugin_id, MAX(created) FROM version WHERE status='a' AND published=true GROUP BY plugin_id)";
  const char *order = "ORDER BY created DESC";


  if(admin) {
    snprintf(buf1, sizeof(buf1), "(plugin_id, version.created) IN (SELECT plugin_id, MAX(version.created) FROM version,plugin WHERE plugin_id = plugin.id GROUP BY plugin_id)");
    filter = buf1;
    order = "ORDER BY plugin_id";
  } else if(userid) {
    snprintf(buf1, sizeof(buf1), "(plugin_id, version.created) IN (SELECT plugin_id, MAX(version.created) FROM version,plugin WHERE userid=%d AND plugin_id = plugin.id GROUP BY plugin_id)", userid);
    filter = buf1;
    order = "ORDER BY plugin_id";
  }


  if(qtype == 0) {

    snprintf(query, sizeof(query),
             "SELECT count(*) "
             "FROM version "
             "WHERE %s", filter);
  } else {

    snprintf(query, sizeof(query),
             "SELECT " PUBLIC_PLUGIN_FIELDS " "
             "FROM version,plugin "
             "WHERE %s "
             "AND plugin.id = version.plugin_id "
             "%s "
             "LIMIT %d "
             "OFFSET %d ",
             filter,
             order,
             limit,
             offset);
  }

  scoped_db_stmt(q, query);
  if(q == NULL)
    return 500;
#if 0
  in[0].buffer_type = MYSQL_TYPE_STRING;
  in[0].buffer = (char *)project;
  in[0].buffer_length = strlen(project);

  if(mysql_stmt_bind_param(q, in)) {
    trace(LOG_ERR,
          "Failed to bind parameters to prepared statement %s -- %s",
          mysql_stmt_sqlstate(q), mysql_stmt_error(q));
    return 500;
  }
#endif

  if(mysql_stmt_execute(q)) {
    trace(LOG_ERR, "Failed to execute statement %s -- %s",
          mysql_stmt_sqlstate(q), mysql_stmt_error(q));
    return 500;
  }

  if(qtype == 0) {
    int numrows;
    int r = db_stream_row(0, q,
                          DB_RESULT_INT(numrows),
                          NULL);
    if(r)
      return 500;

    htsbuf_qprintf(&hc->hc_reply, "%d", numrows);
    return http_output_content(hc, "text/plain");
  }

  htsmsg_t *list = htsmsg_create_list();

  while(1) {
    htsmsg_t *m = public_plugin_to_htsmsg(q, baseurl);
    if(m == NULL)
      return 500;
    if(m == API_NO_DATA)
       break;
    htsmsg_add_msg(list, NULL, m);
  }

  char *json = htsmsg_json_serialize_to_str(list, 1);
  htsmsg_destroy(list);

  htsbuf_append_prealloc(&hc->hc_reply, json, strlen(json));
  return http_output_content(hc, "application/json");
}


/**
 *
 */
static int
plugins_json(http_connection_t *hc, const char *remain, void *opaque)
{
  return do_plugins(hc, 1);
}


/**
 *
 */
static int
plugins_count(http_connection_t *hc, const char *remain, void *opaque)
{
  return do_plugins(hc, 0);
}


/**
 *
 */
static int
plugins(http_connection_t *hc, int argc, char **argv)
{
  htsmsg_t *m;

  if(argc != 2)
    return 500;

  const char *id = argv[1];

  char query[1024];

  conn_t *c = db_get_conn();
  if(c == NULL)
    return 500;

  cfg_root(root);
  const char *baseurl = cfg_get_str(root, CFG("baseurl"), NULL);
  if(baseurl == NULL)
    return 500;

  switch(hc->hc_cmd) {
  case HTTP_CMD_GET:
    {
      snprintf(query, sizeof(query),
               "SELECT userid,betasecret,downloadurl "
               "FROM plugin "
               "WHERE plugin.id = ?");

      scoped_db_stmt(q, query);
      if(q == NULL)
        return 500;

      db_stmt_exec(q, "s", id);

      int userid;
      char betasecret[512];
      char downloadurl[512];

      int r = db_stream_row(0, q,
                            DB_RESULT_INT(userid),
                            DB_RESULT_STRING(betasecret),
                            DB_RESULT_STRING(downloadurl));
      if(r < 0)
        return 500;
      if(r)
        return 404;

      m = htsmsg_create_map();
      htsmsg_add_str(m, "id", id);
      htsmsg_add_u32(m, "userid", userid);
      htsmsg_add_str(m, "betasecret", betasecret);
      htsmsg_add_str(m, "downloadurl", downloadurl);
    }
    break;

  case HTTP_CMD_PUT:

    if(hc->hc_post_message == NULL)
      return 400;

    htsmsg_t *msg = htsmsg_get_map(hc->hc_post_message, "plugin");
    if(msg == NULL)
      return 400;

    const char *betasecret = htsmsg_get_str(msg, "betasecret");
    const char *dlurl      = htsmsg_get_str(msg, "downloadurl");

    MYSQL_STMT *s =
      db_stmt_get(c,
                  "UPDATE plugin "
                  "SET betasecret = ?, downloadurl = ? "
                  "WHERE id = ?");
    if(db_stmt_exec(s, "sss", betasecret, dlurl, id))
      return 500;

    m = htsmsg_create_map();
    break;

  default:
    return 405;
  }

  char *json = htsmsg_json_serialize_to_str(m, 1);
  htsmsg_destroy(m);
  htsbuf_append_prealloc(&hc->hc_reply, json, strlen(json));
  return http_output_content(hc, "application/json");
}


/**
 *
 */
static int
versions(http_connection_t *hc, int argc, char **argv)
{
  if(argc != 2)
    return 500;

  cfg_root(root);
  const char *baseurl = cfg_get_str(root, CFG("baseurl"), NULL);
  if(baseurl == NULL)
    return 500;

  char *id = argv[1];

  conn_t *c = db_get_conn();
  if(c == NULL)
    return 500;

  MYSQL_STMT *s = db_stmt_get(c,
           "SELECT " VERSION_FIELDS " "
           "FROM version "
           "WHERE plugin_id = ? "
           "ORDER BY CREATED DESC");

  if(db_stmt_exec(s, "s", id))
    return 500;

  htsmsg_t *list = htsmsg_create_list();

  while(1) {
    htsmsg_t *m = version_to_htsmsg(s, baseurl);
    if(m == NULL)
      return 500;
    if(m == API_NO_DATA)
       break;
    htsmsg_add_msg(list, NULL, m);
  }
  char *json = htsmsg_json_serialize_to_str(list, 1);
  htsmsg_destroy(list);

  htsbuf_append_prealloc(&hc->hc_reply, json, strlen(json));
  return http_output_content(hc, "application/json");
}


/**
 *
 */
static int
version(http_connection_t *hc, int argc, char **argv)
{
  MYSQL_STMT *s;
  int userid = http_arg_get_int(&hc->hc_req_args, "userid", 0);

  if(argc != 3)
    return 500;

  cfg_root(root);
  const char *baseurl = cfg_get_str(root, CFG("baseurl"), NULL);
  if(baseurl == NULL)
    return 500;

  conn_t *c = db_get_conn();
  if(c == NULL)
    return 500;

  const char *id = argv[1];
  const char *version = argv[2];

  htsmsg_t *m;

  switch(hc->hc_cmd) {
  case HTTP_CMD_DELETE:

    s = db_stmt_get(c,
                    "DELETE FROM version "
                    "WHERE plugin_id = ? AND version = ?");

    if(db_stmt_exec(s, "ss", id, version))
      return 500;

    event_add(c, id, userid, "Deleted %s", version);
    m = htsmsg_create_map();
    break;

  case HTTP_CMD_GET:

    s = db_stmt_get(c,
                    "SELECT " VERSION_FIELDS " "
                    "FROM version "
                    "WHERE plugin_id = ? AND version = ?");

    if(db_stmt_exec(s, "ss", id, version))
      return 500;

    m = version_to_htsmsg(s, baseurl);
    if(m == NULL)
      return 500;
    if(m == API_NO_DATA)
      return 404;

    while(!mysql_stmt_fetch(s)) {}
    break;

  default:
    return 405;
  }

  char *json = htsmsg_json_serialize_to_str(m, 1);
  htsmsg_destroy(m);

  htsbuf_append_prealloc(&hc->hc_reply, json, strlen(json));
  return http_output_content(hc, "application/json");
}



/**
 *
 */
static int
pv_action(http_connection_t *hc, int argc, char **argv)
{
  int userid = http_arg_get_int(&hc->hc_req_args, "userid", 0);

  if(argc != 4)
    return 500;

  if(userid == 0)
    return 403;

  conn_t *c = db_get_conn();
  if(c == NULL)
    return 500;

  const char *id = argv[1];
  const char *version = argv[2];
  const char *action = argv[3];

  db_begin(c);

  const char *info;

  if(!strcmp(action, "publish")) {
    db_stmt_exec(db_stmt_get(c, "UPDATE version SET published=? "
                             "WHERE plugin_id=? AND version=?"),
                 "iss", 1, id, version);
    info = "Published";
  } else if(!strcmp(action, "unpublish")) {
    db_stmt_exec(db_stmt_get(c, "UPDATE version SET published=? "
                             "WHERE plugin_id=? AND version=?"),
                 "iss", 0, id, version);
    info = "Unpublished";
  } else if(!strcmp(action, "approve")) {
    db_stmt_exec(db_stmt_get(c, "UPDATE version SET status=? "
                             "WHERE plugin_id=? AND version=?"),
                 "sss", "a", id, version);
    info = "Approved";
  } else if(!strcmp(action, "reject")) {
    db_stmt_exec(db_stmt_get(c, "UPDATE version SET status=? "
                             "WHERE plugin_id=? AND version=?"),
                 "sss", "r", id, version);
    info = "Rejected";
  } else if(!strcmp(action, "pend")) {
    db_stmt_exec(db_stmt_get(c, "UPDATE version SET status=? "
                             "WHERE plugin_id=? AND version=?"),
                 "sss", "p", id, version);
    info = "Pending";
 } else {
    return 400;
  }
  event_add(c, id, userid, "%s %s", info, version);
  db_commit(c);
  return 200;
}

/**
 *
 */
static void
write_to_file(void *opaque, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(opaque, fmt, ap);
  fprintf(opaque, "\n");
  va_end(ap);
}



/**
 *
 */
static int
ingest(http_connection_t *hc, int argc, char **argv)
{
  FILE *f;
  ingest_result_t result;
  char *out = NULL;
  size_t outlen = 0;
  int r;

  const char *url = http_arg_get(&hc->hc_req_args, "url");
  int userid = http_arg_get_int(&hc->hc_req_args, "userid", 0);
  int admin = http_arg_get_int(&hc->hc_req_args, "admin", 0);
  if(userid == 0)
    return 400;


  f = open_memstream(&out, &outlen);

  if(url != NULL) {
    r = ingest_zip_from_url(url, write_to_file, f, userid,
                            admin ? SPMC_USER_ADMIN : 0,
                            &result);
  } else if(!strcmp(hc->hc_content_type, "application/octet-stream")) {
    r = ingest_zip_from_memory(hc->hc_post_data, hc->hc_post_len,
                               write_to_file, f, userid,
                               admin ? SPMC_USER_ADMIN : 0,
                               &result, NULL);

  } else {
    fclose(f);
    return 400;
  }

  fwrite("", 1, 1, f);
  fclose(f);

  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_u32(m, "error", !!r);
  htsmsg_add_str(m, "result", out);
  if(!r) {
    htsmsg_add_str(m, "pluginid", result.pluginid);
    htsmsg_add_str(m, "version",  result.version);
  }
  free(out);

  char *json = htsmsg_json_serialize_to_str(m, 1);
  htsmsg_destroy(m);

  htsbuf_append_prealloc(&hc->hc_reply, json, strlen(json));
  return http_output_content(hc, "application/json");
}


/**
 *
 */
static int
events(http_connection_t *hc, int argc, char **argv)
{
  int offset = http_arg_get_int(&hc->hc_req_args, "offset", 0);
  int limit  = http_arg_get_int(&hc->hc_req_args, "limit", 10);
  int userid = http_arg_get_int(&hc->hc_req_args, "userid", 0);
  const char *pluginid = http_arg_get(&hc->hc_req_args, "plugin");
  const int qtype = strcmp(argv[1], "count");
  char query[1024];
  char filter[512];

  if(argc != 2)
    return 500;

  conn_t *c = db_get_conn();
  if(c == NULL)
    return 500;

  if(pluginid) {
    snprintf(filter, sizeof(filter), "WHERE plugin_id=?");
  } else if(userid) {
    snprintf(filter, sizeof(filter),
             "WHERE plugin_id IN (SELECT id FROM plugin WHERE userid=%d)", userid);
  } else {
    filter[0] = 0;
  }

  if(qtype == 0) {
    // Count
    snprintf(query, sizeof(query), "SELECT count(*) FROM events %s",
             filter);
  } else {
    snprintf(query, sizeof(query),
             "SELECT created,userid,plugin_id,info FROM events %s "
             "ORDER BY created DESC LIMIT %d OFFSET %d",
             filter, limit, offset);
  }

  scoped_db_stmt(q, query);
  if(q == NULL)
    return 500;

  db_stmt_exec(q, pluginid ? "s" : "", pluginid);

  if(qtype == 0) {
    int numrows;
    int r = db_stream_row(0, q,
                          DB_RESULT_INT(numrows),
                          NULL);
    if(r)
      return 500;

    htsbuf_qprintf(&hc->hc_reply, "%d", numrows);
    return http_output_content(hc, "text/plain");
  }

  htsmsg_t *list = htsmsg_create_list();
  while(1) {
    time_t created;
    int uid;
    char pid[PLUGINID_MAX_LEN];
    char info[1024];

      int r = db_stream_row(0, q,
                            DB_RESULT_TIME(created),
                            DB_RESULT_INT(uid),
                            DB_RESULT_STRING(pid),
                            DB_RESULT_STRING(info));
      if(r < 0) {
        htsmsg_destroy(list);
        return 500;
      }
      if(r)
        break;
      htsmsg_t *m = htsmsg_create_map();
      htsmsg_add_u32(m, "created", created);
      htsmsg_add_u32(m, "userid", uid);
      htsmsg_add_str(m, "pluginid", pid);
      htsmsg_add_str(m, "info", info);
      htsmsg_add_msg(list, NULL, m);
  }

  char *json = htsmsg_json_serialize_to_str(list, 1);
  htsmsg_destroy(list);
  htsbuf_append_prealloc(&hc->hc_reply, json, strlen(json));
  return http_output_content(hc, "application/json");
}



/**
 *
 */
void
restapi_init(void)
{
  http_route_add("/api/ingest$", ingest);

  http_path_add("/api/plugins.json",  NULL, plugins_json);
  http_path_add("/api/plugins.count", NULL, plugins_count);

  http_route_add("/api/events.(json|count)", events);

  http_route_add("/api/plugins/([^/]+).json$", plugins);
  http_route_add("/api/plugins/([^/]+)/versions.json$", versions);
  http_route_add("/api/plugins/([^/]+)/versions/([^/]+)\\.json$", version);

#define PV_ACTIONS "publish|unpublish|approve|reject|pend"

  http_route_add("/api/plugins/([^/]+)/versions/([^/]+)/("PV_ACTIONS").json$",
                 pv_action);
}
