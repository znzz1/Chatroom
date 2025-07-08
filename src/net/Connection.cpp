#include "net/Connection.h"
#include <unistd.h>
#include <iostream>

Connection::Connection(int fd) : fd_(fd) {
    // 预分配缓冲区大小，减少内存重分配
    read_buffer_.reserve(4096);
    write_buffer_.reserve(4096);
}

Connection::~Connection() {
    if (!closed_) {
        close();
    }
}

void Connection::appendToReadBuffer(const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    read_buffer_.append(data, len);
}

std::string Connection::extractReadBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string result = std::move(read_buffer_);
    read_buffer_.clear();
    read_buffer_.reserve(4096);  // 重新预分配
    return result;
}

void Connection::clearReadBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    read_buffer_.clear();
}

size_t Connection::getReadBufferSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_buffer_.size();
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
    std::string result(write_buffer_.substr(0, len));
    write_buffer_.erase(0, len);
    
    return result;
}

void Connection::clearWriteBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    write_buffer_.clear();
}

size_t Connection::getWriteBufferSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return write_buffer_.size();
}

void Connection::close() {
    if (!closed_) {
        closed_ = true;
        ::close(fd_);
        std::cout << "Connection closed: " << fd_ << std::endl;
    }
} 