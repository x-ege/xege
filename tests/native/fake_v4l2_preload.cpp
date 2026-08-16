#include <dlfcn.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
constexpr std::uint32_t kWidth = 640;
constexpr std::uint32_t kHeight = 480;
constexpr std::size_t kFrameBytes = kWidth * kHeight * 2;
constexpr unsigned int kBufferCount = 4;

std::mutex stateMutex;
std::unordered_set<int> cameraFds;
std::unordered_map<unsigned int, unsigned char*> buffers;
unsigned int nextBuffer = 0;

template <typename Function>
Function nextSymbol(const char* name)
{
    return reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
}

const char* virtualPath()
{
    const char* path = std::getenv("EGE_TEST_V4L2_PATH");
    return path && *path ? path : "/dev/video99";
}

bool isCameraFd(int fd)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return cameraFds.count(fd) != 0;
}

void fillFormat(v4l2_format* format)
{
    format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format->fmt.pix.width = kWidth;
    format->fmt.pix.height = kHeight;
    format->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format->fmt.pix.field = V4L2_FIELD_NONE;
    format->fmt.pix.bytesperline = kWidth * 2;
    format->fmt.pix.sizeimage = kFrameBytes;
    format->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
}

void fillFrame(unsigned char* output)
{
    for (std::uint32_t y = 0; y < kHeight; ++y) {
        for (std::uint32_t x = 0; x < kWidth; x += 2) {
            const bool right = x >= kWidth / 2;
            const bool bottom = y >= kHeight / 2;
            const unsigned char luminance = bottom
                ? static_cast<unsigned char>(right ? 224 : 160)
                : static_cast<unsigned char>(right ? 96 : 32);
            const std::size_t offset = (static_cast<std::size_t>(y) * kWidth + x) * 2;
            output[offset + 0] = luminance;
            output[offset + 1] = 128;
            output[offset + 2] = luminance;
            output[offset + 3] = 128;
        }
    }
}

}

extern "C" int open(const char* path, int flags, ...)
{
    using Function = int (*)(const char*, int, ...);
    Function realOpen = nextSymbol<Function>("open");
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = static_cast<mode_t>(va_arg(arguments, int));
        va_end(arguments);
    }

    int fd = std::strcmp(path, virtualPath()) == 0
        ? realOpen("/dev/null", O_RDWR | O_NONBLOCK)
        : ((flags & O_CREAT) ? realOpen(path, flags, mode) : realOpen(path, flags));
    if (fd >= 0 && std::strcmp(path, virtualPath()) == 0) {
        std::lock_guard<std::mutex> lock(stateMutex);
        cameraFds.insert(fd);
    }
    return fd;
}

extern "C" int open64(const char* path, int flags, ...)
{
    using Function = int (*)(const char*, int, ...);
    Function realOpen = nextSymbol<Function>("open64");
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = static_cast<mode_t>(va_arg(arguments, int));
        va_end(arguments);
    }

    int fd = std::strcmp(path, virtualPath()) == 0
        ? realOpen("/dev/null", O_RDWR | O_NONBLOCK)
        : ((flags & O_CREAT) ? realOpen(path, flags, mode) : realOpen(path, flags));
    if (fd >= 0 && std::strcmp(path, virtualPath()) == 0) {
        std::lock_guard<std::mutex> lock(stateMutex);
        cameraFds.insert(fd);
    }
    return fd;
}

extern "C" int close(int fd)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        cameraFds.erase(fd);
    }
    using Function = int (*)(int);
    return nextSymbol<Function>("close")(fd);
}

extern "C" int ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    void* argument = va_arg(arguments, void*);
    va_end(arguments);

    if (!isCameraFd(fd)) {
        using Function = int (*)(int, unsigned long, ...);
        return nextSymbol<Function>("ioctl")(fd, request, argument);
    }

    switch (request) {
    case VIDIOC_QUERYCAP: {
        auto* capability = static_cast<v4l2_capability*>(argument);
        std::memset(capability, 0, sizeof(*capability));
        std::strncpy(reinterpret_cast<char*>(capability->driver), "ege-test", sizeof(capability->driver) - 1);
        std::strncpy(reinterpret_cast<char*>(capability->card), "EGE Virtual Camera", sizeof(capability->card) - 1);
        capability->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
        capability->device_caps = capability->capabilities;
        return 0;
    }
    case VIDIOC_ENUM_FMT: {
        auto* format = static_cast<v4l2_fmtdesc*>(argument);
        if (format->index != 0) { errno = EINVAL; return -1; }
        format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format->pixelformat = V4L2_PIX_FMT_YUYV;
        std::strncpy(reinterpret_cast<char*>(format->description), "YUYV", sizeof(format->description) - 1);
        return 0;
    }
    case VIDIOC_ENUM_FRAMESIZES: {
        auto* size = static_cast<v4l2_frmsizeenum*>(argument);
        if (size->index != 0 || size->pixel_format != V4L2_PIX_FMT_YUYV) {
            errno = EINVAL;
            return -1;
        }
        size->type = V4L2_FRMSIZE_TYPE_DISCRETE;
        size->discrete.width = kWidth;
        size->discrete.height = kHeight;
        return 0;
    }
    case VIDIOC_G_FMT:
    case VIDIOC_S_FMT:
        fillFormat(static_cast<v4l2_format*>(argument));
        return 0;
    case VIDIOC_REQBUFS: {
        auto* requestBuffers = static_cast<v4l2_requestbuffers*>(argument);
        if (requestBuffers->count != 0) requestBuffers->count = kBufferCount;
        return 0;
    }
    case VIDIOC_QUERYBUF: {
        auto* buffer = static_cast<v4l2_buffer*>(argument);
        if (buffer->index >= kBufferCount) { errno = EINVAL; return -1; }
        buffer->length = kFrameBytes;
        buffer->m.offset = buffer->index * kFrameBytes;
        return 0;
    }
    case VIDIOC_QBUF:
    case VIDIOC_STREAMON:
    case VIDIOC_STREAMOFF:
        return 0;
    case VIDIOC_DQBUF: {
        auto* buffer = static_cast<v4l2_buffer*>(argument);
        std::lock_guard<std::mutex> lock(stateMutex);
        buffer->index = nextBuffer++ % kBufferCount;
        buffer->bytesused = kFrameBytes;
        auto found = buffers.find(buffer->index);
        if (found == buffers.end()) { errno = EIO; return -1; }
        fillFrame(found->second);
        return 0;
    }
    default:
        errno = EINVAL;
        return -1;
    }
}

extern "C" void* mmap(void* address, size_t length, int protection, int flags, int fd, off_t offset)
{
    using Function = void* (*)(void*, size_t, int, int, int, off_t);
    Function realMmap = nextSymbol<Function>("mmap");
    if (!isCameraFd(fd)) return realMmap(address, length, protection, flags, fd, offset);

    void* result = realMmap(address, length, protection,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result != MAP_FAILED) {
        std::lock_guard<std::mutex> lock(stateMutex);
        buffers[static_cast<unsigned int>(offset / kFrameBytes)] = static_cast<unsigned char*>(result);
    }
    return result;
}

extern "C" int munmap(void* address, size_t length)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (auto iterator = buffers.begin(); iterator != buffers.end(); ++iterator) {
            if (iterator->second == address) {
                buffers.erase(iterator);
                break;
            }
        }
    }
    using Function = int (*)(void*, size_t);
    return nextSymbol<Function>("munmap")(address, length);
}

extern "C" int poll(pollfd* fileDescriptors, nfds_t count, int timeout)
{
    if (count == 1 && isCameraFd(fileDescriptors[0].fd)) {
        timespec delay{0, 5 * 1000 * 1000};
        nanosleep(&delay, nullptr);
        fileDescriptors[0].revents = POLLIN;
        return 1;
    }
    using Function = int (*)(pollfd*, nfds_t, int);
    return nextSymbol<Function>("poll")(fileDescriptors, count, timeout);
}
