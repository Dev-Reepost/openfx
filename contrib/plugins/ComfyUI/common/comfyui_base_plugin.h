// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_BASE_PLUGIN_H
#define COMFYUI_BASE_PLUGIN_H

#include "ofxsImageEffect.h"
#include "comfyui_client.h"
#include <nlohmann/json.hpp>
#include <memory>

using json = nlohmann::json;

namespace ComfyUI {

/**
 * @brief Base class for all ComfyUI OFX plugins
 *
 * Provides common functionality for:
 * - Server connection management
 * - Workflow execution orchestration
 * - File I/O for image exchange
 * - Parameter management
 *
 * Derived classes must implement:
 * - buildWorkflow() - Construct the ComfyUI workflow JSON
 * - setupParameters() - Define plugin-specific parameters
 * - getRequiredModels() - List required AI models
 * - processOutput() - Handle ComfyUI results
 */
class BasePlugin : public OFX::ImageEffect {
protected:
    // Clips
    OFX::Clip *_srcClip;
    OFX::Clip *_dstClip;

    // Common parameters
    OFX::StringParam *_serverAddress;
    OFX::IntParam *_serverPort;
    OFX::StringParam *_sharedMountPath;
    OFX::StringParam *_projectName;

    // ComfyUI client
    std::unique_ptr<Client> _comfyClient;

    // Processing state
    enum State {
        Idle,
        Queuing,
        Processing,
        Completed,
        Error
    };
    State _state;

public:
    BasePlugin(OfxImageEffectHandle handle);
    virtual ~BasePlugin();

    // OFX lifecycle
    virtual void changedParam(const OFX::InstanceChangedArgs &args,
                             const std::string &paramName) override;
    virtual void render(const OFX::RenderArguments &args) override;
    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args,
                                      OfxRectD &rod) override;

    // Template method pattern - derived classes implement these
    virtual json buildWorkflow() = 0;
    virtual std::vector<std::string> getRequiredModels() = 0;

protected:
    // Helper methods
    void executeWorkflow(const OFX::RenderArguments &args);
    std::string writeInputImage(OFX::Image* img, int frame);
};

} // namespace ComfyUI

#endif // COMFYUI_BASE_PLUGIN_H
