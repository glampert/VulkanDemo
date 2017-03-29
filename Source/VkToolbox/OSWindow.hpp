#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/OSWindow.hpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: System-specific app window creation.
// ================================================================================================

#include <functional>
#include "Utils.hpp"

namespace VkToolbox
{

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

    // Different types for each kind of handle.
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
    HWindow getWindowHandle() const;
    HInstance getInstanceHandle() const;
    const CreateParameters & getCreationParameters() const;

    // Current window dimensions (taking into account user resizing).
    Size2D getSize() const;
    void setSize(Size2D newSize, bool generateEvent = true);

    // User data pointer we can optionally store:
    template<typename T> T * getUserPointer() const;
    template<typename T> void setUserPointer(T * ptr);

    // Window/system events:
    std::function<bool()>             onClose;  // Called when the user tries to close the window.
    std::function<void()>             onRedraw; // Called every run of the event loop to repaint the screen.
    std::function<void(const Size2D)> onResize; // Called when the user resizes the window.

private:

    void createWindow(int width, int height, const char * title,
                      bool maximized, HWindow * outHWind, HInstance * outHInst);
    void destroyWindow();

    // System handles:
    HWindow   m_hWind;
    HInstance m_hInst;

    // Pointer to some user structure - this can be retrieved in the window callbacks.
    void * m_userPtr;

    // Creation parameters / settings, etc:
    const CreateParameters m_createParams;
    Size2D m_currSize;
    bool m_stopEventLoop;
};

// ========================================================
// OSWindow inline methods:
// ========================================================

inline OSWindow::~OSWindow()
{
    destroyWindow();
}

inline void OSWindow::setStopEventLoop(const bool stop)
{
    m_stopEventLoop = stop;
}

inline OSWindow::HWindow OSWindow::getWindowHandle() const
{
    return m_hWind;
}

inline OSWindow::HInstance OSWindow::getInstanceHandle() const
{
    return m_hInst;
}

inline const OSWindow::CreateParameters & OSWindow::getCreationParameters() const
{
    return m_createParams;
}

inline Size2D OSWindow::getSize() const
{
    return m_currSize;
}

template<typename T>
inline T * OSWindow::getUserPointer() const
{
    return static_cast<T *>(m_userPtr);
}

template<typename T>
inline void OSWindow::setUserPointer(T * ptr)
{
    m_userPtr = ptr;
}

} // namespace VkToolbox
