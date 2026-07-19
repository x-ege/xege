#include "GLFWWindow.h"
#include "glad/gl.h"
#include "../../ege_head.h"
#include "../../ege_graph.h"
#include "../../window.h"
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ege {

namespace {

DWORD eventTimeMilliseconds() {
#ifdef _WIN32
    // Keyboard compatibility code compares queued event timestamps with
    // GetTickCount(), so Windows OpenGL events must use the same clock.
    return GetTickCount();
#else
    return static_cast<DWORD>(glfwGetTime() * 1000.0);
#endif
}

int glfwKeyToEgeKey(int key) {
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return key_0 + key - GLFW_KEY_0;
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return key_A + key - GLFW_KEY_A;
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) return key_f1 + key - GLFW_KEY_F1;
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return key_num0 + key - GLFW_KEY_KP_0;
    switch (key) {
    case GLFW_KEY_BACKSPACE:    return key_back;
    case GLFW_KEY_TAB:          return key_tab;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:     return key_enter;
    case GLFW_KEY_ESCAPE:       return key_esc;
    case GLFW_KEY_SPACE:        return key_space;
    case GLFW_KEY_PAGE_UP:      return key_pageup;
    case GLFW_KEY_PAGE_DOWN:    return key_pagedown;
    case GLFW_KEY_END:          return key_end;
    case GLFW_KEY_HOME:         return key_home;
    case GLFW_KEY_LEFT:         return key_left;
    case GLFW_KEY_UP:           return key_up;
    case GLFW_KEY_RIGHT:        return key_right;
    case GLFW_KEY_DOWN:         return key_down;
    case GLFW_KEY_PRINT_SCREEN: return key_snapshot;
    case GLFW_KEY_INSERT:       return key_insert;
    case GLFW_KEY_DELETE:       return key_delete;
    case GLFW_KEY_KP_MULTIPLY:  return key_multiply;
    case GLFW_KEY_KP_ADD:       return key_add;
    case GLFW_KEY_KP_SUBTRACT:  return key_subtract;
    case GLFW_KEY_KP_DECIMAL:   return key_decimal;
    case GLFW_KEY_KP_DIVIDE:    return key_divide;
    case GLFW_KEY_NUM_LOCK:     return key_numlock;
    case GLFW_KEY_SCROLL_LOCK:  return key_scrolllock;
    case GLFW_KEY_PAUSE:        return key_pause;
    case GLFW_KEY_CAPS_LOCK:    return key_capslock;
    case GLFW_KEY_LEFT_SHIFT:   return key_shift_l;
    case GLFW_KEY_RIGHT_SHIFT:  return key_shift_r;
    case GLFW_KEY_LEFT_CONTROL: return key_control_l;
    case GLFW_KEY_RIGHT_CONTROL:return key_control_r;
    case GLFW_KEY_LEFT_ALT:     return key_menu_l;
    case GLFW_KEY_RIGHT_ALT:    return key_menu_r;
    case GLFW_KEY_LEFT_SUPER:   return key_win_l;
    case GLFW_KEY_RIGHT_SUPER:  return key_win_r;
    case GLFW_KEY_SEMICOLON:    return key_semicolon;
    case GLFW_KEY_EQUAL:
    case GLFW_KEY_KP_EQUAL:     return key_plus;
    case GLFW_KEY_COMMA:        return key_comma;
    case GLFW_KEY_MINUS:        return key_minus;
    case GLFW_KEY_PERIOD:       return key_period;
    case GLFW_KEY_SLASH:        return key_slash;
    case GLFW_KEY_GRAVE_ACCENT: return key_tilde;
    case GLFW_KEY_LEFT_BRACKET: return key_lbrace;
    case GLFW_KEY_BACKSLASH:    return key_backslash;
    case GLFW_KEY_RIGHT_BRACKET:return key_rbrace;
    case GLFW_KEY_APOSTROPHE:   return key_quote;
    default:                    return 0;
    }
}

void refreshAggregateModifierState(_graph_setting* pg) {
    pg->keystatemap[key_shift] = pg->keystatemap[key_shift_l] || pg->keystatemap[key_shift_r];
    pg->keystatemap[key_control] = pg->keystatemap[key_control_l] || pg->keystatemap[key_control_r];
    pg->keystatemap[key_menu] = pg->keystatemap[key_menu_l] || pg->keystatemap[key_menu_r];
}

WPARAM currentMouseFlags(const _graph_setting* pg, int mods) {
    WPARAM flags = 0;
    if (pg->keystatemap[key_mouse_l]) flags |= MK_LBUTTON;
    if (pg->keystatemap[key_mouse_r]) flags |= MK_RBUTTON;
    if (pg->keystatemap[key_mouse_m]) flags |= MK_MBUTTON;
    if (pg->keystatemap[key_mouse_x1]) flags |= MK_XBUTTON1;
    if (pg->keystatemap[key_mouse_x2]) flags |= MK_XBUTTON2;
    if (mods & GLFW_MOD_SHIFT) flags |= MK_SHIFT;
    if (mods & GLFW_MOD_CONTROL) flags |= MK_CONTROL;
    return flags;
}

void enqueueKeyMessage(_graph_setting* pg, UINT message, WPARAM key, LPARAM flags) {
    if (!pg->msgkey_queue) return;
    EGEMSG event = {};
    event.hwnd = pg->hwnd;
    event.message = message;
    event.wParam = key;
    event.lParam = flags;
    event.time = eventTimeMilliseconds();
    pg->msgkey_queue->push(event);
}

void enqueueMouseMessage(_graph_setting* pg, UINT message, WPARAM flags,
                         int x, int y, unsigned int callbackFlags) {
    if (pg->callback_mouse &&
        pg->callback_mouse(pg->callback_mouse_param, message, x, y,
                           static_cast<int>(callbackFlags)) == 0) {
        return;
    }
    if (!pg->msgmouse_queue) return;
    EGEMSG event = {};
    event.hwnd = pg->hwnd;
    event.message = message;
    event.wParam = flags;
    event.lParam = MAKELPARAM(x, y);
    event.time = eventTimeMilliseconds();
    pg->msgmouse_queue->push(event);
}

unsigned int callbackMouseFlags(const _graph_setting* pg, int mods) {
    unsigned int flags = 0;
    if (pg->keystatemap[key_mouse_l]) flags |= mouse_flag_left;
    if (pg->keystatemap[key_mouse_r]) flags |= mouse_flag_right;
    if (pg->keystatemap[key_mouse_m]) flags |= mouse_flag_mid;
    if (pg->keystatemap[key_mouse_x1]) flags |= mouse_flag_x1;
    if (pg->keystatemap[key_mouse_x2]) flags |= mouse_flag_x2;
    if (mods & GLFW_MOD_SHIFT) flags |= mouse_flag_shift;
    if (mods & GLFW_MOD_CONTROL) flags |= mouse_flag_ctrl;
    return flags;
}

} // anonymous namespace

GLFWWindow::GLFWWindow()
    : m_window(NULL), m_renderTarget(NULL), m_width(0), m_height(0),
      m_framebufferWidth(0), m_framebufferHeight(0) {
    std::fill(m_lastClickTime, m_lastClickTime + 5, 0.0);
    std::fill(m_lastClickX, m_lastClickX + 5, 0);
    std::fill(m_lastClickY, m_lastClickY + 5, 0);
}

GLFWWindow::~GLFWWindow() {
    close();
}

bool GLFWWindow::create(int width, int height, const char* title) {
    if (!glfwInit()) {
        const char* desc = NULL;
        int err = glfwGetError(&desc);
        if (desc) {
            fprintf(stderr, "Failed to initialize GLFW (%d): %s\n", err, desc);
        } else {
            fprintf(stderr, "Failed to initialize GLFW\n");
        }
        return false;
    }

    const int initMode = static_cast<int>(getinitmode());

    // Request OpenGL 3.3 Core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Configure all observable EGE window attributes before creation. Windows
    // start hidden so INIT_HIDE never flashes a visible native window; the
    // common initialization path shows it after all state has been applied.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, (initMode & INIT_NOBORDER) ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, (initMode & INIT_TOPMOST) ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifdef GLFW_SCALE_FRAMEBUFFER
    // EGE's public width/height APIs use logical drawing coordinates. Ask GLFW
    // for a one-pixel-per-coordinate drawable so macOS and Wayland match the
    // legacy Win32 behavior even on HiDPI displays.
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_FALSE);
#endif
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    // Compatibility spelling used by GLFW 3.3.
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
    // Use single-buffered context to avoid swap issues
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
#endif

    m_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!m_window) {
        const char* desc = NULL;
        int err = glfwGetError(&desc);
        if (desc) {
            fprintf(stderr, "Failed to create GLFW window (%d): %s\n", err, desc);
        } else {
            fprintf(stderr, "Failed to create GLFW window\n");
        }
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetCharCallback(m_window, charCallback);
    glfwSetCursorPosCallback(m_window, cursorPositionCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);
    glfwSetWindowSizeCallback(m_window, windowSizeCallback);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

    // Load OpenGL functions with GLAD
    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    glfwGetWindowSize(m_window, &m_width, &m_height);
    if (m_width <= 0 || m_height <= 0) {
        m_width = width;
        m_height = height;
    }
    glfwGetFramebufferSize(m_window, &m_framebufferWidth, &m_framebufferHeight);

    // Create screen render target
    m_renderTarget = new GlRenderTarget();
    if (!m_renderTarget->initOnScreen(m_width, m_height)) {
        fprintf(stderr, "Failed to initialize screen RenderTarget\n");
        delete m_renderTarget;
        m_renderTarget = NULL;
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Setup basic GL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void GLFWWindow::show() {
    if (m_window) glfwShowWindow(m_window);
}

void GLFWWindow::hide() {
    if (m_window) glfwHideWindow(m_window);
}

void GLFWWindow::setTitle(const char* title) {
    if (m_window) glfwSetWindowTitle(m_window, title ? title : "");
}

void GLFWWindow::setSize(int width, int height) {
    if (!m_window || width <= 0 || height <= 0) return;
    resizeRenderTarget(width, height);
    glfwSetWindowSize(m_window, width, height);
    m_width = width;
    m_height = height;
}

void GLFWWindow::setPosition(int x, int y) {
    if (!m_window) return;
#ifdef _WIN32
    // EGE's Win32-compatible coordinates refer to the outer window origin,
    // while GLFW positions the upper-left corner of the content area.
    int leftFrame = 0;
    int topFrame = 0;
    glfwGetWindowFrameSize(m_window, &leftFrame, &topFrame, NULL, NULL);
    glfwSetWindowPos(m_window, x + leftFrame, y + topFrame);
#else
    glfwSetWindowPos(m_window, x, y);
#endif
}

void GLFWWindow::setCursorVisible(bool visible) {
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

void GLFWWindow::resizeRenderTarget(int width, int height) {
    if (!m_renderTarget || width <= 0 || height <= 0 ||
        (m_renderTarget->getWidth() == width && m_renderTarget->getHeight() == height)) {
        return;
    }
    m_renderTarget->setViewport(0, 0, width, height, false);
    m_renderTarget->rebuild(width, height);
}

void GLFWWindow::close() {
    if (m_renderTarget) {
        delete m_renderTarget;
        m_renderTarget = NULL;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = NULL;
        glfwTerminate();
    }
}

void GLFWWindow::keyCallback(GLFWwindow* window, int key, int, int action, int mods) {
    const bool requestsQuit = action == GLFW_PRESS && key == GLFW_KEY_Q &&
                              (mods & GLFW_MOD_SUPER) != 0;
    const int egeKey = glfwKeyToEgeKey(key);
    if (egeKey <= 0 || egeKey >= MAX_KEY_VCODE) {
        if (requestsQuit) windowCloseCallback(window);
        return;
    }

    _graph_setting* pg = &graph_setting;
    const bool pressed = action != GLFW_RELEASE;
    pg->keystatemap[egeKey] = pressed;
    refreshAggregateModifierState(pg);

    if (action == GLFW_PRESS) {
        if (pg->key_press_count[egeKey] < UINT16_MAX) ++pg->key_press_count[egeKey];
    } else if (action == GLFW_REPEAT) {
        if (pg->key_repeat_count[egeKey] < UINT16_MAX) ++pg->key_repeat_count[egeKey];
    } else if (action == GLFW_RELEASE) {
        if (pg->key_release_count[egeKey] < UINT16_MAX) ++pg->key_release_count[egeKey];
    }

    const unsigned int callbackMessage = action == GLFW_RELEASE ? 0U : 1U;
    if (pg->callback_key &&
        pg->callback_key(pg->callback_key_param, callbackMessage, egeKey) == 0) {
        if (requestsQuit) windowCloseCallback(window);
        return;
    }
    const LPARAM repeatFlag = action == GLFW_REPEAT ? static_cast<LPARAM>(0x40000000) : 0;
    enqueueKeyMessage(pg, action == GLFW_RELEASE ? WM_KEYUP : WM_KEYDOWN,
                      static_cast<WPARAM>(egeKey), repeatFlag);
    if (requestsQuit) windowCloseCallback(window);
}

void GLFWWindow::charCallback(GLFWwindow*, unsigned int codepoint) {
    if (codepoint > 0x10FFFFU || (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) return;
    _graph_setting* pg = &graph_setting;
    const auto emitUnit = [&](unsigned int unit) {
        if (pg->callback_key && pg->callback_key(pg->callback_key_param, 2U, static_cast<int>(unit)) == 0) {
            return;
        }
        enqueueKeyMessage(pg, WM_CHAR, static_cast<WPARAM>(unit), 0);
    };
    if (codepoint <= 0xFFFFU) {
        emitUnit(codepoint);
    } else {
        codepoint -= 0x10000U;
        emitUnit(0xD800U + (codepoint >> 10));
        emitUnit(0xDC00U + (codepoint & 0x3FFU));
    }
}

void GLFWWindow::cursorPositionCallback(GLFWwindow*, double x, double y) {
    _graph_setting* pg = &graph_setting;
    const int cursorX = static_cast<int>(std::lround(x));
    const int cursorY = static_cast<int>(std::lround(y));
    pg->mouse_pos = Point(cursorX, cursorY);
    const int mods = (pg->keystatemap[key_shift] ? GLFW_MOD_SHIFT : 0) |
                     (pg->keystatemap[key_control] ? GLFW_MOD_CONTROL : 0);
    enqueueMouseMessage(pg, WM_MOUSEMOVE, currentMouseFlags(pg, mods),
                        cursorX, cursorY, callbackMouseFlags(pg, mods));
}

void GLFWWindow::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    int egeKey = 0;
    UINT downMessage = 0, upMessage = 0, doubleMessage = 0;
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        egeKey = key_mouse_l; downMessage = WM_LBUTTONDOWN;
        upMessage = WM_LBUTTONUP; doubleMessage = WM_LBUTTONDBLCLK; break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        egeKey = key_mouse_r; downMessage = WM_RBUTTONDOWN;
        upMessage = WM_RBUTTONUP; doubleMessage = WM_RBUTTONDBLCLK; break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        egeKey = key_mouse_m; downMessage = WM_MBUTTONDOWN;
        upMessage = WM_MBUTTONUP; doubleMessage = WM_MBUTTONDBLCLK; break;
    case GLFW_MOUSE_BUTTON_4:
        egeKey = key_mouse_x1; downMessage = WM_XBUTTONDOWN;
        upMessage = WM_XBUTTONUP; doubleMessage = WM_XBUTTONDBLCLK; break;
    case GLFW_MOUSE_BUTTON_5:
        egeKey = key_mouse_x2; downMessage = WM_XBUTTONDOWN;
        upMessage = WM_XBUTTONUP; doubleMessage = WM_XBUTTONDBLCLK; break;
    default: return;
    }

    _graph_setting* pg = &graph_setting;
    const bool pressed = action == GLFW_PRESS;
    pg->keystatemap[egeKey] = pressed;
    uint16_t* counter = pressed ? pg->key_press_count : pg->key_release_count;
    if (counter[egeKey] < UINT16_MAX) ++counter[egeKey];

    GLFWWindow* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    bool isDoubleClick = false;
    if (pressed && self && button >= 0 && button < 5) {
        const double now = glfwGetTime();
        isDoubleClick = self->m_lastClickTime[button] > 0.0 &&
                        now - self->m_lastClickTime[button] <= 0.5 &&
                        std::abs(pg->mouse_pos.x - self->m_lastClickX[button]) <= 4 &&
                        std::abs(pg->mouse_pos.y - self->m_lastClickY[button]) <= 4;
        self->m_lastClickTime[button] = isDoubleClick ? 0.0 : now;
        self->m_lastClickX[button] = pg->mouse_pos.x;
        self->m_lastClickY[button] = pg->mouse_pos.y;
    }

    WPARAM flags = currentMouseFlags(pg, mods);
    if (button == GLFW_MOUSE_BUTTON_4) flags |= static_cast<WPARAM>(XBUTTON1) << 16;
    if (button == GLFW_MOUSE_BUTTON_5) flags |= static_cast<WPARAM>(XBUTTON2) << 16;
    unsigned int callbackFlags = callbackMouseFlags(pg, mods);
    if (isDoubleClick) callbackFlags |= mouse_flag_doubleclick;
    enqueueMouseMessage(pg, pressed ? (isDoubleClick ? doubleMessage : downMessage) : upMessage,
                        flags, pg->mouse_pos.x, pg->mouse_pos.y, callbackFlags);
}

void GLFWWindow::scrollCallback(GLFWwindow*, double, double yOffset) {
    _graph_setting* pg = &graph_setting;
    const int mods = (pg->keystatemap[key_shift] ? GLFW_MOD_SHIFT : 0) |
                     (pg->keystatemap[key_control] ? GLFW_MOD_CONTROL : 0);
    const int delta = std::max(-32768, std::min(32767,
        static_cast<int>(std::lround(yOffset * 120.0))));
    WPARAM flags = currentMouseFlags(pg, mods) |
                   (static_cast<WPARAM>(static_cast<uint16_t>(delta)) << 16);
    enqueueMouseMessage(pg, WM_MOUSEWHEEL, flags, pg->mouse_pos.x, pg->mouse_pos.y,
                        callbackMouseFlags(pg, mods));
}

void GLFWWindow::windowCloseCallback(GLFWwindow* window) {
    _graph_setting* pg = &graph_setting;
    if (pg->callback_close) {
        pg->callback_close();
        // Win32 only destroys the window when WM_CLOSE is left unhandled.
        // A registered EGE handler therefore cancels GLFW's default flag.
        glfwSetWindowShouldClose(window, GLFW_FALSE);
    } else {
        pg->exit_window = 1;
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void GLFWWindow::windowSizeCallback(GLFWwindow* window, int width, int height) {
    if (width <= 0 || height <= 0) return;
    GLFWWindow* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_width = width;
        self->m_height = height;
        self->resizeRenderTarget(width, height);
    }
    resize_window_surface(width, height);
}

void GLFWWindow::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if (width <= 0 || height <= 0) return;
    GLFWWindow* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_framebufferWidth = width;
        self->m_framebufferHeight = height;
    }
}

bool GLFWWindow::isClosed() {
    return !m_window || glfwWindowShouldClose(m_window);
}

void GLFWWindow::processEvents() {
    glfwPollEvents();
}

void GLFWWindow::swapBuffers() {
    if (m_window) {
        if (m_renderTarget) {
            m_renderTarget->flush();
            m_renderTarget->captureScreenToTexture();
        }
        glfwSwapBuffers(m_window);
    }
}

GraphicsContext* GLFWWindow::getGraphicsContext() {
    // Return the GlRenderTarget as a GraphicsContext for backward compatibility.
    // GlRenderTarget inherits from RenderTarget, not GraphicsContext.
    // This returns NULL — callers should use getRenderTarget() instead.
    // The drawing functions in egegapi.cpp are updated to use m_renderTarget.
    return NULL;
}

void* GLFWWindow::getNativeHandle() {
#ifdef _WIN32
    // getHWnd() is a long-standing public Win32 API. Keep returning a real
    // HWND even when GLFW owns the opt-in OpenGL window.
    return m_window ? static_cast<void*>(glfwGetWin32Window(m_window)) : NULL;
#else
    return (void*)m_window;
#endif
}

int GLFWWindow::getWidth() const {
    return m_width;
}

int GLFWWindow::getHeight() const {
    return m_height;
}

} // namespace ege
