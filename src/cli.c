#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#include "libsvc/misc.h"
#include "libsvc/trace.h"
#include "libsvc/db.h"
#include "libsvc/cmd.h"

#include "cli.h"

#include "sql_statements.h"



void
cli_init(void)
{

}


static int
show_plugin(const char *user,
            int argc, const char **argv, int *intv,
            void (*msg)(void *opaque, const char *fmt, ...),
            void *opaque)
{
  MYSQL_STMT *s;
  char tstr[64];
  struct tm tm;
  conn_t *c = db_get_conn();
  if(c == NULL) {
    msg(opaque, "Database connection problems");
    return 0;
  }

  s = db_stmt_get(c, SQL_GET_PLUGIN_BY_ID);
  if(db_stmt_exec(s, "s", argv[0])) {
    msg(opaque, "Database query problems");
    return 0;
  }

  time_t created;
  int userid;
  char betasecret[128];
  char downloadurl[1024];

  int r = db_stream_row(0, s,
                        DB_RESULT_TIME(created),
                        DB_RESULT_INT(userid),
                        DB_RESULT_STRING(betasecret),
                        DB_RESULT_STRING(downloadurl));

  mysql_stmt_reset(s);

  if(r < 0) {
    msg(opaque, "Database query problems");
    return 0;
  }

  if(r) {
    msg(opaque, "No such plugin");
    return 0;
  }

  msg(opaque, "'%s' owned by user #%d", argv[0], userid);
  gmtime_r(&created, &tm);
  strftime(tstr, sizeof(tstr), "%d-%b-%Y %T UTC", &tm);
  msg(opaque, "  Created %s   Betasecret: %s", tstr, betasecret);
  msg(opaque, "  Download URL: %s", downloadurl);


  msg(opaque, "");
  msg(opaque, "Versions:");

  s = db_stmt_get(c, SQL_GET_PLUGIN_VERSIONS);

  if(db_stmt_exec(s, "s", argv[0])) {
    msg(opaque, "Database query problems");
    return 0;
  }

  while(1) {

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

    r = db_stream_row(0, s,
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
                      DB_RESULT_STRING(status)
                      );
    if(r)
      break;

    gmtime_r(&created, &tm);
    strftime(tstr, sizeof(tstr), "%d-%b-%Y %T UTC", &tm);

    msg(opaque, "%-9s %-25s %s    %-9s    %-9s", version, title, tstr,
        published ? "Published" : "",
        *status == 'a' ? "Approved" :
        *status == 'r' ? "Rejected" :
        *status == 'p' ? "Pending" : "Unknown");
    msg(opaque, "          '%s' - %s", *category ? category : "<no category>", synopsis);
    msg(opaque, "          '%s' requierd Showtime ver. %s", type, showtime_min_version);
    msg(opaque, "          %d downloads", downloads);

    msg(opaque, "");
  }

  return 0;
}


CMD(show_plugin,
    CMD_LITERAL("show"),
    CMD_LITERAL("plugin"),
    CMD_VARSTR("pluginid")
    );





static int
delete_plugin(const char *user,
            int argc, const char **argv, int *intv,
            void (*msg)(void *opaque, const char *fmt, ...),
            void *opaque)
{
  MYSQL_STMT *s;
  conn_t *c = db_get_conn();
  if(c == NULL) {
    msg(opaque, "Database connection problems");
    return 0;
  }

  s = db_stmt_get(c, "DELETE FROM version WHERE plugin_id=? AND version=?");
  if(db_stmt_exec(s, "ss", argv[0], argv[1])) {
    msg(opaque, "Database query problems");
    return 0;
  }
  if(mysql_affected_rows(c->m))
    trace(LOG_NOTICE, "User '%s' deleted %s %s", user, argv[0], argv[1]);
  msg(opaque, "OK, %d rows deleted", mysql_affected_rows(c->m));
  return 0;
}

CMD(delete_plugin,
    CMD_LITERAL("delete"),
    CMD_LITERAL("plugin"),
    CMD_VARSTR("pluginid"),
    CMD_VARSTR("version")
    );
