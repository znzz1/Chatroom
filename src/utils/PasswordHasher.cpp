#include "utils/PasswordHasher.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <iostream>

std::string PasswordHasher::generateSalt(int length) {
    const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.length() - 1);
    
    std::string salt;
    salt.reserve(length);
    for (int i = 0; i < length; ++i) {
        salt += charset[dis(gen)];
    }
    return salt;
}

std::string PasswordHasher::hashPassword(const std::string& password, const std::string& salt) {
    // 组合密码和盐值
    std::string combined = password + salt;
    
    // 使用OpenSSL 3.0 EVP接口计算SHA256哈希
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    if (EVP_DigestUpdate(ctx, combined.c_str(), combined.length()) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    
    // 转换为十六进制字符串
    return bytesToHex(hash, hash_len);
}

bool PasswordHasher::verifyPassword(const std::string& password, const std::string& hashedPassword, const std::string& salt) {
    std::string computedHash = hashPassword(password, salt);
    return computedHash == hashedPassword;
}

std::string PasswordHasher::hashPasswordWithSalt(const std::string& password) {
    std::string salt = generateSalt();
    std::string hash = hashPassword(password, salt);
    
    // 格式: salt$hash
    return salt + "$" + hash;
}

bool PasswordHasher::extractSaltAndHash(const std::string& fullHash, std::string& salt, std::string& hash) {
    size_t pos = fullHash.find('$');
    if (pos == std::string::npos) {
        return false;
    }
    
    salt = fullHash.substr(0, pos);
    hash = fullHash.substr(pos + 1);
    return true;
}

bool PasswordHasher::verifyPasswordWithFullHash(const std::string& password, const std::string& fullHash) {
    std::string salt, hash;
    if (!extractSaltAndHash(fullHash, salt, hash)) {
        return false;
    }
    
    return verifyPassword(password, hash, salt);
}

std::string PasswordHasher::bytesToHex(const unsigned char* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
} 