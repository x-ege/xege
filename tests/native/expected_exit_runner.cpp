#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "usage: expected_exit_runner <program> <expected-status>\n";
        return 2;
    }

    char* end = nullptr;
    const long expected = std::strtol(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || expected < 0 || expected > 255) {
        std::cerr << "invalid expected exit status: " << argv[2] << '\n';
        return 2;
    }

    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "fork failed: " << std::strerror(errno) << '\n';
        return 1;
    }
    if (child == 0) {
        execl(argv[1], argv[1], static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    pid_t waited = -1;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        std::cerr << "waitpid failed: " << std::strerror(errno) << '\n';
        return 1;
    }
    if (WIFEXITED(status)) {
        const int actual = WEXITSTATUS(status);
        if (actual == 77) {
            return 77;
        }
        if (actual == expected) {
            return 0;
        }
        std::cerr << "expected child exit " << expected << ", got " << actual << '\n';
        return 1;
    }
    if (WIFSIGNALED(status)) {
        std::cerr << "child terminated by signal " << WTERMSIG(status) << '\n';
    } else {
        std::cerr << "child did not exit normally\n";
    }
    return 1;
}
