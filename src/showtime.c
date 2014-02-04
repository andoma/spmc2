#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "libsvc/http.h"
#include "libsvc/htsmsg_json.h"
#include "libsvc/misc.h"
#include "libsvc/trace.h"
#include "libsvc/cfg.h"
#include "libsvc/db.h"

#include <openssl/sha.h>

#include "showtime.h"
#include "sql_statements.h"
#include "spmc.h"

//	ua_re = regexp.MustCompile("^Showtime [^ ]+ ([0-9]+)\\.([0-9]+)\\.([0-9]+)"); 

LIST_HEAD(plugin_list, plugin);

typedef struct plugin {
  LIST_ENTRY(plugin) link;

  uint32_t intver;

  const char *id;
  const char *version;
  const char *type;
  const char *author;
  const char *showtime_min_version;
  const char *title;
  const char *synopsis;
  const char *description;
  const char *homepage;
  const char *category;
  const char *downloadURL;
  const char *icon;

} plugin_t;

#define SETVAL(field) do {                              \
    if(p->field == NULL || strcmp(p->field, field)) {   \
      p->field = mystrdupa(field);                      \
    }                                                   \
  } while(0)


/**
 *
 */
static int
check_password(http_connection_t *hc, const char *str)
{
  http_arg_t *ra;

  if(str == NULL || *str == 0)
    return 0;

  TAILQ_FOREACH(ra, &hc->hc_req_args, link)
    if(!strcmp(ra->key, "betapassword") && !strcmp(ra->val, str))
      return 1;
  return 0;
}


/**
 *
 */
static int
plugins_v1_json(http_connection_t *hc, const char *remain,
                void *opaque)
{
  cfg_root(root);

  const char *baseurl = cfg_get_str(root, CFG("baseurl"), NULL);

  if(baseurl == NULL)
    return 400;

  const char *ua = http_arg_get(&hc->hc_args, "user-agent");

  int bypass_access_control =
    check_password(hc, cfg_get_str(root, CFG("admin", "betapassword"), NULL));

  int reqversion = INT32_MAX;
  //  const char *arch = NULL;

  if(ua != NULL) {
    const char *x = mystrbegins(ua, "Showtime ");
    if(x != NULL) {
      char *y = mystrdupa(x);
      //      arch = y;
      y = strchr(y, ' ');
      if(y != NULL) {
        *y++ = 0;
        reqversion = parse_version_int(y);
      }
    }
  }

  db_conn_t *c = db_get_conn();
  if(c == NULL)
    return 500;

  db_stmt_t *s = db_stmt_get(c, SQL_GET_ALL);

  if(db_stmt_exec(s, ""))
    return 500;

  struct plugin_list plugins;
  LIST_INIT(&plugins);

  htsmsg_t *blacklist = htsmsg_create_list();

  while(1) {

    time_t created;
    char id[128];
    char version[64];
    char type[64];
    char author[128];
    char showtime_min_version[64];
    int downloads;
    char title[256];
    char category[64];
    char synopsis[256];
    char description[4096];
    char homepage[256];
    char pkg_digest[64];
    char icon_digest[64];
    int published;
    char comment[4096];
    char status[8];
    char betasecret[64];

    int r = db_stream_row(0, s,
                          DB_RESULT_STRING(id),
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
                          DB_RESULT_STRING(betasecret)
                          );
    if(r)
      break;

    int beta = check_password(hc, betasecret);

    if(*status == 'r') {
      // Rejected pluginversion, add to blacklist
      htsmsg_t *m = htsmsg_create_map();
      htsmsg_add_str(m, "id", id);
      htsmsg_add_str(m, "version", version);
      htsmsg_add_msg(blacklist, NULL, m);
      continue;
    }

    if(!bypass_access_control) {

      if(*status != 'a' && (!beta || downloads >= 5000))
        continue;

      if(!published && !beta)
        continue;
    }

    int intver    = parse_version_int(version);
    int intminver = parse_version_int(showtime_min_version);

    if(intminver > reqversion)
      continue;

    plugin_t *p;
    LIST_FOREACH(p, &plugins, link)
      if(!strcmp(p->id, id))
        break;

    if(p != NULL && p->intver > intver)
      continue;

    if(p == NULL) {
      p = alloca(sizeof(plugin_t));
      memset(p, 0, sizeof(plugin_t));
      LIST_INSERT_HEAD(&plugins, p, link);
      p->id = mystrdupa(id);
    }

    p->intver   = intver;
    p->version  = mystrdupa(version);
    SETVAL(type);
    SETVAL(author);
    SETVAL(showtime_min_version);
    SETVAL(title);
    SETVAL(synopsis);
    SETVAL(description);
    SETVAL(homepage);
    SETVAL(category);

    char url[1024];

    snprintf(url, sizeof(url), "%s/data/%s", baseurl, pkg_digest);
    p->downloadURL = mystrdupa(url);

    if(*icon_digest) {
      snprintf(url, sizeof(url), "%s/data/%s", baseurl, icon_digest);
      p->icon = mystrdupa(url);
    } else {
      p->icon = NULL;
    }
  }

  plugin_t *p;

  htsmsg_t *pm = htsmsg_create_list();
  LIST_FOREACH(p, &plugins, link) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "id",              p->id);
    htsmsg_add_str(m, "version",         p->version);
    htsmsg_add_str(m, "type",            p->type);
    htsmsg_add_str(m, "author",          p->author);
    htsmsg_add_str(m, "showtimeVersion", p->showtime_min_version);
    htsmsg_add_str(m, "title",           p->title);
    htsmsg_add_str(m, "synopsis",        p->synopsis);
    htsmsg_add_str(m, "description",     p->description);
    htsmsg_add_str(m, "homepage",        p->homepage);
    htsmsg_add_str(m, "category",        p->category);
    htsmsg_add_str(m, "downloadURL",     p->downloadURL);
    if(p->icon)
      htsmsg_add_str(m, "icon",          p->icon);
    htsmsg_add_msg(pm, NULL, m);
  }

  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_u32(m, "version", 1);
  htsmsg_add_msg(m, "plugins", pm);
  htsmsg_add_msg(m, "blacklist", blacklist);

  char *json = htsmsg_json_serialize_to_str(m, 1);
  htsmsg_destroy(m);

  uint8_t md[20];
  char digest[41];
  SHA1((void *)json, strlen(json), md);
  bin2hex(digest, 41, md, 20);

  http_arg_set(&hc->hc_response_headers, "ETag", digest);

  const char *cached_copy = http_arg_get(&hc->hc_args, "If-None-Match");
  if(cached_copy && !strcmp(cached_copy, digest)) {
    free(json);
    return 304;
  }

  htsbuf_append_prealloc(&hc->hc_reply, json, strlen(json));
  http_output_content(hc, "application/json");
  return 0;
}


/**
 *
 */
void
showtime_init(void)
{
  http_path_add("/public/plugins-v1.json", NULL, plugins_v1_json);
}
