// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "comfyui_client.h"
#include <httplib.h>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>

// WebSocket client type
typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

namespace ComfyUI {

// PIMPL implementation
class Client::Impl {
public:
    std::string hostname;
    int port;
    std::string inputDir;
    std::string outputDir;
    std::string clientId;

    Impl(const std::string& addr) {
        parseServerAddress(addr);
    }

    void parseServerAddress(const std::string& addr) {
        // Parse "hostname:port" format
        size_t colonPos = addr.find(':');
        if (colonPos != std::string::npos) {
            hostname = addr.substr(0, colonPos);
            port = std::stoi(addr.substr(colonPos + 1));
        } else {
            hostname = addr;
            port = 8188; // Default ComfyUI port
        }
    }

    std::string getServerAddress() const {
        return hostname + ":" + std::to_string(port);
    }
};

Client::Client(const std::string& serverAddress)
    : m_impl(std::make_unique<Impl>(serverAddress))
{
    m_impl->clientId = generateClientId();
}

Client::~Client() = default;

bool Client::testConnection() {
    try {
        httplib::Client client(m_impl->hostname, m_impl->port);
        client.set_connection_timeout(5, 0); // 5 seconds

        // Try to GET /system_stats or root endpoint
        auto res = client.Get("/");

        if (res && (res->status == 200 || res->status == 404)) {
            // 404 is OK - server responded, just no root handler
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "ComfyUI connection test failed: " << e.what() << std::endl;
        return false;
    }
}

void Client::setServerAddress(const std::string& address) {
    m_impl->parseServerAddress(address);
}

std::string Client::getServerAddress() const {
    return m_impl->getServerAddress();
}

std::string Client::queuePrompt(const json& workflow, const std::string& clientId) {
    try {
        httplib::Client client(m_impl->hostname, m_impl->port);
        client.set_connection_timeout(10, 0); // 10 seconds

        // Build request payload matching ComfyUI API format
        json payload = {
            {"prompt", workflow},
            {"client_id", clientId}
        };

        std::string body = payload.dump();

        // POST to /prompt endpoint
        auto res = client.Post("/prompt", body, "application/json");

        if (!res) {
            throw std::runtime_error("Failed to connect to ComfyUI server at " +
                                   m_impl->getServerAddress());
        }

        if (res->status != 200) {
            throw std::runtime_error("ComfyUI server returned error: " +
                                   std::to_string(res->status) + " - " + res->body);
        }

        // Parse response to get prompt_id
        json response = json::parse(res->body);

        if (response.contains("prompt_id")) {
            return response["prompt_id"].get<std::string>();
        } else if (response.contains("error")) {
            throw std::runtime_error("ComfyUI error: " +
                                   response["error"].dump());
        }

        throw std::runtime_error("Unexpected response from ComfyUI server");

    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("JSON error: ") + e.what());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("ComfyUI queue error: ") + e.what());
    }
}

json Client::getHistory(const std::string& promptId) {
    try {
        httplib::Client client(m_impl->hostname, m_impl->port);
        client.set_connection_timeout(10, 0);

        // GET /history/{prompt_id}
        std::string endpoint = "/history/" + promptId;
        auto res = client.Get(endpoint.c_str());

        if (!res) {
            throw std::runtime_error("Failed to get history from ComfyUI server");
        }

        if (res->status != 200) {
            throw std::runtime_error("ComfyUI history request failed: " +
                                   std::to_string(res->status));
        }

        // Parse and return FULL history response
        // Response format: {prompt_id: {outputs: {...}, status: {...}, prompt: {...}}}
        json history = json::parse(res->body);

        // Return the full history object so caller can check if promptId exists
        return history;

    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Get history error: ") + e.what());
    }
}

bool Client::interruptExecution(const std::string& clientId) {
    try {
        httplib::Client client(m_impl->hostname, m_impl->port);
        client.set_connection_timeout(5, 0);

        // POST to /interrupt
        json payload = {{"client_id", clientId}};
        std::string body = payload.dump();

        auto res = client.Post("/interrupt", body, "application/json");

        return (res && res->status == 200);

    } catch (const std::exception& e) {
        std::cerr << "Interrupt execution failed: " << e.what() << std::endl;
        return false;
    }
}

void Client::setInputDirectory(const std::string& path) {
    m_impl->inputDir = path;
}

void Client::setOutputDirectory(const std::string& path) {
    m_impl->outputDir = path;
}

std::string Client::getInputDirectory() const {
    return m_impl->inputDir;
}

std::string Client::getOutputDirectory() const {
    return m_impl->outputDir;
}

std::vector<std::string> Client::findModels(const std::string& modelType) {
    // Model discovery would require filesystem access or ComfyUI API endpoint
    // For now, return empty - will be populated from UI or config
    // TODO: Implement via ComfyUI /object_info endpoint or filesystem scan
    return {};
}

std::string Client::getClientId() const {
    return m_impl->clientId;
}

std::string Client::generateClientId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "ofx_client_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

void Client::monitorExecution(const std::string& promptId, EventCallback callback) {
    try {
        // Create WebSocket client
        ws_client client;

        // Set up logging (minimal)
        client.set_access_channels(websocketpp::log::alevel::none);
        client.set_error_channels(websocketpp::log::elevel::none);

        // Initialize ASIO
        client.init_asio();

        // Track completion state
        bool completed = false;
        bool error_occurred = false;
        std::string error_message;
        std::mutex mtx;
        std::condition_variable cv;

        // Message handler
        client.set_message_handler([&](websocketpp::connection_hdl hdl, ws_client::message_ptr msg) {
            try {
                json message = json::parse(msg->get_payload());
                std::string type = message["type"];

                if (type == "status") {
                    // Queue status update
                    if (callback) {
                        callback(EventType::Status, message["data"]);
                    }
                }
                else if (type == "executing") {
                    // Node execution update
                    json data = message["data"];
                    std::string node = data.contains("node") && !data["node"].is_null()
                        ? data["node"].get<std::string>()
                        : "";

                    if (node.empty() && data["prompt_id"] == promptId) {
                        // Execution completed (node is null)
                        if (callback) {
                            callback(EventType::Completed, data);
                        }
                        std::lock_guard<std::mutex> lock(mtx);
                        completed = true;
                        cv.notify_all();
                    } else if (!node.empty()) {
                        // Node executing
                        if (callback) {
                            callback(EventType::Executing, data);
                        }
                    }
                }
                else if (type == "progress") {
                    // Progress update
                    if (callback) {
                        callback(EventType::Progress, message["data"]);
                    }
                }
                else if (type == "execution_error") {
                    // Execution error
                    if (callback) {
                        callback(EventType::ExecutionError, message["data"]);
                    }
                    std::lock_guard<std::mutex> lock(mtx);
                    error_occurred = true;
                    error_message = message["data"].dump();
                    cv.notify_all();
                }
                else if (type == "execution_cached") {
                    // Result from cache
                    if (callback) {
                        callback(EventType::ExecutionCached, message["data"]);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "WebSocket message parsing error: " << e.what() << std::endl;
            }
        });

        // Open handler
        client.set_open_handler([&](websocketpp::connection_hdl hdl) {
            // Connection established
        });

        // Fail handler
        client.set_fail_handler([&](websocketpp::connection_hdl hdl) {
            std::lock_guard<std::mutex> lock(mtx);
            error_occurred = true;
            error_message = "WebSocket connection failed";
            cv.notify_all();
        });

        // Build WebSocket URL: ws://hostname:port/ws?clientId=xxx
        std::stringstream url;
        url << "ws://" << m_impl->hostname << ":" << m_impl->port
            << "/ws?clientId=" << m_impl->clientId;

        // Connect
        websocketpp::lib::error_code ec;
        ws_client::connection_ptr con = client.get_connection(url.str(), ec);
        if (ec) {
            throw std::runtime_error("WebSocket connection failed: " + ec.message());
        }

        client.connect(con);

        // Run in separate thread
        std::thread ws_thread([&client]() {
            client.run();
        });

        // Wait for completion or error
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]{ return completed || error_occurred; });

        // Close connection
        client.stop();
        if (ws_thread.joinable()) {
            ws_thread.join();
        }

        // Check for errors
        if (error_occurred) {
            throw std::runtime_error("ComfyUI execution error: " + error_message);
        }

    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("WebSocket monitoring error: ") + e.what());
    }
}

} // namespace ComfyUI
