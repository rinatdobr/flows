#pragma once
#include "IDatabase.hpp"
#include "crow.h"
#include <pqxx/pqxx>
#include <memory>
#include <vector>
#include <string>

// Test-friendly approach: pass database interface as dependency
class FlowsApi {
public:
    explicit FlowsApi(std::shared_ptr<IDatabase> db) : db_(std::move(db)) {}
    
    // CREATE (POST /api/v1/flows)
    // Root flow: POST /api/v1/flows with owner_id
    // Nested flow: POST /api/v1/flows with parent_flow_id (no owner_id needed, item is created automatically)
    crow::response create_flow(const crow::request& req, const std::string& parent_flow_id = "") {
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
            pqxx::work txn(*db_->connection());
            
            // If parent_flow_id is provided, this is a nested flow
            if (!parent_flow_id.empty()) {
                // Check if parent flow exists
                pqxx::result parent_check = txn.exec_params(
                    "SELECT id FROM flows WHERE id = $1;", parent_flow_id.c_str()
                );
                if (parent_check.empty()) {
                    return crow::response(404, "Parent flow not found");
                }
                
                // Insert flow without owner_id (for nested flows)
                pqxx::result r = txn.exec_params(
                    "INSERT INTO flows (name, description, start_at, end_at) VALUES ($1, $2, $3, $4) "
                    "RETURNING id, name, description, created_at::text, start_at::text, end_at::text;",
                    name.c_str(), desc.c_str(),
                    (has_start_at ? start_at_str.c_str() : nullptr),
                    (has_end_at ? end_at_str.c_str() : nullptr)
                );
                txn.commit();
                
                std::string flow_id = r[0]["id"].c_str();
                
                // Get max order_id for parent flow and increment
                pqxx::result order_check = txn.exec_params(
                    "SELECT COALESCE(MAX(order_id), -1) as max_order FROM items WHERE flow_id = $1;",
                    parent_flow_id.c_str()
                );
                int new_order_id = 0;
                if (!order_check.empty()) {
                    new_order_id = std::stoi(order_check[0]["max_order"].c_str()) + 1;
                }
                
                // Insert item binding child flow to parent
                txn.exec_params(
                    "INSERT INTO items (flow_id, item_type, item_id, order_id) VALUES ($1, 'flow', $2, $3) RETURNING id;",
                    parent_flow_id.c_str(), flow_id.c_str(), new_order_id
                );
                txn.commit();
                
                crow::json::wvalue ret;
                ret["id"] = flow_id;
                ret["name"] = r[0]["name"].c_str();
                ret["description"] = r[0]["description"].c_str();
                ret["created_at"] = r[0]["created_at"].c_str();
                ret["start_at"] = r[0]["start_at"].c_str();
                ret["end_at"] = r[0]["end_at"].c_str();
                // Note: nested flows don't have owner_id
                
                return crow::response(201, ret);
            } else {
                // Root flow - requires owner_id
                if (!json.has("owner_id")) {
                    return crow::response(400, "Missing required field: owner_id");
                }
                
                std::string owner_id = json["owner_id"].s();
                
                pqxx::result r = txn.exec_params(
                    "INSERT INTO flows (name, description, start_at, end_at, owner_id) VALUES ($1, $2, $3, $4, $5) "
                    "RETURNING id, name, description, created_at::text, start_at::text, end_at::text, owner_id::text;",
                    name.c_str(), desc.c_str(), 
                    (has_start_at ? start_at_str.c_str() : nullptr),
                    (has_end_at ? end_at_str.c_str() : nullptr),
                    owner_id.c_str()
                );
                txn.commit();

                crow::json::wvalue ret;
                ret["id"] = r[0]["id"].c_str();
                ret["name"] = r[0]["name"].c_str();
                ret["description"] = r[0]["description"].c_str();
                ret["created_at"] = r[0]["created_at"].c_str();
                ret["start_at"] = r[0]["start_at"].c_str();
                ret["end_at"] = r[0]["end_at"].c_str();
                ret["owner_id"] = r[0]["owner_id"].c_str();

                return crow::response(201, ret);
            }
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // READ (GET /api/v1/flows/<id>)
    crow::response get_flow(const std::string& id) {
        try {
            pqxx::nontransaction txn(*db_->connection());
            pqxx::result r = txn.exec_params(
                "SELECT id, name, description, created_at::text, start_at::text, end_at::text, owner_id::text FROM flows WHERE id = $1;", 
                id.c_str()
            );

            if (r.empty()) return crow::response(404);

            crow::json::wvalue ret;
            ret["id"] = r[0]["id"].c_str();
            ret["name"] = r[0]["name"].c_str();
            ret["description"] = r[0]["description"].c_str();
            ret["created_at"] = r[0]["created_at"].c_str();
            ret["start_at"] = r[0]["start_at"].c_str();
            ret["end_at"] = r[0]["end_at"].c_str();
            ret["owner_id"] = r[0]["owner_id"].c_str();

            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // READ ALL (GET /api/v1/flows)
    crow::response get_all_flows() {
        try {
            pqxx::nontransaction txn(*db_->connection());

            pqxx::result r = txn.exec(
                "SELECT id, name, description, created_at::text, start_at::text, end_at::text, owner_id::text FROM flows ORDER BY created_at DESC;"
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
                item["owner_id"] = row["owner_id"].c_str();
                flows_list.push_back(std::move(item));
            }

            crow::json::wvalue ret = std::move(flows_list);
            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // UPDATE (PUT /api/v1/flows/<id>)
    crow::response update_flow(const crow::request& req, const std::string& id) {
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
            pqxx::work txn(*db_->connection());
            pqxx::result r = txn.exec_params(
                "UPDATE flows SET name = $1, description = $2, start_at = $3, end_at = $4 WHERE id = $5 "
                "RETURNING id, name, description, created_at::text, start_at::text, end_at::text, owner_id::text;",
                name.c_str(), desc.c_str(), 
                (has_start_at ? start_at_str.c_str() : nullptr),
                (has_end_at ? end_at_str.c_str() : nullptr),
                id.c_str()
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
            ret["owner_id"] = r[0]["owner_id"].c_str();

            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // DELETE (DELETE /api/v1/flows/<id>)
    crow::response delete_flow(const std::string& id) {
        try {
            pqxx::work txn(*db_->connection());
            pqxx::result r = txn.exec_params("DELETE FROM flows WHERE id = $1 RETURNING id;", id.c_str());
            txn.commit();

            if (r.empty()) return crow::response(404);

            return crow::response(204); // 204 No Content
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // ==========================================
    // ITEMS API METHODS
    // ==========================================
    
    // GET items in a flow (GET /api/v1/flows/<flow_id>/items)
    crow::response get_flow_items(const std::string& flow_id) {
        try {
            pqxx::nontransaction txn(*db_->connection());
            
            // Check if flow exists first
            pqxx::result flow_check = txn.exec_params(
                "SELECT id FROM flows WHERE id = $1;", flow_id.c_str()
            );
            if (flow_check.empty()) {
                return crow::response(404, "Flow not found");
            }
            
            // Get all items in the flow ordered by order_id
            pqxx::result r = txn.exec_params(
                R"(SELECT id, flow_id, item_type, item_id, order_id, created_at::text 
                   FROM items 
                   WHERE flow_id = $1 
                   ORDER BY order_id ASC;)",
                flow_id.c_str()
            );
            
            crow::json::wvalue::list items_list;
            for (auto row : r) {
                crow::json::wvalue item;
                item["id"] = row["id"].c_str();
                item["flow_id"] = row["flow_id"].c_str();
                item["item_type"] = row["item_type"].c_str();
                item["item_id"] = row["item_id"].c_str();
                item["order_id"] = std::stoi(row["order_id"].c_str());
                item["created_at"] = row["created_at"].c_str();
                items_list.push_back(std::move(item));
            }
            
            crow::json::wvalue ret = std::move(items_list);
            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // UPDATE item order (PATCH /api/v1/flows/<flow_id>/items/<item_id>)
    crow::response update_item_order(const crow::request& req, const std::string& flow_id, const std::string& item_id) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("order_id")) {
            return crow::response(400, "Missing required field: order_id");
        }
        
        int new_order_id = json["order_id"].i();
        
        try {
            pqxx::work txn(*db_->connection());
            
            // Check if item exists and belongs to flow
            pqxx::result item_check = txn.exec_params(
                "SELECT id, order_id FROM items WHERE id = $1 AND flow_id = $2;",
                item_id.c_str(), flow_id.c_str()
            );
            if (item_check.empty()) {
                return crow::response(404, "Item not found in this flow");
            }
            
            int current_order_id = std::stoi(item_check[0]["order_id"].c_str());
            
            // Update the order_id
            pqxx::result r = txn.exec_params(
                "UPDATE items SET order_id = $1 WHERE id = $2 RETURNING id, flow_id, item_type, item_id, created_at::text;",
                new_order_id, item_id.c_str()
            );
            txn.commit();
            
            // Shift other items' order_ids if needed
            // If new_order_id < current_order_id, shift items in between up by 1
            // If new_order_id > current_order_id, shift items in between down by 1
            if (new_order_id < current_order_id) {
                txn.exec_params(
                    "UPDATE items SET order_id = order_id + 1 WHERE flow_id = $1 AND order_id >= $2 AND order_id < $3 AND id != $4;",
                    flow_id.c_str(), new_order_id, current_order_id, item_id.c_str()
                );
            } else if (new_order_id > current_order_id) {
                txn.exec_params(
                    "UPDATE items SET order_id = order_id - 1 WHERE flow_id = $1 AND order_id > $2 AND order_id <= $3 AND id != $4;",
                    flow_id.c_str(), current_order_id, new_order_id, item_id.c_str()
                );
            }
            txn.commit();
            
            crow::json::wvalue ret;
            ret["id"] = r[0]["id"].c_str();
            ret["flow_id"] = r[0]["flow_id"].c_str();
            ret["item_type"] = r[0]["item_type"].c_str();
            ret["item_id"] = r[0]["item_id"].c_str();
            ret["order_id"] = new_order_id;
            ret["created_at"] = r[0]["created_at"].c_str();
            
            return crow::response(200, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // ==========================================
    // TASK API METHODS
    // ==========================================
    
    // CREATE task (POST /api/v1/flows/<flow_id>/tasks)
    // Task must be created and bound to a flow via item
    crow::response create_task(const crow::request& req, const std::string& flow_id) {
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
            pqxx::work txn(*db_->connection());
            pqxx::result r = txn.exec_params(
                "INSERT INTO tasks (name, description, start_at, end_at) VALUES ($1, $2, $3, $4) "
                "RETURNING id, name, description, created_at::text, start_at::text, end_at::text;",
                name.c_str(), desc.c_str(),
                (has_start_at ? start_at_str.c_str() : nullptr),
                (has_end_at ? end_at_str.c_str() : nullptr)
            );
            txn.commit();
            
            std::string task_id = r[0]["id"].c_str();
            
            // Check if flow exists
            pqxx::result flow_check = txn.exec_params(
                "SELECT id FROM flows WHERE id = $1;", flow_id.c_str()
            );
            if (flow_check.empty()) {
                return crow::response(404, "Flow not found");
            }
            
            // Get max order_id for this flow and increment
            pqxx::result order_check = txn.exec_params(
                "SELECT COALESCE(MAX(order_id), -1) as max_order FROM items WHERE flow_id = $1;",
                flow_id.c_str()
            );
            int new_order_id = 0;
            if (!order_check.empty()) {
                new_order_id = std::stoi(order_check[0]["max_order"].c_str()) + 1;
            }
            
            // Insert item binding task to flow
            txn.exec_params(
                "INSERT INTO items (flow_id, item_type, item_id, order_id) VALUES ($1, 'task', $2, $3) RETURNING id;",
                flow_id.c_str(), task_id.c_str(), new_order_id
            );
            txn.commit();
            
            crow::json::wvalue ret;
            ret["id"] = task_id;
            ret["name"] = r[0]["name"].c_str();
            ret["description"] = r[0]["description"].c_str();
            ret["created_at"] = r[0]["created_at"].c_str();
            ret["start_at"] = r[0]["start_at"].c_str();
            ret["end_at"] = r[0]["end_at"].c_str();
            
            return crow::response(201, ret);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }
    
    // READ task (GET /api/v1/tasks/<id>)
    crow::response get_task(const std::string& id) {
        try {
            pqxx::nontransaction txn(*db_->connection());
            pqxx::result r = txn.exec_params(
                "SELECT id, name, description, created_at::text, start_at::text, end_at::text FROM tasks WHERE id = $1;", 
                id.c_str()
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
    }
    
    // UPDATE task (PUT /api/v1/tasks/<id>)
    crow::response update_task(const crow::request& req, const std::string& id) {
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
            pqxx::work txn(*db_->connection());
            pqxx::result r = txn.exec_params(
                "UPDATE tasks SET name = $1, description = $2, start_at = $3, end_at = $4 WHERE id = $5 "
                "RETURNING id, name, description, created_at::text, start_at::text, end_at::text;",
                name.c_str(), desc.c_str(),
                (has_start_at ? start_at_str.c_str() : nullptr),
                (has_end_at ? end_at_str.c_str() : nullptr),
                id.c_str()
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
    }
    
    // DELETE task (DELETE /api/v1/tasks/<id>)
    crow::response delete_task(const std::string& id) {
        try {
            pqxx::work txn(*db_->connection());
            pqxx::result r = txn.exec_params("DELETE FROM tasks WHERE id = $1 RETURNING id;", id.c_str());
            txn.commit();
            
            if (r.empty()) return crow::response(404);
            
            return crow::response(204); // 204 No Content
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    }

private:
    std::shared_ptr<IDatabase> db_;
};
