// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_CLIENT_H
#define COMFYUI_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace ComfyUI {

/**
 * @brief REST client for ComfyUI server communication
 *
 * Handles HTTP/WebSocket communication with ComfyUI server for:
 * - Submitting workflows
 * - Monitoring execution status
 * - Retrieving generated images
 * - Managing models
 */
class Client {
public:
    Client(const std::string& serverAddress);
    ~Client();

    // Connection management
    bool testConnection();
    void setServerAddress(const std::string& address);
    std::string getServerAddress() const;

    // Workflow execution
    std::string queuePrompt(const json& workflow, const std::string& clientId);
    json getHistory(const std::string& promptId);
    bool interruptExecution(const std::string& clientId);

    // File I/O paths
    void setInputDirectory(const std::string& path);
    void setOutputDirectory(const std::string& path);
    std::string getInputDirectory() const;
    std::string getOutputDirectory() const;

    // Model management
    std::vector<std::string> findModels(const std::string& modelType);

    // Client ID
    std::string getClientId() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    std::string generateClientId();
};

} // namespace ComfyUI

#endif // COMFYUI_CLIENT_H
