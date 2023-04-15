#pragma once

#pragma warning (disable: 28182)
#pragma warning (disable: 28183)

#define _USERDEBUG

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define VAR_NAME(a) #a

#include <iostream>

#include <WinSock2.h>
#pragma comment (lib, "ws2_32.lib")

#include <Windows.h>
#include <windowsx.h>
#include <direct.h>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include <shared_mutex>

#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")

#pragma comment (lib, "winmm.lib")

#include <d3d9.h>
#pragma comment (lib, "d3d9.lib")

#ifdef _USERDEBUG
#define TRACE_FUNC(Text, ...) printf("[+] " __FUNCTION__ " > " Text, __VA_ARGS__)
#define TRACE(Text, ...) printf(Text, __VA_ARGS__)
#else
#define TRACE_FUNC(Text, ...)
#define TRACE(Text, ...)
#endif // _USERDEBUG

#include "../libs/DXWF/framework.h"

#ifdef _WIN64
#pragma comment (lib, "libs/DXWF/x64/DXWF.lib")
#else
#pragma comment (lib, "libs/DXWF/x86/DXWF.lib")
#endif // _WIN64

#include "../libs/ImGui/imgui.h"
#include "../libs/ImGui/imgui_internal.h"
#include "../libs/ImGui/imgui_impl_dx9.h"
#include "../libs/ImGui/imgui_impl_win32.h"

#include "defines.h"

#include "ierror/ierror.h"
#include "window/window.h"
#include "network/network.h"
#include "chat_common/chat_common.h"
#include "gui/gui.h"
#include "chat/chat.h"