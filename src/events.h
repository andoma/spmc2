#include "libsvc/db.h"

void event_add(conn_t *c, const char *pluginid, int userid, const char *fmt, ...);

void event_init(void);
