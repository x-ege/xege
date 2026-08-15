# Native backend test matrix

This matrix makes the implemented native surface auditable without claiming
that every legacy Win32-compatible API has a macOS equivalent. `Runtime` means a
deterministic executable assertion; `Compile/link` means every public header is
compiled and linked against `xege`; `Platform exclusion` is a checked build
condition, not a pass by omission.

| Area | Evidence | Current native-macOS coverage |
| --- | --- | --- |
| Public C/C++ headers (English and Chinese) | `native.public_headers_en`, `native.public_headers_zh` | Compile/link |
| CPU pixel surface and raster operation core | `native.pixel_surface`, `native.coregraphics_surface`, `native.coregraphics_render_target` | Runtime |
| Public pixel, primitive, viewport and blit APIs | `native.ege_raster_image_contract` | Runtime, exact pixel assertions |
| Enhanced GDI+-compatible API (curves, paths, transforms, gradients and textures) | `native.ege_enhanced_api_contract` calls every public enhanced declaration and overload | Compile/link plus deterministic geometry, pixel, invalid-input and lifetime assertions |
| Save/decode image APIs | `native.ege_raster_image_contract` produces `raster-contract.png` and `.bmp`, then decodes and recognises line/shape pixels | Runtime + visual artifact |
| Text/fonts, colour conversion, deterministic RNG and compression | `native.ege_text_utility_contract` | Runtime |
| Control-tree focus lifetime | `native.ege_control_focus_contract` | Runtime detach, reparent and intermediate-destruction assertions |
| MUSIC file/error lifecycle | `native.ege_music_contract` | Runtime with generated silent WAV plus missing/corrupt inputs; no audible playback or hardware-output assertion |
| Process teardown | `native.ege_process_exit_contract` | Runtime subprocess verifies EGE preserves the application's non-zero return code |
| Global canvas and public API | `native.ege_api_smoke` produces `ege-api-headless.png` | Runtime without NSApplication or NSWindow; saved image is decoded and checked pixel-by-pixel |
| Native window/options/event bridge | `native.mac_window_smoke` covers sizing/styles, presentation lifetime, keyboard/text, left/right modifiers, double-click/X buttons, and backend close veto | Visible manual opt-in only with `EGE_ENABLE_WINDOW_TESTS=ON`; absent from the default CTest suite |
| Public Objective-C++ header interop | `native.public_objcxx_headers` compiles EGE/AppKit headers in both include orders | Headless; part of the default CTest suite |
| Public close adapter | `native.public_close_callback_contract` verifies the public `SetCloseHandler` notification in a sentinel child process | Visible manual opt-in only; the sentinel detects accidental `exit(0)` during teardown |
| Demo programs | `demos` build target; optional `demo.*.launch` tests | Build-only by default; visible launches require `EGE_ENABLE_WINDOW_TESTS=ON` |
| CMake backend, cross toolchain and macOS release contracts | `cmake.backend_default_contract`, `cmake.mingw_toolchain_contract`, `cmake.macos_release_contract` | Configure/build plus static release-path and macOS 11.0 deployment-target assertions |
| macOS prebuilt SDK | `Release Package` and `macOS Native CoreGraphics Build` workflows | Universal `arm64`/`x86_64` archive validation and all-demo link against the packaged CMake configuration |
| Windows compatibility | `cmake.mingw_toolchain_contract` plus Windows/Linux workflows | The local contract validates toolchain isolation and only configures when a compiler exists; actual library/demo builds belong to Windows or Linux-MinGW CI |
| Camera samples | `camera_base`, `camera_wave`; enabling camera capture requires the `3rdparty/ccap` sources to already be present (normally via a recursive submodule checkout; CMake never initializes the submodule) | Compile/link in the `demos` target; live camera startup is an explicit hardware test so ordinary CTest does not access devices or show permission UI |
| `graph_star` | Windows screensaver with Win32 preview-parent APIs | Windows-only exclusion on macOS/Linux |
| Linux native backend | Cairo selection currently fails fast because no backend is implemented | Known implementation gap, not accepted as covered |

## Acceptance commands

```sh
git submodule update --init --recursive 3rdparty/ccap
cmake -S . -B build/mac-tests \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DEGE_DEFAULT_BACKEND=COREGRAPHICS \
  -DEGE_ENABLE_OPENGL=OFF \
  -DEGE_ENABLE_CAMERA_CAPTURE=ON \
  -DEGE_BUILD_TEST=ON \
  -DEGE_BUILD_DEMO=ON \
  -DEGE_BUILD_TEMP=OFF \
  -DEGE_ENABLE_WINDOW_TESTS=OFF
cmake --build build/mac-tests --parallel 4
cmake --build build/mac-tests --target demos --parallel 4
ctest --test-dir build/mac-tests --output-on-failure
```

Artifacts are written to `build/mac-tests/test-artifacts/`. Both the offscreen
raster contract and the `initgraph` global-canvas contract render without a
window, encode their result, decode it through EGE, and assert characteristic
pixels. Default test commands must never enable `EGE_ENABLE_WINDOW_TESTS`.

Known exclusions are part of the contract: `sys_edit`, Win32 handles/resources,
`graph_star`, live camera permission/device behavior, audible output, Intel-Mac
runtime, and a native Linux backend are not covered by the default native suite.
