#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <vector>

struct Message {
    uint16_t type;
    uint16_t length;
    std::string data;
    
    Message() : type(0), length(0) {}
    Message(uint16_t t, const std::string& d) 
        : type(t), length(d.length()), data(d) {}
};

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
    
    // 原始缓冲区操作
    void appendToReadBuffer(const char* data, size_t len);
    void clearReadBuffer();
    size_t getReadBufferSize() const;
    
    // 消息级操作
    std::vector<Message> extractMessages();
    bool hasCompleteMessage() const;
    
    // 写缓冲区操作
    void appendToWriteBuffer(const std::string& data);
    std::string extractWriteBuffer(size_t maxLen);
    void clearWriteBuffer();
    size_t getWriteBufferSize() const;
    
    // 消息发送
    void sendMessage(uint16_t type, const std::string& data);
    
    void close();
    void setClosed(bool closed) { closed_ = closed; }
    
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    
private:
    static constexpr size_t HEADER_SIZE = 4;  // type(2) + length(2)
    static constexpr size_t MAX_MESSAGE_LENGTH = 65536; // 64KB
    
    int fd_;
    std::string read_buffer_;
    std::string write_buffer_;
    mutable std::mutex mutex_;
    bool closed_ = false;
}; 