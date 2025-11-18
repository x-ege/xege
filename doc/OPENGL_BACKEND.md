# EGE OpenGL Backend

## Overview

EGE now supports an OpenGL rendering backend alongside the traditional GDI backend. This enables true cross-platform support for Windows, Linux, and macOS while maintaining full backward compatibility.

## Architecture

The new rendering architecture uses an abstraction layer with two backends:

```
┌─────────────────────────────────────────┐
│         EGE Public API (ege.h)          │
│  (initgraph, circle, line, putpixel...) │
└─────────────────┬───────────────────────┘
                  │
    ┌─────────────▼──────────────┐
    │  Renderer Abstraction Layer │
    │    (IRenderer interface)     │
    └─────┬────────────────┬───────┘
          │                │
    ┌─────▼──────┐   ┌────▼──────────┐
    │ GDI Backend│   │ OpenGL Backend │
    │  (Legacy)  │   │ (Cross-platform)│
    └────────────┘   └────────────────┘
         │                  │
    ┌────▼─────┐      ┌─────▼─────┐
    │ GDI/GDI+ │      │ GLFW+GLAD │
    │ (Windows)│      │  OpenGL   │
    └──────────┘      └───────────┘
```

## Usage

### Using the OpenGL Backend

To use the OpenGL backend, simply add the `INIT_OPENGL` flag when initializing:

```cpp
#include <graphics.h>

int main() {
    // Use OpenGL backend for cross-platform rendering
    initgraph(640, 480, INIT_OPENGL);
    
    // Draw as usual
    circle(320, 240, 100);
    
    getch();
    closegraph();
    return 0;
}
```

### Using the Traditional GDI Backend (Default)

For backward compatibility, the GDI backend remains the default:

```cpp
#include <graphics.h>

int main() {
    // Uses GDI backend by default
    initgraph(640, 480);
    
    // Or explicitly specify GDI
    initgraph(640, 480, INIT_DEFAULT);
    
    circle(320, 240, 100);
    
    getch();
    closegraph();
    return 0;
}
```

## Building with OpenGL Support

### Prerequisites

The OpenGL backend requires:
- **GLFW 3.x**: For window management
- **GLAD**: For OpenGL function loading (generated for OpenGL 3.3 Core)

These are included as submodules and automatically configured by CMake.

### Build Configuration

#### Enable OpenGL Backend (Default)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

#### Disable OpenGL Backend

If you only want the GDI backend:

```bash
mkdir build && cd build
cmake -DEGE_ENABLE_OPENGL=OFF ..
cmake --build .
```

### Generating GLAD (if needed)

GLAD files are pre-generated in `3rdparty/glad_generated/`. If you need to regenerate them:

```bash
cd 3rdparty/glad
python -m glad --api gl:core=3.3 --out-path ../glad_generated c
```

## Features

### Current Implementation (Phase 1-3)

✅ **Infrastructure**
- INIT_OPENGL flag added to initmode_flag enum
- IRenderer abstraction interface
- GDI renderer wrapper (maintains existing behavior)
- OpenGL renderer with GLFW and GLAD integration
- CMake build system integration

✅ **Basic Rendering**
- Window creation and management
- Pixel buffer operations (getPixel, setPixel, putPixels)
- Basic shapes (line, rectangle, filled rectangle)
- Clear screen
- Direct pixel buffer access

✅ **Advanced Shapes (Phase 3)**
- Circle drawing (drawCircle using Midpoint algorithm)
- Filled circle (fillCircle with horizontal line fill)
- Ellipse drawing (drawEllipse using Midpoint algorithm)
- Filled ellipse (fillEllipse with horizontal line fill)

### Planned Features (Future Phases)

🔄 **Phase 4: More Shapes & Optimization**
- Polygon drawing
- Arc drawing
- Bezier curves
- Performance optimization (batching, PBO)

🔄 **Phase 5-6: Image Operations**
- putimage/getimage support
- Image scaling and rotation
- Alpha blending and transparency
- Image filters

🔄 **Phase 7-8: Advanced Graphics**
- Anti-aliasing (MSAA)
- Text rendering (FreeType integration)
- Complex paths and gradients
- Texture brushes

🔄 **Phase 9: Input and Window Management**
- GLFW keyboard event mapping
- GLFW mouse event mapping
- Window styles and resize support
- Multi-window support

🔄 **Phase 10-11: Testing and Optimization**
- Performance optimization (PBO, texture atlases, batching)
- Compatibility testing with all 31 demos
- Memory leak checks
- Cross-platform testing (Linux, macOS)

## Cross-Platform Support

### Windows
- Both GDI and OpenGL backends available
- No additional dependencies for GDI backend
- OpenGL backend requires GLFW (bundled)

### Linux
Currently requires mingw-w64 cross-compilation. Native Linux support planned with OpenGL backend:
- GLFW provides native X11/Wayland window support
- OpenGL 3.3 Core widely supported
- All basic drawing operations work

### macOS
Native macOS support planned with OpenGL backend:
- GLFW provides native Cocoa window support
- OpenGL 3.3 Core available (deprecated but functional)
- Metal backend possible in future

## Performance Considerations

### OpenGL Backend
- **CPU-Side Operations**: getPixel/setPixel operate on a CPU buffer, synced to GPU on demand
- **GPU-Side Operations**: Future optimizations will use GPU for drawing (shaders, vertex buffers)
- **Batch Rendering**: Planned for reducing draw calls
- **Texture Atlases**: Planned for efficient text/sprite rendering

### GDI Backend
- **Performance**: Unchanged from original implementation
- **Compatibility**: 100% backward compatible

## Known Limitations

### Current Phase (1-3)
- Text rendering not yet implemented
- Image operations (putimage) not yet implemented
- No anti-aliasing or advanced effects
- Polygon and arc drawing not yet implemented
- Only tested on Windows (mingw-w64 cross-compilation)

### Platform Support
- Linux: Requires native port (in progress)
- macOS: Not yet tested
- Cross-compilation: mingw-w64 still required for now

## Testing

### Test Programs

Two test programs are provided:

1. **Basic Test**: `demo/test_opengl_backend.cpp` - Tests all basic shapes
2. **Advanced Shapes**: `demo/test_opengl_shapes.cpp` - Showcases circles and ellipses

```bash
# Build with demos enabled
mkdir build && cd build
cmake -DEGE_BUILD_DEMO=ON ..
cmake --build .

# Run the OpenGL tests
./demo/test_opengl_backend.exe
./demo/test_opengl_shapes.exe
```

### Compatibility

All existing demos should work with GDI backend (default):

```bash
cd build
cmake --build . --target demos
./demo/graph_ball  # Uses GDI backend by default
```

## Troubleshooting

### "Failed to initialize GLFW"
- Ensure your system supports OpenGL 3.3
- Update graphics drivers
- On Linux: Install libglfw3-dev

### "GLAD not found"
- Run: `cd 3rdparty/glad && python -m glad --api gl:core=3.3 --out-path ../glad_generated c`
- Or: `git submodule update --init --recursive`

### Build Errors
- Ensure C++11 or later is enabled
- Check that GLFW submodule is initialized: `git submodule update --init 3rdparty/glfw`

## Contributing

The OpenGL backend is under active development. Contributions are welcome!

### Development Priorities
1. ✅ Complete basic drawing functions (circle, ellipse) - **DONE in Phase 3**
2. Implement polygon and arc drawing
3. Implement putimage/getimage
4. Add text rendering support (FreeType)
5. Port input handling (keyboard/mouse)
6. Optimize performance (PBO, batching)
7. Test on Linux and macOS

### Code Structure
- `src/renderer/renderer_interface.h`: Abstract renderer interface
- `src/renderer/gdi_renderer.*`: GDI backend wrapper
- `src/renderer/opengl_renderer.*`: OpenGL backend implementation
- `src/renderer/renderer_factory.cpp`: Renderer creation

### Implemented Drawing Functions (Phase 3)
- ✅ `drawCircle()` - Midpoint circle algorithm
- ✅ `fillCircle()` - Horizontal line fill method
- ✅ `drawEllipse()` - Midpoint ellipse algorithm
- ✅ `fillEllipse()` - Ellipse equation-based fill

## References

- [GLFW Documentation](https://www.glfw.org/documentation.html)
- [GLAD Generator](https://glad.dav1d.de/)
- [OpenGL 3.3 Core Specification](https://www.khronos.org/registry/OpenGL/specs/gl/glspec33.core.pdf)
- [EGE API Documentation](../man/api.md)
- [Bresenham's Circle Algorithm](https://en.wikipedia.org/wiki/Midpoint_circle_algorithm)
- [Midpoint Ellipse Algorithm](https://en.wikipedia.org/wiki/Midpoint_ellipse_algorithm)

## License

The OpenGL backend follows the same MIT license as EGE.
