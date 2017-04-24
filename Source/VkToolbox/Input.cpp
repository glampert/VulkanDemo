
// ================================================================================================
// File: VkToolbox/Input.hpp
// Author: Guilherme R. Lampert
// Created on: 23/04/17
// Brief: Mouse and keyboard input utils.
// ================================================================================================

#include "Input.hpp"
#include "Log.hpp"
#include "OSWindow.hpp"
#include <array>

#if defined(WIN32) || defined(WIN64)
    #define NOMINMAX 1
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
#endif // WIN32 || WIN64

namespace VkToolbox
{

// ========================================================
// Local helpers:
// ========================================================

struct SavedMouseState
{
    int  x;
    int  y;
    int  deltaX;
    int  deltaY;
    bool LButtonDown;
    bool MButtonDown;
    bool RButtonDown;
    bool isOverWindow;
};

struct SavedKbdState
{
    std::array<std::uint8_t, 256> keys;
};

static SavedMouseState s_prevMouseSate = {};
static SavedKbdState   s_currKbdState  = {};

inline bool testKeyPressed(const std::uint8_t value) { return (value & 0x80) > 0; };

// ========================================================
// struct MouseState:
// ========================================================

int  MouseState::x            = 0;
int  MouseState::y            = 0;
int  MouseState::deltaX       = 0;
int  MouseState::deltaY       = 0;
int  MouseState::wheelDelta   = 0;
bool MouseState::LButtonDown  = false;
bool MouseState::MButtonDown  = false;
bool MouseState::RButtonDown  = false;
bool MouseState::isOverWindow = false;

void MouseState::update(OSWindow * window)
{
    POINT pos;
    if (!::GetCursorPos(&pos))
    {
        Log::warningF("GetCursorPos failed with error code %#x", GetLastError());
        return;
    }

    HWND hwnd = nullptr;
    if (window != nullptr)
    {
        hwnd = reinterpret_cast<HWND>(window->windowHandle());
        if (!::ScreenToClient(hwnd, &pos))
        {
            Log::warningF("ScreenToClient failed with error code %#x", GetLastError());
            return;
        }
    }

    SavedMouseState newState;
    newState.x           = pos.x;
    newState.y           = pos.y;
    newState.deltaX      = pos.x - s_prevMouseSate.x;
    newState.deltaY      = pos.y - s_prevMouseSate.y;
    newState.LButtonDown = (GetKeyState(VK_LBUTTON) & 0x8000) > 0;
    newState.MButtonDown = (GetKeyState(VK_MBUTTON) & 0x8000) > 0;
    newState.RButtonDown = (GetKeyState(VK_RBUTTON) & 0x8000) > 0;

    if (hwnd != nullptr && ::GetForegroundWindow() == hwnd)
    {
        RECT clientRect;
        if (!::GetClientRect(hwnd, &clientRect))
        {
            Log::warningF("GetClientRect failed with error code %#x", GetLastError());
            return;
        }
        newState.isOverWindow = (pos.x >= 0 && pos.x < clientRect.right && pos.y >= 0 && pos.y < clientRect.bottom);
    }
    else
    {
        newState.isOverWindow = false;
    }

    s_prevMouseSate.x            = MouseState::x;
    s_prevMouseSate.y            = MouseState::y;
    s_prevMouseSate.deltaX       = MouseState::deltaX;
    s_prevMouseSate.deltaY       = MouseState::deltaY;
    s_prevMouseSate.LButtonDown  = MouseState::LButtonDown;
    s_prevMouseSate.MButtonDown  = MouseState::MButtonDown;
    s_prevMouseSate.RButtonDown  = MouseState::RButtonDown;
    s_prevMouseSate.isOverWindow = MouseState::isOverWindow;

    MouseState::x                = newState.x;
    MouseState::y                = newState.y;
    MouseState::deltaX           = newState.deltaX;
    MouseState::deltaY           = newState.deltaY;
    MouseState::LButtonDown      = newState.LButtonDown;
    MouseState::MButtonDown      = newState.MButtonDown;
    MouseState::RButtonDown      = newState.RButtonDown;
    MouseState::isOverWindow     = newState.isOverWindow;
}

void MouseState::setCursorPos(const int newX, const int newY, OSWindow * window)
{
    POINT pos;
    pos.x = newX;
    pos.y = newY;

    if (window != nullptr)
    {
        HWND hwnd = reinterpret_cast<HWND>(window->windowHandle());
        if (!::ClientToScreen(hwnd, &pos))
        {
            Log::warningF("ClientToScreen failed with error code %#x", GetLastError());
            return;
        }
    }

    if (!::SetCursorPos(pos.x, pos.y))
    {
        Log::warningF("SetCursorPos failed with error code %#x", GetLastError());
        return;
    }

    MouseState::update(window); // Force an update.

    // Prev/current states now should match.
    s_prevMouseSate.x            = MouseState::x;
    s_prevMouseSate.y            = MouseState::y;
    s_prevMouseSate.deltaX       = MouseState::deltaX;
    s_prevMouseSate.deltaY       = MouseState::deltaY;
    s_prevMouseSate.LButtonDown  = MouseState::LButtonDown;
    s_prevMouseSate.MButtonDown  = MouseState::MButtonDown;
    s_prevMouseSate.RButtonDown  = MouseState::RButtonDown;
    s_prevMouseSate.isOverWindow = MouseState::isOverWindow;   
}

void MouseState::hideCursor(const bool trueIfShouldHide)
{
    ::ShowCursor(trueIfShouldHide ? false : true);
}

void MouseState::clear()
{
    s_prevMouseSate          = {};
    MouseState::x            = 0;
    MouseState::y            = 0;
    MouseState::deltaX       = 0;
    MouseState::deltaY       = 0;
    MouseState::wheelDelta   = 0;
    MouseState::LButtonDown  = false;
    MouseState::MButtonDown  = false;
    MouseState::RButtonDown  = false;
    MouseState::isOverWindow = false;
}

// ========================================================
// struct KeyboardState:
// ========================================================

bool KeyboardState::isKeyDown(const unsigned k)
{
    return testKeyPressed(s_currKbdState.keys[k]);
}

bool KeyboardState::isKeyUp(const unsigned k)
{
    return !testKeyPressed(s_currKbdState.keys[k]);
}

void KeyboardState::update(OSWindow * window)
{
    HWND hwnd = (window != nullptr ? reinterpret_cast<HWND>(window->windowHandle()) : nullptr);
    if (::GetForegroundWindow() != hwnd)
    {
        return; // Don't update if window out of focus.
    }

    SavedKbdState newKbdState = {};
    if (!::GetKeyboardState(newKbdState.keys.data()))
    {
        Log::warningF("GetKeyboardState failed with error code %#x", GetLastError());
        return;
    }
    s_currKbdState = newKbdState;
}

void KeyboardState::clear()
{
    s_currKbdState.keys.fill(0);
}

} // namespace VkToolbox
