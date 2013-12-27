#include <sys/param.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <unistd.h>

#include <openssl/sha.h>

#include "libsvc/cfg.h"
#include "libsvc/misc.h"
#include "libsvc/trace.h"
#include "libsvc/http.h"

#include "stash.h"

int
stash_write(const void *data, size_t size, char digest[41])
{
  cfg_root(root);
  const char *stashdir = cfg_get_str(root, CFG("stashdir"), NULL);
  if(stashdir == NULL) {
    trace(LOG_ERR, "No stashdir configured, unable to ingest anything");
    return -1;
  }
  uint8_t md[20];
  char path[PATH_MAX];
  SHA1(data, size, md);
  bin2hex(digest, 41, md, 20);

  snprintf(path, sizeof(path), "%s/%.*s", stashdir, 2, digest);
  if(makedirs(path)) {
    trace(LOG_ERR, "Unable to mkdir('%s') -- %s", path, strerror(errno));
    return -1;
  }

  snprintf(path, sizeof(path), "%s/%.*s/%s", stashdir, 2, digest, digest);

  int r = writefile(path, data, size);
  if(r == WRITEFILE_NO_CHANGE)
    return 0;
  if(r)
    trace(LOG_ERR, "Unable to write('%s') -- %s", path, strerror(r));
  return r;
}

/**
 *
 */
static int
do_send_file(http_connection_t *hc, const char *ct,
             int content_len, const char *ce, int fd)
{

  http_send_header(hc, HTTP_STATUS_OK, ct, content_len, ce,
                   NULL, 0, NULL, NULL, NULL);

  if(!hc->hc_no_output) {
    while(content_len > 0) {
      int chunk = MIN(1024 * 1024 * 1024, content_len);
      int r = tcp_sendfile(hc->hc_ts, fd, chunk);
      if(r == -1) {
        close(fd);
        return -1;
      }
      content_len -= r;
    }
  }
  close(fd);
  return 0;
}


/**
 *
 */
static int
send_data(http_connection_t *hc, const char *remain, void *opaque)
{
  char path[PATH_MAX];

  cfg_root(root);

  if(remain == NULL || strstr(remain, ".") || strstr(remain, "/") ||
     strlen(remain) != 40)
    return 404;

  const char *stashdir = cfg_get_str(root, CFG("stashdir"), NULL);
  if(stashdir == NULL)
    return 500;


  snprintf(path, sizeof(path), "%s/%.2s/%s", stashdir, remain, remain);

  int fd = open(path, O_RDONLY);
  if(fd == -1) {
    trace(LOG_INFO, "Missing file '%s' -- %s", path, strerror(errno));
      return 404;
  }

  struct stat st;
  if(fstat(fd, &st)) {
    trace(LOG_INFO, "Stat failed for file '%s'  -- %s",
          path, strerror(errno));
    close(fd);
    return 404;
  }

  const char *ct = NULL;
  const char *ce = NULL;

  int content_len = st.st_size;

  if(do_send_file(hc, ct, content_len, ce, fd))
    return -1;
  return 0;
}


/**
 *
 */
void
stash_init(void)
{
  http_path_add("/public/data",  NULL, send_data);
}
