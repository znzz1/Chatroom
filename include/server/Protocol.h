#pragma once
#include <cstdint>

constexpr uint16_t MSG_REGISTER = 1;
constexpr uint16_t MSG_CHANGE_PASSWORD = 2;
constexpr uint16_t MSG_CHANGE_DISPLAY_NAME = 3;
constexpr uint16_t MSG_LOGIN = 4;
constexpr uint16_t MSG_LOGOUT = 5;
constexpr uint16_t MSG_GET_ACTIVE_ROOMS = 6;
constexpr uint16_t MSG_GET_ALL_ROOMS = 7;
constexpr uint16_t MSG_GET_ROOM_INFO = 8;
constexpr uint16_t MSG_CREATE_ROOM = 9;
constexpr uint16_t MSG_DELETE_ROOM = 10;
constexpr uint16_t MSG_SET_ROOM_NAME = 11;
constexpr uint16_t MSG_SET_ROOM_DESCRIPTION = 12;
constexpr uint16_t MSG_SET_ROOM_MAX_USERS = 13;
constexpr uint16_t MSG_SET_ROOM_STATUS = 14;
constexpr uint16_t MSG_SEND_MESSAGE = 15;
constexpr uint16_t MSG_GET_MESSAGE_HISTORY = 16;
constexpr uint16_t MSG_JOIN_ROOM = 17;
constexpr uint16_t MSG_LEAVE_ROOM = 18;
constexpr uint16_t MSG_GET_ROOM_MEMBERS = 19;
constexpr uint16_t MSG_GET_USER_INFO = 20;


constexpr uint16_t MSG_REGISTER_RESPONSE = 1001;
constexpr uint16_t MSG_CHANGE_PASSWORD_RESPONSE = 1002;
constexpr uint16_t MSG_CHANGE_DISPLAY_NAME_RESPONSE = 1003;
constexpr uint16_t MSG_LOGIN_RESPONSE = 1004;
constexpr uint16_t MSG_LOGOUT_RESPONSE = 1005;
constexpr uint16_t MSG_GET_ACTIVE_ROOMS_RESPONSE = 1006;
constexpr uint16_t MSG_GET_ALL_ROOMS_RESPONSE = 1007;
constexpr uint16_t MSG_GET_ROOM_INFO_RESPONSE = 1008;
constexpr uint16_t MSG_CREATE_ROOM_RESPONSE = 1009;
constexpr uint16_t MSG_DELETE_ROOM_RESPONSE = 1010;
constexpr uint16_t MSG_SET_ROOM_NAME_RESPONSE = 1011;
constexpr uint16_t MSG_SET_ROOM_DESCRIPTION_RESPONSE = 1012;
constexpr uint16_t MSG_SET_ROOM_MAX_USERS_RESPONSE = 1013;
constexpr uint16_t MSG_SET_ROOM_STATUS_RESPONSE = 1014;
constexpr uint16_t MSG_SEND_MESSAGE_RESPONSE = 1015;
constexpr uint16_t MSG_GET_MESSAGE_HISTORY_RESPONSE = 1016;
constexpr uint16_t MSG_JOIN_ROOM_RESPONSE = 1017;
constexpr uint16_t MSG_LEAVE_ROOM_RESPONSE = 1018;
constexpr uint16_t MSG_GET_ROOM_MEMBERS_RESPONSE = 1019;
constexpr uint16_t MSG_GET_USER_INFO_RESPONSE = 1020;


constexpr uint16_t MSG_ERROR_RESPONSE = 1999;


constexpr uint16_t MSG_CHAT_MESSAGE_PUSH = 2001;
constexpr uint16_t MSG_USER_JOIN_PUSH = 2002;
constexpr uint16_t MSG_USER_LEAVE_PUSH = 2003;
constexpr uint16_t MSG_SYSTEM_MESSAGE_PUSH = 2004;
constexpr uint16_t MSG_ROOM_NAME_UPDATE_PUSH = 2005;
constexpr uint16_t MSG_ROOM_DESCRIPTION_UPDATE_PUSH = 2006;
constexpr uint16_t MSG_ROOM_MAX_USERS_UPDATE_PUSH = 2007;
constexpr uint16_t MSG_ROOM_STATUS_CHANGE_PUSH = 2008;



constexpr uint16_t MSG_PING = 3001;
constexpr uint16_t MSG_PONG = 3002;
constexpr uint16_t MSG_HEARTBEAT_TIMEOUT = 3003; 