CREATE DATABASE IF NOT EXISTS chatroom CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE chatroom;

DROP TABLE IF EXISTS room_members;
DROP TABLE IF EXISTS rooms;
DROP TABLE IF EXISTS users;

CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    discriminator VARCHAR(4) NOT NULL,
    name VARCHAR(50) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(100) NOT NULL UNIQUE,
    role TINYINT NOT NULL DEFAULT 1,
    is_online BOOLEAN NOT NULL DEFAULT FALSE,
    created_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_email (email),
    INDEX idx_name_discriminator (name, discriminator),
    INDEX idx_online (is_online),
    INDEX idx_role (role),
    UNIQUE KEY unique_name_discriminator (name, discriminator)
);

CREATE TABLE rooms (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT,
    creator_id INT NOT NULL,
    max_users INT NOT NULL DEFAULT 0,
    current_users INT NOT NULL DEFAULT 0,
    status TINYINT NOT NULL DEFAULT 1,
    created_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_activity_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    INDEX idx_name (name),
    INDEX idx_creator (creator_id),
    INDEX idx_status (status),
    INDEX idx_activity (last_activity_time),
    FOREIGN KEY (creator_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE room_members (
    room_id INT NOT NULL,
    user_id INT NOT NULL,
    name VARCHAR(50) NOT NULL,
    discriminator VARCHAR(4) NOT NULL,
    email VARCHAR(100) NOT NULL,
    role TINYINT NOT NULL DEFAULT 1,
    join_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (room_id, user_id),
    INDEX idx_room (room_id),
    INDEX idx_user (user_id),
    INDEX idx_join_time (join_time),
    FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

INSERT INTO users (discriminator, name, password_hash, email, role, created_time) 
VALUES ('0001', 'znzz1', 'salt$2a0e00cb53940019ffb5ee0dd02ea86282963740d3266c4e3d5632cbe173d797', 'zhun@usc.edu', 2, NOW())
ON DUPLICATE KEY UPDATE name = name;

INSERT INTO rooms (name, description, creator_id, max_users, created_time, last_activity_time)
SELECT '休闲聊天', '轻松聊天，分享生活趣事', id, 30, NOW(), NOW()
FROM users WHERE email = 'zhun@usc.edu'
ON DUPLICATE KEY UPDATE name = name;

INSERT INTO rooms (name, description, creator_id, max_users, created_time, last_activity_time)
SELECT '技术讨论', '讨论编程、技术相关话题', id, 50, NOW(), NOW()
FROM users WHERE email = 'zhun@usc.edu'
ON DUPLICATE KEY UPDATE name = name;

INSERT INTO rooms (name, description, creator_id, max_users, created_time, last_activity_time)
SELECT '游戏交流', '游戏攻略、心得分享', id, 40, NOW(), NOW()
FROM users WHERE email = 'zhun@usc.edu'
ON DUPLICATE KEY UPDATE name = name;

INSERT IGNORE INTO room_members (room_id, user_id, name, discriminator, email, role, join_time)
SELECT r.id, u.id, u.name, u.discriminator, u.email, u.role, NOW()
FROM rooms r, users u 
WHERE u.email = 'zhun@usc.edu';

UPDATE rooms r SET current_users = (
    SELECT COUNT(*) FROM room_members rm WHERE rm.room_id = r.id
);