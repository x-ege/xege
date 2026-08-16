#ifndef EGE_BACKEND_LINUX_LINUX_WINDOW_H
#define EGE_BACKEND_LINUX_LINUX_WINDOW_H

#include "backend/interface/Window.h"

#include <memory>
#include <string>

namespace ege
{
namespace backend
{

/** A small Xlib window used by the native Cairo backend. */
class LinuxWindow final : public Window
{
public:
    LinuxWindow();
    ~LinuxWindow() override;

    LinuxWindow(const LinuxWindow&) = delete;
    LinuxWindow& operator=(const LinuxWindow&) = delete;

    static bool primaryScreenSize(int* width, int* height);
    static bool inputBox(const char* title, const char* prompt, std::string* value);

    bool create(int width, int height, const char* title,
        const WindowOptions& options, WindowEventSink* eventSink) override;
    void show() override;
    void hide() override;
    void setTitle(const char* title) override;
    void setSize(int width, int height) override;
    void setPosition(int x, int y) override;
    void setCursorVisible(bool visible) override;
    void close() override;
    bool isClosed() const override;
    void processEvents() override;
    void present(const std::uint32_t* pixels, int width, int height,
        std::size_t strideBytes) override;
    void* getNativeHandle() const override;
    int getWidth() const override;
    int getHeight() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace backend
} // namespace ege

#endif
