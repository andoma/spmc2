#include "libsvc/db.h"

void event_add(db_conn_t *c, const char *pluginid, int userid, const char *fmt, ...);

void event_init(void);
