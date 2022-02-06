#pragma once

constexpr auto NULL_TERMINATE_BYTE = '\0';
constexpr auto NULL_TERMINATE_BYTE_SIZE = 1;

enum APP_MODE
{
	NOT_INITIALIZED,
	PROCESS_INITIALIZING,
	FAILED_INITIALIZING,
	HOST,
	CLIENT
};

enum GUI_MODE
{
	SELECT_MODE,
	ENTER_USERNAME
};

constexpr auto MAX_USERNAME_SIZE = 32;
constexpr auto MAX_MESSAGE_TEXT_SIZE = 4096;

constexpr auto CLIENT_SOCKET = 0;

constexpr auto MAX_PROCESSED_USERS_IN_CHAT = 10;

constexpr auto pszApplicationName = "Local chat";