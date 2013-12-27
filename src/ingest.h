

typedef struct ingest_result {
  char pluginid[PLUGINID_MAX_LEN];
  char version[64];
} ingest_result_t;

int ingest_zip_from_path(const char *path,
                         void (*msg)(void *opaque, const char *fmt, ...),
                         void *opaque, int userid, int flags,
                         ingest_result_t *result);

int ingest_zip_from_memory(const void *data, size_t datalen,
                           void (*msg)(void *opaque, const char *fmt, ...),
                           void *opaque, int userid, int flags,
                           ingest_result_t *result, const char *origin);

int ingest_zip_from_url(const char *url,
                        void (*msg)(void *opaque, const char *fmt, ...),
                        void *opaque, int userid, int flags,
                        ingest_result_t *result);
