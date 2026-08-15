#ifndef EGE_BACKEND_MACOS_MAC_WINDOW_H
#define EGE_BACKEND_MACOS_MAC_WINDOW_H

#include "backend/interface/Window.h"

#include <memory>
#include <string>

namespace ege
{
namespace backend
{

/**
 * AppKit window used by the native Core Graphics backend.
 *
 * AppKit objects are intentionally hidden behind a pimpl so this header stays
 * valid C++ and does not require Objective-C syntax in its consumers.
 */
class MacWindow final : public Window
{
public:
    MacWindow();
    ~MacWindow() override;

    MacWindow(const MacWindow&)            = delete;
    MacWindow& operator=(const MacWindow&) = delete;
    MacWindow(MacWindow&&)                 = delete;
    MacWindow& operator=(MacWindow&&)      = delete;

    static bool primaryScreenSize(int* width, int* height);
    static bool inputBox(const char* title, const char* prompt,
                         std::string* value);

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
    void present(const std::uint32_t* pixels, int width, int height, std::size_t strideBytes) override;
    void* getNativeHandle() const override;
    int getWidth() const override;
    int getHeight() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace backend
} // namespace ege

#endif // EGE_BACKEND_MACOS_MAC_WINDOW_H
