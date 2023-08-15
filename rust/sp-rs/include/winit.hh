#pragma once
// #include "rust/cxx.h"

namespace sp {
    class WinitInputHandler;
    enum KeyCode : int;
    enum InputAction : int;
    enum MouseButton : int;

    bool InputFrameCallback(WinitInputHandler *ctx);
    void KeyInputCallback(WinitInputHandler *ctx, KeyCode key, int scancode, InputAction action);
    void CharInputCallback(WinitInputHandler *ctx, unsigned int ch);
    void MouseMoveCallback(WinitInputHandler *ctx, double dx, double dy);
    void MousePositionCallback(WinitInputHandler *ctx, double xPos, double yPos);
    void MouseButtonCallback(WinitInputHandler *ctx, MouseButton button, InputAction actions);
    void MouseScrollCallback(WinitInputHandler *ctx, double xOffset, double yOffset);
} // namespace sp
