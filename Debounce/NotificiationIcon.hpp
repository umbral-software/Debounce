#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mutex>

static constexpr DWORD DEFAULT_DEBOUNCE_THRESHOLD_MS = 10;
extern DWORD DEBOUNCE_THRESHOLD_MS;

class NotificationIcon {
public:
    NotificationIcon();
    NotificationIcon(const NotificationIcon&) = delete;
    NotificationIcon(NotificationIcon&& other) noexcept;
    ~NotificationIcon();

    NotificationIcon& operator=(const NotificationIcon&) = delete;
    NotificationIcon& operator=(NotificationIcon&& other) noexcept;

    HWND window() const noexcept;

private:
    static void InitClass();
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    static HINSTANCE _classHInstance;
    static bool _is_rtl;
    static ATOM _classAtom;
    static HMODULE _resourceDLL;
    static HMENU _hMenu, _hSubMenu;

    static std::once_flag _classInitFlag;    

private:
    HWND _hWnd;
};
