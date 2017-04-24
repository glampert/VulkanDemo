
// ================================================================================================
// File: VkToolbox/OSWindow.cpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: System-specific app window creation.
// ================================================================================================

#include "OSWindow.hpp"
#include "Input.hpp"
#include "Log.hpp"

#if defined(WIN32) || defined(WIN64)
    #define NOMINMAX 1
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
#endif // WIN32 || WIN64

namespace VkToolbox
{

OSWindow::OSWindow(const CreateParameters & params)
    : m_createParams{ params }
{
    createWindow(params.width, params.height, params.title,
                 params.openMaximized, &m_hWind, &m_hInst);

    Log::debugF("OSWindow created successfully.");
}

#if defined(WIN32) || defined(WIN64)

extern "C"
{

static LRESULT CALLBACK OSWindow_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto * window = reinterpret_cast<OSWindow *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (window == nullptr)
    {
        // First few messages when CreateWindow is called - we don't have a pointer yet.
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg)
    {
    case WM_CLOSE :
    case WM_DESTROY :
        {
            PostQuitMessage(0);
            return 0;
        }
    case WM_SIZE :
        {
            Size2D newSize;
            newSize.width  = LOWORD(lParam);
            newSize.height = HIWORD(lParam);
            window->setSize(newSize, false);
            if (window->onResize)
            {
                window->onResize(newSize);
            }
            return 0;
        }
    case WM_MOUSEWHEEL :
        {
            const int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            MouseState::wheelDelta = clamp(zDelta, -1, +1);
            if (window->onMouseScroll)
            {
                window->onMouseScroll(MouseState::wheelDelta);
            }
            return 0;
        }
    case WM_CHAR :
        {
            if (window->onKeyChar)
            {
                const char keyChar = wParam & 0xFF;
                window->onKeyChar(keyChar);
                return 0;
            }
            break;
        }
    case WM_KEYDOWN :
        {
            if (window->onVirtKeyPress)
            {
                const auto keyCode = static_cast<unsigned>(wParam);
                window->onVirtKeyPress(keyCode);
                return 0;
            }
            break;
        }
    case WM_LBUTTONDOWN :
        {
            if (window->onMouseButtonClick)
            {
                window->onMouseButtonClick(true, false, false);
                return 0;
            }
            break;
        }
    case WM_MBUTTONDOWN :
        {
            if (window->onMouseButtonClick)
            {
                window->onMouseButtonClick(false, true, false);
                return 0;
            }
            break;
        }
    case WM_RBUTTONDOWN :
        {
            if (window->onMouseButtonClick)
            {
                window->onMouseButtonClick(false, false, true);
                return 0;
            }
            break;
        }
    default :
        break;
    } // switch

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

} // extern "C"

void OSWindow::createWindow(const int width, const int height, const char * const title,
                            const bool maximized, HWindow * outHWind, HInstance * outHInst)
{
    assert(width  > 0);
    assert(height > 0);
    assert(title    != nullptr);
    assert(outHWind != nullptr);
    assert(outHInst != nullptr);

    (*outHInst) = reinterpret_cast<HInstance>(GetModuleHandle(nullptr));

    WNDCLASSEX winClass    = {};
    winClass.cbSize        = sizeof(WNDCLASSEX);
    winClass.style         = (CS_HREDRAW | CS_VREDRAW | CS_OWNDC);
    winClass.lpfnWndProc   = &OSWindow_WndProc;
    winClass.cbClsExtra    = 0;
    winClass.cbWndExtra    = 0;
    winClass.hInstance     = reinterpret_cast<HINSTANCE>(*outHInst);
    winClass.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    winClass.hIconSm       = LoadIcon(nullptr, IDI_WINLOGO);
    winClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    winClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    winClass.lpszMenuName  = nullptr;
    winClass.lpszClassName = title;

    if (!RegisterClassEx(&winClass))
    {
        Log::fatalF("Unable to create app window! RegisterClassEx() failed!");
    }

    const DWORD winStyle = (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU);

    RECT wr = { 0, 0, width, height };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    (*outHWind) = reinterpret_cast<HWindow>(CreateWindowEx(
                    WS_EX_APPWINDOW,                        // extended style
                    title,                                  // class name
                    title,                                  // app name
                    winStyle,                               // window style
                    0, 0,                                   // x/y coords
                    wr.right  - wr.left,                    // width
                    wr.bottom - wr.top,                     // height
                    nullptr,                                // handle to parent (none)
                    nullptr,                                // handle to menu (none)
                    reinterpret_cast<HINSTANCE>(*outHInst), // hInstance
                    nullptr));                              // no extra parameters

    if ((*outHWind) == nullptr)
    {
        Log::fatalF("Unable to create app window! CreateWindowEx() failed!");
    }

    auto hWnd = reinterpret_cast<HWND>(*outHWind);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ShowWindow(hWnd, (maximized ? SW_SHOWMAXIMIZED : SW_SHOW));
    UpdateWindow(hWnd);

    m_currSize.width  = wr.right  - wr.left;
    m_currSize.height = wr.bottom - wr.top;
}

void OSWindow::destroyWindow()
{
    if (m_hWind == nullptr)
    {
        return; // Never created.
    }

    DestroyWindow(reinterpret_cast<HWND>(m_hWind));
    m_hWind = nullptr;
    m_hInst = nullptr;
}

void OSWindow::runEventLoop()
{
    MSG msg = {};
    while (!m_stopEventLoop)
    {
        if (onRedraw)
        {
            onRedraw();
        }

        // Hacky way to reset the mouse wheel scroll, since the value
        // is only updated when we get a scroll event in the window.
        MouseState::wheelDelta = 0;

        // Poll system events:
        while (PeekMessage(&msg, reinterpret_cast<HWND>(windowHandle()), 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                if (onClose)
                {
                    // Give the callback a chance to cancel a quit message.
                    if (onClose())
                    {
                        m_stopEventLoop = true;
                        break;
                    }
                }
                else
                {
                    m_stopEventLoop = true;
                    break;
                }
            }
        }
    }
}

void OSWindow::setSize(const Size2D newSize, const bool generateEvent)
{
    if (generateEvent)
    {
        RECT wr = { 0, 0, newSize.width, newSize.height };
        AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

        m_currSize.width  = wr.right  - wr.left;
        m_currSize.height = wr.bottom - wr.top;

        SetWindowPos(reinterpret_cast<HWND>(windowHandle()), HWND_TOP, 0, 0,
                     m_currSize.width, m_currSize.height, (SWP_NOMOVE | SWP_NOREPOSITION));
    }
    else
    {
        m_currSize = newSize;
    }
}

#else // !WIN32 && !WIN64
    #error "OSWindow not implemented for this platform!"
#endif // WIN32 || WIN64

} // namespace VkToolbox
