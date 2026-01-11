#pragma once
#include "GraphicsContext.h"

namespace ege {

class Window {
public:
    virtual ~Window() {}
    virtual bool create(int width, int height, const char* title) = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;
    virtual bool isClosed() = 0;
    virtual void processEvents() = 0;
    virtual void swapBuffers() = 0;
    virtual GraphicsContext* getGraphicsContext() = 0;
    virtual void* getNativeHandle() = 0;
    
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};

}
