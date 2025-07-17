#include "net/Connection.h"
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>
#include <algorithm>

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() {
    std::lock_guard<std::mutex> lock(mutex_);
    read_buffer_.clear();
    write_buffer_.clear();
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
        
        // 无完整消息 等待更多数据
        if (read_buffer_.size() < HEADER_SIZE + length) {
            break;
        }
        
        messages.emplace_back(type, read_buffer_.substr(HEADER_SIZE, length));
        read_buffer_.erase(0, HEADER_SIZE + length);
    }
    return messages;
}

void Connection::appendToWriteBuffer(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (write_buffer_.size() + data.size() > MAX_WRITE_BUFFER_SIZE) {
        return;
    }
    write_buffer_.append(data);
}

Connection::SendResult Connection::sendFromWriteBuffer(int fd, size_t maxLen) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SendResult result;
    result.bytes_sent = 0;
    result.has_more_data = false;
    result.error = false;
    
    if (write_buffer_.empty()) {
        return result;
    }
    
    size_t len = std::min(maxLen, write_buffer_.size());
    ssize_t n = send(fd, write_buffer_.data(), len, 0);
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            result.error = false; // 不是真正的错误
        } else {
            result.error = true;
        }
        return result;
    }
    
    result.bytes_sent = n;
    if (n > 0) {
        write_buffer_.erase(0, n);
    }
    
    result.has_more_data = !write_buffer_.empty();
    return result;
}

Connection::ReadResult Connection::recvToReadBuffer(int fd, size_t maxLen) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ReadResult result;
    result.bytes_read = 0;
    result.error = false;
    result.connection_closed = false;
    
    if (read_buffer_.size() + maxLen >= MAX_READ_BUFFER_SIZE) {
        result.error = true;
        return result;
    }
    
    size_t original_size = read_buffer_.size();
    read_buffer_.resize(original_size + maxLen);
    ssize_t n = recv(fd, &read_buffer_[original_size], maxLen, 0);
    
    if (n < 0) {
        read_buffer_.resize(original_size);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            result.error = false;
        } else {
            result.error = true;
        }
        return result;
    } else if (n == 0) {
        read_buffer_.resize(original_size);
        result.connection_closed = true;
        return result;
    }
    
    read_buffer_.resize(original_size + n);
    result.bytes_read = n;
    return result;
}

void Connection::sendMessage(uint16_t type, const std::string& data) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (data.length() > MAX_MESSAGE_LENGTH) {
            return;
        }
        
        write_buffer_.reserve(write_buffer_.size() + HEADER_SIZE + data.length());

        uint16_t msgType = htons(type);
        uint16_t length  = htons(data.length());
        write_buffer_.append(reinterpret_cast<const char*>(&msgType), sizeof(msgType));
        write_buffer_.append(reinterpret_cast<const char*>(&length),  sizeof(length));
        write_buffer_.append(data);
    }
    if (write_callback_) write_callback_(fd_);
}

void Connection::setWriteEventCallback(std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_callback_ = std::move(callback);
}