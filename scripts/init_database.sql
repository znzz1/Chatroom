CREATE DATABASE IF NOT EXISTS chatroom CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE chatroom;

DROP TABLE IF EXISTS messages;
DROP TABLE IF EXISTS rooms;
DROP TABLE IF EXISTS users;

CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    discriminator VARCHAR(4) NOT NULL,
    name VARCHAR(10) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(50) NOT NULL UNIQUE,
    is_admin BOOLEAN NOT NULL DEFAULT FALSE,
    created_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_email (email),
    INDEX idx_name_discriminator (name, discriminator),
    INDEX idx_role (is_admin),
    INDEX idx_created_time (created_time),
    
    UNIQUE KEY unique_name_discriminator (name, discriminator)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE rooms (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(20) NOT NULL,
    description VARCHAR(100),
    creator_id INT NOT NULL,
    max_users INT NOT NULL DEFAULT 100,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    created_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_name (name),
    INDEX idx_creator (creator_id),
    INDEX idx_status (is_active),
    INDEX idx_created_time (created_time),
    
    FOREIGN KEY (creator_id) REFERENCES users(id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE messages (
    message_id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    room_id INT NOT NULL,
    content TEXT NOT NULL,
    display_name VARCHAR(15) NOT NULL,
    send_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_room_time (room_id, send_time),
    INDEX idx_user_time (user_id, send_time),
    INDEX idx_send_time (send_time),
    
    FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO users (discriminator, name, email, password_hash, is_admin, created_time) 
VALUES ('0001', 'znzz1', 'zhun@usc.edu', 'salt$2a0e00cb53940019ffb5ee0dd02ea86282963740d3266c4e3d5632cbe173d797', 1, NOW())
ON DUPLICATE KEY UPDATE name = VALUES(name);

INSERT INTO rooms (name, description, creator_id, max_users, created_time) VALUES
('休闲聊天', '轻松聊天，分享生活趣事', 1, 50, NOW()),
('学习互助', '学习交流，互相帮助', 1, 60, NOW())
ON DUPLICATE KEY UPDATE name = VALUES(name);