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

#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")

#pragma comment (lib, "winmm.lib")

#include <d3d9.h>
#pragma comment (lib, "d3d9.lib")

#ifdef _USERDEBUG
#define LOGGER(Text, ...) printf("[+] %s() -> " Text, __FUNCTION__, __VA_ARGS__)
#else
#define LOGGER(Text, ...)
#endif // _USERDEBUG

#include "../libs/DXWF/framework.h"
#pragma comment (lib, "libs/DXWF/DXWF.lib")

#include "../libs/ImGui/imgui.h"
#include "../libs/ImGui/imgui_internal.h"
#include "../libs/ImGui/imgui_impl_dx9.h"
#include "../libs/ImGui/imgui_impl_win32.h"

#include "defines.h"
#include "structures/structures.h"

#include "window/window.h"
#include "chat_data/chat_data.h"
#include "network/network.h"
#include "network_chat_manager/network_chat_manager.h"
#include "globals/globals.h"
#include "gui/gui.h"
#include "chat/chat.h"

extern bool IsLittleEndian();