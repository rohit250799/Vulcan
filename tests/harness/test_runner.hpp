// Purpose: No-framework test runner (what HRT/Citadel actually use)
// Why: Header-only avoids linking complexities. No constructors before main().

// tests/harness/test_runner.hpp
#ifndef TEST_RUNNER_HPP
#define TEST_RUNNER_HPP

#include <cstdio>
#include <cstring>

struct TestCase {
    const char* name;
    int (*func)();
};

inline int run_tests(TestCase* tests, int argc, char** argv) {
    int passed = 0;
    int failed = 0;
    const char* filter = (argc > 1) ? argv[1] : nullptr;

    // Count how many tests we actually have
    int test_count = 0;
    for (int i = 0; tests[i].name != nullptr; ++i) {
        test_count++;
    }

    printf("=== Running %d test(s) ===\n", test_count);

    for (int i = 0; tests[i].name != nullptr; ++i) {
        if (filter && strcmp(filter, tests[i].name) != 0) {
            continue;
        }

        printf("[ RUN    ] %s\n", tests[i].name);
        fflush(stdout);

        int result = tests[i].func();

        if (result == 0) {
            printf("[       OK ]\n");
            passed++;
        } else {
            printf("[  FAILED  ] (code: %d)\n", result);
            failed++;
        }
    }

    printf("\n========== Results ==========\n");
    printf("Passed: %d, Failed: %d\n", passed, failed);
    printf("=============================\n");

    return (failed == 0) ? 0 : 1;
}

#endif
