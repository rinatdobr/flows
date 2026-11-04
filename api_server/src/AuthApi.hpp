#pragma once
#include "IDatabase.hpp"
#include "crow.h"
#include <pqxx/pqxx>
#include <memory>
#include <chrono>
#include <ctime>
#include <random>
#include <sstream>
#include <iomanip>

// Authentication API for user registration, login, and session management
class AuthApi {
public:
    explicit AuthApi(std::shared_ptr<IDatabase> db) : db_(std::move(db)) {}
    
    // User Registration (POST /api/v1/auth/register)
    // No email verification as requested
    crow::response register_user(const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }
        
        // Validate required fields
        if (!json.has("email") || !json.has("password") || !json.has("first_name")) {
            return crow::response(400, "Missing required fields: email, password, first_name");
        }
        
        std::string email = json["email"].s();
        std::string password = json["password"].s();
        std::string first_name = json["first_name"].s();
        
        // Optional fields
        std::string last_name = "";
        std::optional<std::string> nickname = std::nullopt;
        
        if (json.has("last_name")) {
            last_name = json["last_name"].s();
        }
        if (json.has("nickname")) {
            std::string nick_val = json["nickname"].s();
            if (!nick_val.empty()) {
                nickname = nick_val;
            }
        }
        
        // Validate email format (basic validation)
        if (email.find("@") == std::string::npos || email.find(".") == std::string::npos) {
            return crow::response(400, "Invalid email format");
        }
        
        // Validate password strength (basic)
        if (password.length() < 6) {
            return crow::response(400, "Password must be at least 6 characters long");
        }
        
        // Check if user already exists
        if (db_->user_exists_by_email(email)) {
            return crow::response(409, "User with this email already exists");
        }
        
        if (nickname.has_value() && db_->user_exists_by_nickname(nickname.value())) {
            return crow::response(409, "User with this nickname already exists");
        }
        
        // Create password hash (in production, use bcrypt or argon2)
        std::string password_hash = db_->hash_password(password);
        
        // Create user
        auto [success, result] = db_->create_user(first_name, last_name, nickname, email, password_hash);
        
        if (!success) {
            return crow::response(500, result.empty() ? "Failed to create user" : result);
        }
        
        // Return user data (without password hash)
        crow::json::wvalue ret;
        ret["id"] = result;
        ret["email"] = email;
        ret["first_name"] = first_name;
        if (!last_name.empty()) ret["last_name"] = last_name;
        if (nickname.has_value()) ret["nickname"] = nickname.value();
        
        return crow::response(201, ret);
    }
    
    // User Login (POST /api/v1/auth/login)
    crow::response login(const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) {
            return crow::response(400, "Invalid JSON");
        }
        
        if (!json.has("email") || !json.has("password")) {
            return crow::response(400, "Missing required fields: email, password");
        }
        
        std::string email = json["email"].s();
        std::string password = json["password"].s();
        
        // Get user by email
        auto [found, user_id, password_hash] = db_->get_user_by_email(email);
        
        if (!found) {
            return crow::response(401, "Invalid email or password");
        }
        
        // Verify password
        if (!db_->verify_password(password, password_hash)) {
            return crow::response(401, "Invalid email or password");
        }
        
        // Create session token
        std::string token = generate_session_token();
        std::string token_hash = db_->hash_password(token); // Store hash in database
        
        // Calculate expiration (24 hours from now)
        auto now = std::chrono::system_clock::now();
        auto expires = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now() + std::chrono::hours(24)
        );
        
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&expires), "%Y-%m-%d %H:%M:%S");
        std::string expires_at = ss.str();
        
        // Create session
        auto [session_success, session_result] = db_->create_session(user_id, token_hash, expires_at);
        
        if (!session_success) {
            return crow::response(500, "Failed to create session");
        }
        
        // Return session token to client
        crow::json::wvalue ret;
        ret["token"] = token;
        ret["user_id"] = user_id;
        ret["email"] = email;
        
        return crow::response(200, ret);
    }
    
    // Get Current User (GET /api/v1/auth/me)
    // Requires Authorization header with Bearer token
    crow::response get_current_user(const crow::request& req) {
        // Extract token from Authorization header
        std::string auth_header = "";
        for (const auto& header : req.headers) {
            if (header.first == "Authorization" || header.first == "authorization") {
                auth_header = header.second;
                break;
            }
        }
        
        if (auth_header.empty()) {
            return crow::response(401, "Missing Authorization header");
        }
        
        // Parse Bearer token
        std::string prefix = "Bearer ";
        if (auth_header.substr(0, prefix.length()) != prefix) {
            return crow::response(401, "Invalid Authorization header format");
        }
        
        std::string token = auth_header.substr(prefix.length());
        std::string token_hash = db_->hash_password(token);
        
        // Get user by session token
        auto [found, user_id, email] = db_->get_user_by_session_token(token_hash);
        
        if (!found) {
            return crow::response(401, "Invalid or expired token");
        }
        
        // Return user data
        crow::json::wvalue ret;
        ret["id"] = user_id;
        ret["email"] = email;
        
        return crow::response(200, ret);
    }
    
    // Logout (DELETE /api/v1/auth/logout)
    // Requires Authorization header with Bearer token
    crow::response logout(const crow::request& req) {
        // Extract token from Authorization header
        std::string auth_header = "";
        for (const auto& header : req.headers) {
            if (header.first == "Authorization" || header.first == "authorization") {
                auth_header = header.second;
                break;
            }
        }
        
        if (auth_header.empty()) {
            return crow::response(401, "Missing Authorization header");
        }
        
        // Parse Bearer token
        std::string prefix = "Bearer ";
        if (auth_header.substr(0, prefix.length()) != prefix) {
            return crow::response(401, "Invalid Authorization header format");
        }
        
        std::string token = auth_header.substr(prefix.length());
        std::string token_hash = db_->hash_password(token);
        
        // Delete session
        if (!db_->delete_session(token_hash)) {
            return crow::response(500, "Failed to logout");
        }
        
        return crow::response(204, ""); // 204 No Content
    }
    
    // Get Session Info (GET /api/v1/auth/session)
    crow::response get_session_info(const crow::request& req) {
        // Extract token from Authorization header
        std::string auth_header = "";
        for (const auto& header : req.headers) {
            if (header.first == "Authorization" || header.first == "authorization") {
                auth_header = header.second;
                break;
            }
        }
        
        if (auth_header.empty()) {
            return crow::response(401, "Missing Authorization header");
        }
        
        // Parse Bearer token
        std::string prefix = "Bearer ";
        if (auth_header.substr(0, prefix.length()) != prefix) {
            return crow::response(401, "Invalid Authorization header format");
        }
        
        std::string token = auth_header.substr(prefix.length());
        std::string token_hash = db_->hash_password(token);
        
        // Get user by session token
        auto [found, user_id, email] = db_->get_user_by_session_token(token_hash);
        
        if (!found) {
            return crow::response(401, "Invalid or expired token");
        }
        
        // Return session info
        crow::json::wvalue ret;
        ret["user_id"] = user_id;
        ret["email"] = email;
        
        return crow::response(200, ret);
    }

private:
    std::shared_ptr<IDatabase> db_;
    
    // Generate a secure random session token
    std::string generate_session_token() {
        static const std::string chars = 
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, chars.size() - 1);
        
        std::string token;
        token.reserve(32);
        
        for (int i = 0; i < 32; ++i) {
            token += chars[dis(gen)];
        }
        
        return token;
    }
};
