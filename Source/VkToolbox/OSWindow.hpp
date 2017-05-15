#pragma once

// ================================================================================================
// File: VkToolbox/OSWindow.hpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: System-specific app window creation.
// ================================================================================================

#include "InPlaceFunction.hpp"
#include "Utils.hpp"

namespace VkToolbox
{

// ========================================================
// class OSWindow:
// ========================================================

class OSWindow final
{
public:

    struct CreateParameters
    {
        int          width;
        int          height;
        bool         openMaximized;
        const char * title;
    };

    // Different types for each kind of handle to avoid implicit conversions.
    struct HWindowImpl;
    struct HInstanceImpl;

    using HWindow   = HWindowImpl *;   // Handle to the underlaying OS window object.
    using HInstance = HInstanceImpl *; // Handle to the unique application instance given by the OS.

    explicit OSWindow(const CreateParameters & params);
    ~OSWindow();

    // Not copyable.
    OSWindow(const OSWindow &) = delete;
    OSWindow & operator = (const OSWindow &) = delete;

    // Event/message loop:
    void runEventLoop();
    void setStopEventLoop(bool stop);

    // Window queries:
    HWindow windowHandle() const;
    HInstance instanceHandle() const;
    const CreateParameters & creationParameters() const;

    // Current window dimensions (taking into account user resizing).
    Size2D size() const;
    void setSize(Size2D newSize, bool generateEvent = true);

    // Window/system events:
    InPlaceFunction32<bool()>                 onClose;            // Called when the user tries to close the window.
    InPlaceFunction32<void()>                 onRedraw;           // Called every run of the event loop to repaint the screen.
    InPlaceFunction32<void(Size2D)>           onResize;           // Called when the user resizes the window.
    InPlaceFunction32<void(int)>              onMouseScroll;      // Mouse wheel scroll event - param is the wheel delta: -1,0,+1.
    InPlaceFunction32<void(bool, bool, bool)> onMouseButtonClick; // Mouse click, each param is a btn: (lmb_down, mmb_down, rmb_down).
    InPlaceFunction32<void(unsigned)>         onVirtKeyPress;     // Virtual Key pressed - param is either a char or a virtual key code.
    InPlaceFunction32<void(char)>             onKeyChar;          // Character input in the window.

private:

    void createWindow(int width, int height, const char * title,
                      bool maximized, HWindow * outHWind, HInstance * outHInst);
    void destroyWindow();

    // System handles:
    HWindow   m_hWind = nullptr;
    HInstance m_hInst = nullptr;

    // Creation parameters / settings, etc:
    const CreateParameters m_createParams;
    Size2D m_currSize      = {0,0};
    bool   m_stopEventLoop = false;
};

// ========================================================

inline OSWindow::~OSWindow()
{
    destroyWindow();
}

inline void OSWindow::setStopEventLoop(const bool stop)
{
    m_stopEventLoop = stop;
}

inline OSWindow::HWindow OSWindow::windowHandle() const
{
    return m_hWind;
}

inline OSWindow::HInstance OSWindow::instanceHandle() const
{
    return m_hInst;
}

inline const OSWindow::CreateParameters & OSWindow::creationParameters() const
{
    return m_createParams;
}

inline Size2D OSWindow::size() const
{
    return m_currSize;
}

// ========================================================

} // namespace VkToolbox
