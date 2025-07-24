#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>


struct SendResult {
    ssize_t bytes_sent;
    bool has_more_data;
    bool error;
};

struct ReadResult {
    ssize_t bytes_read;
    bool error;
    bool connection_closed;
};

struct NetworkMessage {
    uint16_t type;
    uint16_t length;
    std::string data;
    
    NetworkMessage() : type(0), length(0) {}
    NetworkMessage(uint16_t t, const std::string& d) 
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
    
    std::vector<NetworkMessage> extractMessages();
    void appendToWriteBuffer(const std::string& data);
    
    SendResult sendFromWriteBuffer(int fd, size_t maxLen);
    ReadResult recvToReadBuffer(int fd, size_t maxLen);

    void sendMessage(uint16_t type, const std::string& data);
    void setWriteEventCallback(std::function<void(int)> callback);
        
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    
private:
    static constexpr size_t HEADER_SIZE = 4;
    static constexpr size_t MAX_MESSAGE_LENGTH = 65536;
    static constexpr size_t MAX_READ_BUFFER_SIZE  = 1024 * 1024;
    static constexpr size_t MAX_WRITE_BUFFER_SIZE = 1024 * 1024;
    
    int fd_;
    std::string read_buffer_;
    std::string write_buffer_;
    mutable std::mutex mutex_;
    
    std::function<void(int)> write_callback_;
}; 