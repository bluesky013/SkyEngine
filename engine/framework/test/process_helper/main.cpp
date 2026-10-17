//
// Test helper for framework ProcessTest. Spawned by tests; not a gtest target.
//
// Modes:
//   echo        write "ready" to stderr, then echo stdin bytes to stdout (flushed)
//   exit <N>    exit with code N
//   sleep       sleep ~10s (for timeout/kill tests)
//

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "echo";

    if (std::strcmp(mode, "echo") == 0) {
        std::fputs("ready\n", stderr);
        std::fflush(stderr);

        int c = 0;
        while ((c = std::fgetc(stdin)) != EOF) {
            std::fputc(c, stdout);
            std::fflush(stdout);
        }
        return 0;
    }

    if (std::strcmp(mode, "exit") == 0) {
        return (argc > 2) ? std::atoi(argv[2]) : 0;
    }

    if (std::strcmp(mode, "sleep") == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }

    return 2;
}
