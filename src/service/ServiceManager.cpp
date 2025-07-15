#include "service/ServiceManager.h"
#include "utils/PasswordHasher.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>
#include <ctime>

ServiceManager::ServiceManager() {
}

ServiceResult<User> ServiceManager::registerUser(const std::string& email, const std::string& password, const std::string& name) {
    return user_service_.registerUser(email, password, name);
}

ServiceResult<void> ServiceManager::changePassword(int userId, const std::string& oldPassword, const std::string& newPassword) {
    return user_service_.changePassword(userId, oldPassword, newPassword);
}

ServiceResult<void> ServiceManager::changeDisplayName(int userId, const std::string& newName) {
    return user_service_.changeDisplayName(userId, newName);
}

ServiceResult<void> ServiceManager::sendMessage(int userId, int roomId, const std::string& content, const std::string& displayName) {
    return user_service_.sendMessage(userId, roomId, content, displayName);
}

ServiceResult<std::vector<Message>> ServiceManager::getMessageHistory(int roomId, int limit) {
    return user_service_.getMessageHistory(roomId, limit);
}

ServiceResult<User> ServiceManager::login(const std::string& email, const std::string& password) {
    return user_service_.BaseService::login(email, password);
}

ServiceResult<std::vector<Room>> ServiceManager::getActiveRooms() {
    return user_service_.BaseService::getActiveRooms();
}

ServiceResult<Room> ServiceManager::getRoomInfo(int roomId) {
    return user_service_.BaseService::getRoomInfo(roomId);
}

ServiceResult<Room> ServiceManager::createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers) {
    return admin_service_.createRoom(adminId, name, description, maxUsers);
}

ServiceResult<void> ServiceManager::deleteRoom(int roomId) {
    return admin_service_.deleteRoom(roomId);
}

ServiceResult<void> ServiceManager::setRoomName(int roomId, const std::string& name) {
    return admin_service_.setRoomName(roomId, name);
}

ServiceResult<void> ServiceManager::setRoomDescription(int roomId, const std::string& description) {
    return admin_service_.setRoomDescription(roomId, description);
}

ServiceResult<void> ServiceManager::setRoomMaxUsers(int roomId, int maxUsers) {
    return admin_service_.setRoomMaxUsers(roomId, maxUsers);
}

ServiceResult<void> ServiceManager::setRoomStatus(int roomId, RoomStatus status) {
    return admin_service_.setRoomStatus(roomId, status);
}

ServiceResult<std::vector<Room>> ServiceManager::getAllRooms() {
    return admin_service_.getAllRooms();
}