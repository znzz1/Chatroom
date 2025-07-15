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
    
    void appendToReadBuffer(const char* data, size_t len);
    void clearReadBuffer();
    size_t getReadBufferSize() const;
    
    std::vector<Message> extractMessages();
    bool hasCompleteMessage() const;
    
    void appendToWriteBuffer(const std::string& data);
    std::string extractWriteBuffer(size_t maxLen);
    void clearWriteBuffer();
    size_t getWriteBufferSize() const;
    
    void sendMessage(uint16_t type, const std::string& data);
    
    void close();
    void setClosed(bool closed) { closed_ = closed; }
    
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    
private:
    static constexpr size_t HEADER_SIZE = 4;
    static constexpr size_t MAX_MESSAGE_LENGTH = 65536;
    
    int fd_;
    std::string read_buffer_;
    std::string write_buffer_;
    mutable std::mutex mutex_;
    bool closed_ = false;
}; 