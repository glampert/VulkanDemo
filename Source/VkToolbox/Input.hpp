#pragma once

// ================================================================================================
// File: VkToolbox/Input.hpp
// Author: Guilherme R. Lampert
// Created on: 23/04/17
// Brief: Mouse and keyboard input utils.
// ================================================================================================

namespace VkToolbox
{

class OSWindow;

// ========================================================

struct MouseState final
{
    static int x;
    static int y;
    static int deltaX;
    static int deltaY;
    static int wheelDelta; // > 0 scroll forward, < 0 scroll back, 0 = no scroll.

    static bool LButtonDown;
    static bool MButtonDown;
    static bool RButtonDown;
    static bool isOverWindow;

    static void update(OSWindow * window);
    static void setCursorPos(int newX, int newY, OSWindow * window);
    static void hideCursor(bool trueIfShouldHide);
    static void clear();
};

// ========================================================

struct Key final
{
    enum Code
    {
        Null = 0,

        // 0 - 9 are the same as ASCII '0' - '9' (0x30 - 0x39)
        // A - Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
        // These match the standard windows VK_* keys that can be passed to GetAsyncKeyState().
        VK_BACK         = 0x08,
        VK_TAB          = 0x09,
        VK_RETURN       = 0x0D,
        VK_SHIFT        = 0x10,
        VK_CONTROL      = 0x11,
        VK_MENU         = 0x12,
        VK_PAUSE        = 0x13,
        VK_CAPSLOCK     = 0x14,
        VK_ESCAPE       = 0x1B,
        VK_SPACE        = 0x20,
        VK_PRIOR        = 0x21,
        VK_NEXT         = 0x22,
        VK_END          = 0x23,
        VK_HOME         = 0x24,
        VK_LEFT         = 0x25,
        VK_UP           = 0x26,
        VK_RIGHT        = 0x27,
        VK_DOWN         = 0x28,
        VK_INSERT       = 0x2D,
        VK_DELETE       = 0x2E,
        VK_F1           = 0x70,
        VK_F2           = 0x71,
        VK_F3           = 0x72,
        VK_F4           = 0x73,
        VK_F5           = 0x74,
        VK_F6           = 0x75,
        VK_F7           = 0x76,
        VK_F8           = 0x77,
        VK_F9           = 0x78,
        VK_F10          = 0x79,
        VK_F11          = 0x7A,
        VK_F12          = 0x7B,
        VK_LSHIFT       = 0xA0,
        VK_RSHIFT       = 0xA1,
        VK_LCONTROL     = 0xA2,
        VK_RCONTROL     = 0xA3,
        VK_LMENU        = 0xA4,
        VK_RMENU        = 0xA5,
    };
};

// ========================================================

struct KeyboardState final
{
    static bool isKeyDown(unsigned k);
    static bool isKeyUp(unsigned k);

    static void update(OSWindow * window);
    static void clear();
};

// ========================================================

} // namespace VkToolbox
