#pragma once
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

class PasswordHasher {
public:
    static std::string generateSalt(int length = 16);
    
    static std::string hashPassword(const std::string& password, const std::string& salt);
    
    static bool verifyPassword(const std::string& password, const std::string& hashedPassword, const std::string& salt);
    
    static std::string hashPasswordWithSalt(const std::string& password);
    
    static bool extractSaltAndHash(const std::string& fullHash, std::string& salt, std::string& hash);
    
    static bool verifyPasswordWithFullHash(const std::string& password, const std::string& fullHash);

private:
    static std::string bytesToHex(const unsigned char* data, size_t length);
}; 