# Linux native backend

## Decision

XEGE uses a small native Linux stack:

- **Xlib** owns windows, events, keyboard input, mouse input, cursors and frame presentation.
- **Cairo image surfaces** implement the `RenderTarget` drawing contract.
- **PixelSurface** remains the single CPU-authoritative premultiplied ARGB buffer used by `IMAGE`, `getbuffer()` and presentation.
- **XWayland** provides the initial Wayland compatibility path. A direct Wayland window adapter can be added later without replacing the renderer.

This deliberately avoids Qt, wxWidgets, GTK, SDL, GLFW and Pango. The only direct Linux packages required by the default backend are the distribution-provided X11 and Cairo development packages. Camera capture remains the existing optional ccap feature.

## Why this is the default

| Choice | Direct framework dependencies | Binary/deployment impact | Fit for XEGE |
| --- | --- | --- | --- |
| Xlib + Cairo | `libX11`, `libcairo` | Uses common system libraries; no GUI framework payload | Selected: smallest implementation that preserves the CPU pixel contract |
| wxWidgets | wxWidgets plus GTK on common Linux builds | Large toolkit and transitive widget stack | Rejected: duplicates XEGE's window and drawing abstractions |
| GTK + Cairo | GTK, GLib/GObject and related desktop libraries | Cairo rendering fits, but the application inherits a full GUI runtime | Rejected: unnecessary widget/runtime surface |
| SDL2 + Cairo | SDL2 plus Cairo | Portable and convenient but adds another abstraction and runtime | Rejected: XEGE already owns its platform abstraction |
| GLFW + OpenGL | GLFW, OpenGL loader/driver stack | Good optional accelerated backend, not a minimal CPU default | Retained only as a future opt-in backend |
| Direct Wayland + Cairo | Wayland client, xkbcommon and protocol generation | Lean at runtime but considerably more lifecycle/input work | Deferred until native Wayland is worth the maintenance cost |

The Linux backend adds no vendored code. Dynamic linking also prevents Cairo and X11 from being copied into XEGE's static archive or application package. Cairo's own distribution dependencies remain the operating system's responsibility.

## Architecture

1. Public EGE drawing calls resolve an `IMAGE` and its `RenderTarget`.
2. `CairoRenderTarget` draws directly into `PixelSurface`; there is no upload/download staging buffer.
3. Boolean ROP2 primitives render coverage into a reusable scratch surface, then perform exact straight-RGB operations while preserving destination alpha and restoring valid premultiplied pixels.
4. `LinuxWindow::present()` performs one row copy into an XImage and submits it with `XPutImage`.
5. Xlib events are converted to XEGE's existing Win32-compatible virtual-key, mouse and Unicode event contract.

The first version intentionally favors a small, inspectable implementation over XShm or GPU acceleration. Those can be introduced behind the existing interfaces if profiling demonstrates a need.

## Implemented surface

- Window create/show/hide/title/position/resize/topmost/borderless/cursor control.
- WM close negotiation, resize, focus, keyboard, UTF-8 text, mouse buttons, double-clicks, motion and wheel events.
- CPU presentation and headless `EGE_HEADLESS=1` rendering.
- Lines and styles, fills and patterns, rectangles, rounded rectangles, ellipses, arcs, sectors, polygons and flood fills.
- Viewports, affine transforms, ROP2 writing modes and premultiplied alpha.
- Copy/stretch/transparent/alpha/rotate/affine image transfers and blur.
- Cairo toy-font text measurement and drawing, plus narrow/wide string consistency.
- Native input box with UTF-8 value handling.

Cairo's toy-font API intentionally avoids a Pango dependency. It handles ordinary UTF-8 text but does not promise advanced script shaping or desktop font fallback. If that becomes a requirement, it should be an optional text module rather than a dependency of the native backend.

## Build

On Debian/Ubuntu:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config libcairo2-dev libx11-dev
cmake -S . -B build -G Ninja \
  -DEGE_DEFAULT_BACKEND=CAIRO \
  -DEGE_ENABLE_CAMERA_CAPTURE=OFF
cmake --build build
```

For all native tests, including the X11 integration smoke test:

```sh
sudo apt-get install xvfb
cmake -S . -B build -G Ninja \
  -DEGE_DEFAULT_BACKEND=CAIRO \
  -DEGE_BUILD_TEST=ON \
  -DEGE_ENABLE_WINDOW_TESTS=ON \
  -DEGE_ENABLE_CAMERA_CAPTURE=ON \
  -DEGE_ENABLE_CAMERA_TESTS=ON
cmake --build build
sudo install -m 666 /dev/null /dev/video99
xvfb-run -a ctest --test-dir build --output-on-failure
sudo unlink /dev/video99
```

The camera test preloads a test-only userspace V4L2 implementation for
`/dev/video99`. It exercises enumeration, format negotiation, mmap streaming,
YUYV-to-BGRA conversion and the public EGE camera/image bridge without loading a
kernel module or adding a runtime dependency. The Linux workflow also builds the
ccap CLI and runs its complete test suite against the same virtual device.

## Packaging and compatibility

- The default is dynamically linked to system Cairo and X11. Applications do not ship wxWidgets/GTK/SDL or an embedded browser/runtime.
- X11 desktops run directly. Wayland desktops run through the widely available XWayland compatibility server.
- The static `libgraphics.a` contains only XEGE code; system library code is resolved when the application links.
- `EGE_HEADLESS=1` skips the X connection while retaining the same Cairo rendering path for tests and server-side image generation.
