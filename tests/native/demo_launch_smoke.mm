#import <AppKit/AppKit.h>

#include <CoreGraphics/CoreGraphics.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool hasWindowServerSession()
{
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session == nullptr) {
        return false;
    }
    CFRelease(session);
    return true;
}

bool isFrontmostApplication(pid_t pid)
{
    @autoreleasepool {
        NSRunningApplication* frontmost = NSWorkspace.sharedWorkspace.frontmostApplication;
        return frontmost != nil && frontmost.processIdentifier == pid;
    }
}

void terminateChild(pid_t pid)
{
    if (kill(pid, SIGTERM) != 0 && errno != ESRCH) {
        std::cerr << "cannot stop demo process: " << std::strerror(errno) << '\n';
    }
    int status = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (waitpid(pid, &status, WNOHANG) == pid) {
            return;
        }
        usleep(50 * 1000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: demo_launch_smoke <demo-executable>\n";
        return 2;
    }
    if (!hasWindowServerSession()) {
        std::cout << "demo smoke skipped: no WindowServer session\n";
        return 77;
    }

    int readinessPipe[2] = {-1, -1};
    if (pipe(readinessPipe) != 0) {
        std::cerr << "readiness pipe failed: " << std::strerror(errno) << '\n';
        return 1;
    }
    const std::string readinessDescriptor = std::to_string(readinessPipe[1]);

    const pid_t child = fork();
    if (child < 0) {
        close(readinessPipe[0]);
        close(readinessPipe[1]);
        std::cerr << "fork failed: " << std::strerror(errno) << '\n';
        return 1;
    }
    if (child == 0) {
        close(readinessPipe[0]);
        setenv("EGE_MACOS_TEST_NO_ACTIVATE", "1", 1);
        setenv("EGE_MACOS_TEST_READY_FD", readinessDescriptor.c_str(), 1);
        execl(argv[1], argv[1], static_cast<char*>(nullptr));
        close(readinessPipe[1]);
        _exit(127);
    }
    close(readinessPipe[1]);
    const int pipeFlags = fcntl(readinessPipe[0], F_GETFL, 0);
    if (pipeFlags < 0 || fcntl(readinessPipe[0], F_SETFL, pipeFlags | O_NONBLOCK) < 0) {
        const int pipeError = errno;
        close(readinessPipe[0]);
        terminateChild(child);
        std::cerr << "cannot make readiness pipe non-blocking: "
                  << std::strerror(pipeError) << '\n';
        return 1;
    }

    int status = 0;
    bool ready = false;
    constexpr int maxWindowPollAttempts = 30;
    constexpr useconds_t windowPollIntervalUs = 100 * 1000;
    for (int attempt = 0; attempt < maxWindowPollAttempts; ++attempt) {
        const pid_t observed = waitpid(child, &status, WNOHANG);
        if (observed == child) {
            close(readinessPipe[0]);
            std::cerr << "demo exited before presenting a native window: " << argv[1]
                      << " (status " << status << ")\n";
            return 1;
        }
        if (isFrontmostApplication(child)) {
            close(readinessPipe[0]);
            terminateChild(child);
            std::cerr << "demo activated itself and stole focus: " << argv[1] << '\n';
            return 1;
        }
        char signal = '\0';
        const ssize_t received = read(readinessPipe[0], &signal, sizeof(signal));
        if (received > 0) {
            if (isFrontmostApplication(child)) {
                close(readinessPipe[0]);
                terminateChild(child);
                std::cerr << "demo activated itself while presenting its window: "
                          << argv[1] << '\n';
                return 1;
            }
            ready = true;
            break;
        }
        if (received < 0 && errno != EAGAIN && errno != EINTR) {
            const int readError = errno;
            close(readinessPipe[0]);
            terminateChild(child);
            std::cerr << "cannot read native-window readiness: "
                      << std::strerror(readError) << '\n';
            return 1;
        }
        usleep(windowPollIntervalUs);
    }

    close(readinessPipe[0]);
    terminateChild(child);
    if (!ready) {
        std::cerr << "demo did not report native-window readiness within 3 seconds: " << argv[1] << '\n';
        return 1;
    }
    std::cout << "demo native-window smoke passed: " << argv[1] << '\n';
    return 0;
}
