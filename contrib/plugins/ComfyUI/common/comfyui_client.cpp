// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "comfyui_client.h"
#include <httplib.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

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

        // Parse and return history
        json history = json::parse(res->body);

        // History response format: {prompt_id: {outputs: {...}, status: {...}}}
        if (history.contains(promptId)) {
            return history[promptId];
        }

        return json::object();

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

} // namespace ComfyUI
