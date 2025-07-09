#include "net/Connection.h"
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() {
    close();
}

void Connection::appendToReadBuffer(const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    read_buffer_.append(data, len);
}

void Connection::clearReadBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    read_buffer_.clear();
}

size_t Connection::getReadBufferSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_buffer_.size();
}

bool Connection::hasCompleteMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (read_buffer_.size() < HEADER_SIZE) {
        return false;
    }
    
    uint16_t length;
    std::memcpy(&length, read_buffer_.data() + 2, sizeof(length));
    length = ntohs(length);
    
    if (length > MAX_MESSAGE_LENGTH) {
        return false;
    }
    
    return read_buffer_.size() >= HEADER_SIZE + length;
}

std::vector<Message> Connection::extractMessages() {
    std::vector<Message> messages;
    std::lock_guard<std::mutex> lock(mutex_);
    
    while (read_buffer_.size() >= HEADER_SIZE) {
        uint16_t type, length;
        std::memcpy(&type, read_buffer_.data(), sizeof(type));
        std::memcpy(&length, read_buffer_.data() + 2, sizeof(length));
        type = ntohs(type);
        length = ntohs(length);
        
        if (length > MAX_MESSAGE_LENGTH) {
            std::cerr << "[Connection] Message too long, dropped. Length: " << length << std::endl;
            read_buffer_.clear();
            break;
        }
        if (read_buffer_.size() < HEADER_SIZE + length) {
            break;
        }
        Message msg;
        msg.type = type;
        msg.length = length;
        msg.data = read_buffer_.substr(HEADER_SIZE, length);
        messages.push_back(std::move(msg));
        read_buffer_.erase(0, HEADER_SIZE + length);
    }
    return messages;
}

void Connection::appendToWriteBuffer(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_buffer_.append(data);
}

std::string Connection::extractWriteBuffer(size_t maxLen) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (write_buffer_.empty()) {
        return "";
    }
    
    size_t len = std::min(maxLen, write_buffer_.size());
    std::string data = write_buffer_.substr(0, len);
    write_buffer_.erase(0, len);
    
    return data;
}

void Connection::clearWriteBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    write_buffer_.clear();
}

size_t Connection::getWriteBufferSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return write_buffer_.size();
}

void Connection::sendMessage(uint16_t type, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) return;
    
    if (data.length() > MAX_MESSAGE_LENGTH) {
        std::cerr << "[Connection] sendMessage: data too long, not sent. Length: " << data.length() << std::endl;
        return;
    }
    
    uint16_t msgType = htons(type);
    uint16_t length = htons(data.length());
    write_buffer_.append(reinterpret_cast<const char*>(&msgType), sizeof(msgType));
    write_buffer_.append(reinterpret_cast<const char*>(&length), sizeof(length));
    write_buffer_.append(data);
}

void Connection::close() {
    if (!closed_) {
        ::close(fd_);
        closed_ = true;
    }
} 