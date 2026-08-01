# Native backend test matrix

This matrix makes “all interfaces tested” auditable.  `Runtime` means a
deterministic executable assertion; `Compile/link` means every public header is
compiled and linked against `xege`; `Platform exclusion` is a checked build
condition, not a pass by omission.

| Area | Evidence | Current native-macOS coverage |
| --- | --- | --- |
| Public C/C++ headers (English and Chinese) | `native.public_headers_en`, `native.public_headers_zh` | Compile/link |
| CPU pixel surface and raster operation core | `native.pixel_surface`, `native.coregraphics_surface`, `native.coregraphics_render_target` | Runtime |
| Public pixel, primitive, viewport and blit APIs | `native.ege_raster_image_contract` | Runtime, exact pixel assertions |
| Save/decode image APIs | `native.ege_raster_image_contract` produces `raster-contract.png` and `.bmp`, then decodes and recognises line/shape pixels | Runtime + visual artifact |
| Text/fonts, colour conversion, deterministic RNG and compression | `native.ege_text_utility_contract` | Runtime |
| Window/options/present event bridge | `native.mac_window_smoke`, `native.ege_api_smoke` | Runtime when WindowServer is available; otherwise explicit skip (77) |
| Demo programs | `demo.*.launch` CTest instances, one per portable configured demo | Build plus native process/window-start smoke |
| CMake backend and cross toolchain contracts | `cmake.backend_default_contract`, `cmake.mingw_toolchain_contract` | Configure/build |
| Windows compatibility | MinGW configure and selected library/demo link | Build-only from macOS host |
| Camera samples | `demo.camera_base.launch`, `demo.camera_wave.launch`; `EGE_ENABLE_CAMERA_CAPTURE=ON` requires initialized `3rdparty/ccap` | Build + native-window smoke; live-frame acceptance additionally requires camera hardware and macOS permission |
| `graph_star` | Windows screensaver with Win32 preview-parent APIs | Windows-only exclusion on macOS/Linux |
| Linux native backend | Cairo selection currently fails fast because no backend is implemented | Known implementation gap, not accepted as covered |

## Acceptance commands

```sh
cmake -S . -B build/mac-tests -DEGE_DEFAULT_BACKEND=COREGRAPHICS -DEGE_BUILD_TEST=ON -DEGE_BUILD_DEMO=ON
cmake --build build/mac-tests --target demos -j4
ctest --test-dir build/mac-tests --output-on-failure
```

Artifacts are written to `build/mac-tests/test-artifacts/`.  The image contract
is an independent end-to-end check: draw a known scene, encode PNG/BMP, decode
it through EGE, and assert the characteristic line and shape pixels.
