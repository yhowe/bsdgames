#if defined(APP_DEBUG)
#if defined(BT_DEBUG)
#define DPRINTF(...)	serialBT.printf(__VA_ARGS__);
#else
#define DPRINTF(...)	fprintf(stderr, __VA_ARGS__);
#endif
#else /* !DEBUG */
#define DPRINTF(...)
#endif
