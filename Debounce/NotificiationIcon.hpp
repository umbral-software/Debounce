#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class NotificationIconClass;

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
    static NotificationIconClass _class;

private:
    HWND _hWnd;
};
