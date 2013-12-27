#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <pthread.h>
#include <curl/curl.h>

#include "libsvc/misc.h"
#include "libsvc/trace.h"
#include "libsvc/cfg.h"
#include "libsvc/db.h"
#include "libsvc/utf8.h"
#include "libsvc/htsmsg_json.h"

#include "events.h"

static TAILQ_HEAD(, event) events;

typedef struct event {
  TAILQ_ENTRY(event) link;
  int userid;
  char *pluginid;
  char *info;
} event_t;


static pthread_mutex_t event_mutex;
static pthread_cond_t event_cond;


typedef struct user_info {
  char name[256];
  char mail[256];
} user_info_t;

static void
resolve_user(int userid, user_info_t *ui, cfg_t *cfg)
{
  snprintf(ui->name, sizeof(ui->name), "User#%d", userid);
  ui->mail[0] = 0;

  const char *baseurl = cfg_get_str(cfg, CFG("redmine", "baseurl"), NULL);
  const char *apikey  = cfg_get_str(cfg, CFG("redmine", "apikey"), NULL);
  char url[1024];

  if(baseurl == NULL || apikey == NULL)
    return;

  struct curl_slist *slist = NULL;
  char auth[256];

  snprintf(auth, sizeof(auth), "X-Redmine-API-Key: %s", apikey);
  slist = curl_slist_append(slist, auth);

  snprintf(url, sizeof(url), "%s/users/%d.json", baseurl, userid);

  char *out = NULL;
  size_t outlen = 0;

  FILE *f = open_memstream(&out, &outlen);

  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

  CURLcode r = curl_easy_perform(curl);
  curl_slist_free_all(slist);
  fwrite("", 1, 1, f);
  fclose(f);
  curl_easy_cleanup(curl);

  if(r) {
    trace(LOG_ERR, "Unable to query %s -- CURL error: %d", url, r);
    free(out);
    return;
  }
  char errbuf[256];
  htsmsg_t *m = htsmsg_json_deserialize(out, errbuf, sizeof(errbuf));
  free(out);
  if(m == NULL) {
    trace(LOG_ERR, "Unable to decode JSON from %s -- %s", url, out);
    return;
  }

  htsmsg_t *u = htsmsg_get_map(m, "user");
  if(u != NULL) {
    const char *firstname = htsmsg_get_str(u, "firstname");
    const char *lastname = htsmsg_get_str(u, "lastname");
    const char *username = htsmsg_get_str(u, "login");
    const char *mail = htsmsg_get_str(u, "mail");

    if(firstname && lastname) {
      snprintf(ui->name, sizeof(ui->name), "%s %s", firstname, lastname);
    } else if(username) {
      snprintf(ui->name, sizeof(ui->name), "%s", username);
    }

    if(mail != NULL) {
      snprintf(ui->mail, sizeof(ui->mail), "%s", mail);
    }
  }
  htsmsg_destroy(m);
}

/**
 *
 */
static void
sendmail(const char *recipient, const char *subject, const char *body,
         cfg_t *cfg)
{
  const char *sender = cfg_get_str(cfg, CFG("email", "sender"), NULL);
  if(sender == NULL)
    return;

  char cmd[512];

  if(recipient[0] == '-' || strstr(recipient, " "))
    return;

  snprintf(cmd, sizeof(cmd), "sendmail %s", recipient);

  FILE *out = popen(cmd, "w");
  if(out == NULL) {
    perror(cmd);
    return;
  }

  trace(LOG_INFO, "Sending mail From:%s To:%s Subject:'%s'",
        sender, recipient, subject);

  fprintf(out, "Subject: %s\n", subject);
  fprintf(out, "From: %s\n", sender);
  fprintf(out, "To: %s\n", sender);
  fprintf(out, "\n");
  fprintf(out, "%s", body);
  fprintf(out, "\n");
  fclose(out);
}



/**
 *
 */
static void
event_process(event_t *e, conn_t *c)
{
  char subject[256];
  char body[512];
  cfg_root(cfg);
  user_info_t actor, owner;

  const char *adminmail = cfg_get_str(cfg, CFG("admin", "email"), NULL);

  resolve_user(e->userid, &actor, cfg);

  snprintf(subject, sizeof(subject), "SPMC %s %s", e->pluginid, e->info);


  body[0] = 0;

  const char *linkprefix = cfg_get_str(cfg, CFG("email", "linkprefix"), NULL);

  snprintf(body + strlen(body), sizeof(body) - strlen(body),
           "Change made by: %s <%s>\n", actor.name, actor.mail);

  if(linkprefix != NULL) {
    snprintf(body + strlen(body), sizeof(body) - strlen(body),
             "%s%s\n", linkprefix, e->pluginid);
  }
  snprintf(body + strlen(body), sizeof(body) - strlen(body),
           "--\nAutomated mail from SPMC\n");

  if(adminmail)
    sendmail(adminmail, subject, body, cfg);

  trace(LOG_INFO, "Plugin '%s' changed by '%s <%s>' %s",
        e->pluginid, actor.name, actor.mail, e->info);

  MYSQL_STMT *s = db_stmt_get(c, "SELECT userid FROM plugin WHERE id=?");

  if(db_stmt_exec(s, "s", e->pluginid))
    return;

  int ownerid;
  int r = db_stream_row(0, s, DB_RESULT_INT(ownerid), NULL);

  if(r)
    return;

  resolve_user(ownerid, &owner, cfg);

  if(owner.mail)
    sendmail(owner.mail, subject, body, cfg);
}


/**
 *
 */
static void *
event_worker_thread(void *aux)
{
  event_t *e;

  conn_t *c = db_get_conn();
  if(c == NULL) {
    trace(LOG_ALERT, "Unable to connect to database");
    exit(1);
  }

  pthread_mutex_lock(&event_mutex);
  while(1) {
    if((e = TAILQ_FIRST(&events)) == NULL) {
      pthread_cond_wait(&event_cond, &event_mutex);
      continue;
    }

    TAILQ_REMOVE(&events, e, link);
    pthread_mutex_unlock(&event_mutex);
    event_process(e, c);
    free(e->pluginid);
    free(e->info);
    free(e);
    pthread_mutex_lock(&event_mutex);
  }
  return NULL;
}



/**
 *
 */
void
event_add(conn_t *c, const char *pluginid, int userid, const char *fmt, ...)
{
  char buf[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  MYSQL_STMT *s =
    db_stmt_get(c,
                "INSERT INTO events (userid, plugin_id, info) VALUES (?,?,?)");

  if(db_stmt_exec(s, "iss", userid, pluginid, buf))
    return;

  event_t *e = malloc(sizeof(event_t));
  e->userid = userid;
  e->pluginid = strdup(pluginid);
  e->info = strdup(buf);
  pthread_mutex_lock(&event_mutex);
  TAILQ_INSERT_TAIL(&events, e, link);
  pthread_cond_signal(&event_cond);
  pthread_mutex_unlock(&event_mutex);
}


/**
 *
 */
void
event_init(void)
{
  pthread_t tid;
  TAILQ_INIT(&events);

  pthread_mutex_init(&event_mutex, NULL);
  pthread_cond_init(&event_cond, NULL);

  pthread_create(&tid, NULL, event_worker_thread, NULL);
}
