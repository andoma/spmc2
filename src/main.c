#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <mysql.h>
#include <time.h>

#include "libsvc/htsmsg_json.h"

#include "libsvc/tcp.h"
#include "libsvc/http.h"
#include "libsvc/trace.h"
#include "libsvc/irc.h"
#include "libsvc/cfg.h"
#include "libsvc/ctrlsock.h"
#include "libsvc/db.h"
#include "libsvc/cmd.h"
#include "libsvc/libsvc.h"

#include "showtime.h"
#include "spmc.h"
#include "restapi.h"
#include "stash.h"
#include "events.h"

static int running = 1;
static int reload = 0;

/**
 *
 */
static void
handle_sigpipe(int x)
{
  return;
}


/**
 *
 */
static void
doexit(int x)
{
  running = 0;
}



/**
 *
 */
static void
doreload(int x)
{
  reload = 1;
}


/**
 *
 */
static void
refresh_subsystems(void)
{
  irc_refresh_config();
}


/**
 *
 */
static void
http_init(void)
{
  cfg_root(cr);

  int port = cfg_get_int(cr, CFG("http", "port"), 9000);
  const char *bindaddr = cfg_get_str(cr, CFG("http", "bindAddress"),
                                     "127.0.0.1");
  if(http_server_init(port, bindaddr))
    exit(1);
}


/**
 *
 */
int
main(int argc, char **argv)
{
  int c;
  sigset_t set;
  const char *cfgfile = NULL;
  const char *ctrlsockpath = "/tmp/spmcctrl";

  signal(SIGPIPE, handle_sigpipe);

  while((c = getopt(argc, argv, "c:s:")) != -1) {
    switch(c) {
    case 'c':
      cfgfile = optarg;
      break;
    case 's':
      enable_syslog("spmc", optarg);
      break;
    }
  }

  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, NULL);

  srand48(getpid() ^ time(NULL));

  if(cfg_load(cfgfile, "config.json")) {
    fprintf(stderr, "Unable to load config (check -c option). Giving up\n");
    exit(1);
  }

  libsvc_init();

  http_init();

  if(db_upgrade_schema("sql")) {
    fprintf(stderr, "Unable to upgrade database schema. Giving up\n");
    exit(1);
  }

  ctrlsock_init(ctrlsockpath);

  event_init();

  showtime_init();

  restapi_init();

  stash_init();

  running = 1;
  sigemptyset(&set);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGHUP);

  signal(SIGTERM, doexit);
  signal(SIGINT, doexit);
  signal(SIGHUP, doreload);

  pthread_sigmask(SIG_UNBLOCK, &set, NULL);

  while(running) {
    if(reload) {
      reload = 0;
      if(!cfg_load(NULL, "config.json")) {
        refresh_subsystems();
      }
    }
    pause();
  }

  return 0;
}


uint32_t
parse_version_int(const char *str)
{
  int major = 0, minor = 0, commit = 0;
  sscanf(str, "%d.%d.%d", &major, &minor, &commit);
  return major * 10000000 + minor * 100000 + commit;
}
