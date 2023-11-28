/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

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
