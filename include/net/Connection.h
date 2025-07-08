#pragma once
#include <string>
#include <mutex>
#include <memory>

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();
    
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;
    
    int getFd() const { return fd_; }
    bool isClosed() const { return closed_; }
    
    void appendToReadBuffer(const char* data, size_t len);
    std::string extractReadBuffer();
    void clearReadBuffer();
    size_t getReadBufferSize() const;
    
    void appendToWriteBuffer(const std::string& data);
    std::string extractWriteBuffer(size_t maxLen);
    void clearWriteBuffer();
    size_t getWriteBufferSize() const;
    
    void close();
    void setClosed(bool closed) { closed_ = closed; }
    
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    
private:
    int fd_;
    std::string read_buffer_;
    std::string write_buffer_;
    mutable std::mutex mutex_;
    bool closed_ = false;
}; 