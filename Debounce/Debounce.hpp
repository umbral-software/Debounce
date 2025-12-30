#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static constexpr DWORD DEFAULT_DEBOUNCE_THRESHOLD_MS = 10;

DWORD GetDebounceDelay() noexcept;
void SetDebounceDelay(DWORD delayMs);
