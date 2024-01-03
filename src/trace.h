#ifndef __TRACE_H__
#define __TRACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <assert.h>

#define LIBDAIKIN_ERROR(...)        do { printf("==> (ERR): "); printf(__VA_ARGS__); } while(0);
#define LIBDAIKIN_INFO(...)         do { printf("==> (INF): "); printf(__VA_ARGS__); } while(0);

#ifdef NDEBUG
#   define LIBDAIKIN_ASSERT(x)
#   define LIBDAIKIN_DEBUG(...)
#   define LIBDAIKIN_TRACE(...)
#   define LIBDAIKIN_TRACE_L3(...)
#else
#   define LIBDAIKIN_ASSERT(x)      assert(x);
#   define LIBDAIKIN_DEBUG(...)     do { printf("==> (DBG): "); printf(__VA_ARGS__); } while(0);
#   define LIBDAIKIN_TRACE(...)     //do { printf("==> (TRC): "); printf(__VA_ARGS__); } while(0);
#   define LIBDAIKIN_TRACE_L3(...)  //do { printf("==> (TL3): "); printf(__VA_ARGS__); } while(0);
#endif

#ifdef __cplusplus
}
#endif

#endif
