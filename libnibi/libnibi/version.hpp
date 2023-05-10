#ifndef LIBNIBI_VERSION_
#define LIBNIBI_VERSION_

#ifndef COMPILED_GIT_HASH
#define COMPILED_GIT_HASH "unknown"
#endif

#define LIBNIBI_MAJOR_VERSION (0)
#define LIBNIBI_MINOR_VERSION (1)
#define LIBNIBI_PATCH_VERSION (0)
#define LIBNIBI_VERSION "0.1.0"

static const char *get_build_hash() { return COMPILED_GIT_HASH; }

#endif // LIBNIBI_VERSION_