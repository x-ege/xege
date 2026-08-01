#include <CoreGraphics/CoreGraphics.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>

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

bool hasVisibleWindow(pid_t pid)
{
    // `NSWindow` can be ordered front before AppKit has completed the first
    // compositor turn.  Include all server-side windows here; the layer-0
    // requirement below still rules out helper/menu windows.
    CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
    if (windows == nullptr) {
        return false;
    }
    bool found = false;
    for (CFIndex index = 0; index < CFArrayGetCount(windows); ++index) {
        const auto* info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, index));
        const auto* owner = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowOwnerPID));
        const auto* layer = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowLayer));
        if (owner == nullptr || layer == nullptr) {
            continue;
        }
        int ownerPid = 0;
        int windowLayer = 1;
        CFNumberGetValue(owner, kCFNumberIntType, &ownerPid);
        CFNumberGetValue(layer, kCFNumberIntType, &windowLayer);
        if (ownerPid == pid && windowLayer == 0) {
            found = true;
            break;
        }
    }
    CFRelease(windows);
    return found;
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
    bool visible = false;
    for (int attempt = 0; attempt < 30; ++attempt) {
        const pid_t observed = waitpid(child, &status, WNOHANG);
        if (observed == child) {
            std::cerr << "demo exited before presenting a native window: " << argv[1]
                      << " (status " << status << ")\n";
            return 1;
        }
        if (hasVisibleWindow(child)) {
            visible = true;
            break;
        }
        usleep(100 * 1000);
    }

    terminateChild(child);
    if (!visible) {
        std::cerr << "demo did not present a visible native window within 3 seconds: " << argv[1] << '\n';
        return 1;
    }
    std::cout << "demo native-window smoke passed: " << argv[1] << '\n';
    return 0;
}
