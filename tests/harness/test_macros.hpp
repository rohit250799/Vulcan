// Why: No heap allocation, no exceptions, no signal handlers. Pure C I/O.

#ifndef TEST_MACROS_HPP
#define TEST_MACROS_HPP

#include <cstdio>

// No std::assert - it calls abort() and prevents cleanup
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, msg); \
            return 1; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "FAIL: %s:%d: %s (%lld != %lld)\n", \
                    __FILE__, __LINE__, msg, (long long)(a), (long long)(b)); \
            return 1; \
        } \
    } while(0)

#define TEST_SKIP(msg) \
    do { \
        printf("SKIP: %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 0; \
    } while(0)

#endif