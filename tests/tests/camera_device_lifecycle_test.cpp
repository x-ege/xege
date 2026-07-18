#include "ege/camera_capture.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

} // namespace

int main() {
    expect(ege::hasCameraCaptureModule(), "camera capture module is enabled");

    ege::CameraCapture camera;
    expect(!camera.isOpened(), "a new capture is closed");
    expect(!camera.isStarted(), "a new capture is stopped");
    expect(!camera.start(), "starting before open fails");
    expect(!camera.grabFrame(1), "grabbing before start returns no frame");

    const auto closedResolutions = camera.getDeviceSupportedResolutions();
    expect(closedResolutions.count == 0 && closedResolutions.info == nullptr, "a closed capture has no device resolutions");

    const auto devices = camera.findDeviceNames();
    expect(devices.count >= 0, "device enumeration returns a valid count");
    expect((devices.count == 0) == (devices.info == nullptr), "device enumeration keeps count and storage consistent");

    expect(!camera.open("/dev/xege-camera-that-does-not-exist", false), "opening an invalid device fails");
    expect(!camera.isOpened(), "a failed open leaves the capture closed");
    expect(!camera.isStarted(), "a failed open leaves the capture stopped");

    if (devices.count > 0) {
        if (camera.open(0, false)) {
            expect(camera.isOpened(), "an available device reports an open state");
            expect(!camera.isStarted(), "open(..., false) does not start capture");
            camera.close();
            expect(!camera.isOpened(), "an available device closes cleanly");
        } else {
            std::cout << "SKIP: enumerated camera is unavailable or permission was denied\n";
        }
    } else {
        std::cout << "SKIP: no physical camera is available\n";
    }

    camera.stop();
    camera.close();
    camera.close();
    expect(!camera.isOpened(), "close is idempotent");
    expect(!camera.isStarted(), "the capture remains stopped after repeated close");

    if (failures != 0) {
        std::cerr << failures << " camera lifecycle assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All camera lifecycle assertions passed\n";
    return EXIT_SUCCESS;
}
