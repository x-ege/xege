// src/backend/interface/InputBridge.h
// Input bridge interface — GLFW events → EGE message queue
#pragma once

namespace ege {

class InputBridge {
public:
    virtual ~InputBridge() {}
    virtual void init(void* nativeWindow) = 0;
    virtual void shutdown() = 0;
};

} // namespace ege
