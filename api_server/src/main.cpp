#include "crow.h"
#include "DbPool.hpp"
#include <pqxx/pqxx>
#include <iostream>

int main() {
    // 1. Initialize DB Pool
    std::string conn_str = "host=postgres port=5432 dbname=flows_db user=flows_db_admin password=an_admin_test_password";
    DbPool::instance().init(conn_str, 16); // 16 connections for 16 threads

    crow::SimpleApp app;

    // ==========================================
    // CREATE (POST /api/v1/flows)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("name") || !json.has("description")) {
            return crow::response(400, "Missing required fields: name, description");
        }

        std::string name = json["name"].s();
        std::string desc = json["description"].s();
        
        // Handle optional start_at and end_at fields
        std::string start_at_str = "";
        std::string end_at_str = "";
        bool has_start_at = json.has("start_at") && json["start_at"].s().size() > 0;
        bool has_end_at = json.has("end_at") && json["end_at"].s().size() > 0;
        
        if (has_start_at) start_at_str = json["start_at"].s();
        if (has_end_at) end_at_str = json["end_at"].s();
        
        // Validate: end_at can't be earlier than start_at but can be equal
        if (has_start_at && has_end_at) {
            if (end_at_str < start_at_str) {
                return crow::response(400, "Error: End datetime must be the same or after start datetime");
            }
        }

        try {
            auto conn_guard = DbPool::instance().acquire();
            pqxx::work txn(conn_guard.get());

            pqxx::result r = txn.exec_params(
                "INSERT INTO flows (name, description, start_at, end_at) VALUES ($1, $2, $3, $4) "
                "RETURNING id, name, description, created_at::text, start_at::text, end_at::text;",
                name, desc, 
                (has_start_at ? start_at_str.c_str() : nullptr),
                (has_end_at ? end_at_str.c_str() : nullptr)
            );
            txn.commit();

            crow::json::wvalue ret;
            ret["id"] = r[0]["id"].c_str();
            ret["name"] = r[0]["name"].c_str();
            ret["description"] = r[0]["description"].c_str();
            ret["created_at"] = r[0]["created_at"].c_str();
            ret["start_at"] = r[0]["start_at"].c_str();
            ret["end_at"] = r[0]["end_at"].c_str();

            return crow::response(201, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // ==========================================
    // READ (GET /api/v1/flows/<id>)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows/<string>").methods(crow::HTTPMethod::GET)([](const std::string& id) {
        try {
            auto conn_guard = DbPool::instance().acquire();
            pqxx::nontransaction read_tx(conn_guard.get()); // nontransaction is faster for pure reads

            pqxx::result r = read_tx.exec_params(
                "SELECT id, name, description, created_at::text, start_at::text, end_at::text FROM flows WHERE id = $1;", 
                id
            );

            if (r.empty()) return crow::response(404);

            crow::json::wvalue ret;
            ret["id"] = r[0]["id"].c_str();
            ret["name"] = r[0]["name"].c_str();
            ret["description"] = r[0]["description"].c_str();
            ret["created_at"] = r[0]["created_at"].c_str();
            ret["start_at"] = r[0]["start_at"].c_str();
            ret["end_at"] = r[0]["end_at"].c_str();

            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });


    // ==========================================
    // READ ALL (GET /api/v1/flows)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows").methods(crow::HTTPMethod::GET)([]() {
        try {
            auto conn_guard = DbPool::instance().acquire();
            pqxx::nontransaction read_tx(conn_guard.get());

            pqxx::result r = read_tx.exec(
                "SELECT id, name, description, created_at::text, start_at::text, end_at::text FROM flows ORDER BY created_at DESC;"
            );

            crow::json::wvalue::list flows_list;
            for (auto row : r) {
                crow::json::wvalue item;
                item["id"] = row["id"].c_str();
                item["name"] = row["name"].c_str();
                item["description"] = row["description"].c_str();
                item["created_at"] = row["created_at"].c_str();
                item["start_at"] = row["start_at"].c_str();
                item["end_at"] = row["end_at"].c_str();
                flows_list.push_back(std::move(item));
            }

            crow::json::wvalue ret = std::move(flows_list);
            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // ==========================================
    // UPDATE (PUT /api/v1/flows/<id>)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows/<string>").methods(crow::HTTPMethod::PUT)([](const crow::request& req, const std::string& id) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("name") || !json.has("description")) {
            return crow::response(400, "Missing required fields: name, description");
        }

        std::string name = json["name"].s();
        std::string desc = json["description"].s();
        
        // Handle optional start_at and end_at fields
        std::string start_at_str = "";
        std::string end_at_str = "";
        bool has_start_at = json.has("start_at") && json["start_at"].s().size() > 0;
        bool has_end_at = json.has("end_at") && json["end_at"].s().size() > 0;
        
        if (has_start_at) start_at_str = json["start_at"].s();
        if (has_end_at) end_at_str = json["end_at"].s();
        
        // Validate: end_at can't be earlier than start_at but can be equal
        if (has_start_at && has_end_at) {
            if (end_at_str < start_at_str) {
                return crow::response(400, "Error: End datetime must be the same or after start datetime");
            }
        }

        try {
            auto conn_guard = DbPool::instance().acquire();
            pqxx::work txn(conn_guard.get());

            pqxx::result r = txn.exec_params(
                "UPDATE flows SET name = $1, description = $2, start_at = $3, end_at = $4 WHERE id = $5 "
                "RETURNING id, name, description, created_at::text, start_at::text, end_at::text;",
                name, desc, 
                (has_start_at ? start_at_str.c_str() : nullptr),
                (has_end_at ? end_at_str.c_str() : nullptr),
                id
            );
            txn.commit();

            if (r.empty()) return crow::response(404);

            crow::json::wvalue ret;
            ret["id"] = r[0]["id"].c_str();
            ret["name"] = r[0]["name"].c_str();
            ret["description"] = r[0]["description"].c_str();
            ret["created_at"] = r[0]["created_at"].c_str();
            ret["start_at"] = r[0]["start_at"].c_str();
            ret["end_at"] = r[0]["end_at"].c_str();

            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // ==========================================
    // DELETE (DELETE /api/v1/flows/<id>)
    // ==========================================
    CROW_ROUTE(app, "/api/v1/flows/<string>").methods(crow::HTTPMethod::DELETE)([](const std::string& id) {
        try {
            auto conn_guard = DbPool::instance().acquire();
            pqxx::work txn(conn_guard.get());

            pqxx::result r = txn.exec_params("DELETE FROM flows WHERE id = $1 RETURNING id;", id);
            txn.commit();

            if (r.empty()) return crow::response(404);

            return crow::response(204); // 204 No Content
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // 2. Start the Server
    std::cout << "Starting Crow server on port 8080...\n";
    app.port(8080).multithreaded().run();

    return 0;
}
