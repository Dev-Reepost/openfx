// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_BASE_PLUGIN_H
#define COMFYUI_BASE_PLUGIN_H

#include "ofxsImageEffect.h"
#include "comfyui_client.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <mutex>

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
    OFX::BooleanParam *_enableProcessing;      // Master enable/disable for ComfyUI processing
    OFX::StringParam *_serverAddress;
    OFX::IntParam *_serverPort;
    OFX::StringParam *_sharedMountPath;        // Client-side mount (Mac/Linux path)
    OFX::StringParam *_serverMountPoint;       // Server-side mount (Windows drive letter, e.g., "Z:")
    OFX::StringParam *_projectName;            // Project name for file organization (e.g., "my_commercial")
    OFX::StringParam *_workflowName;           // Workflow subdirectory (e.g., "segmentation")
    OFX::StringParam *_outputVersion;          // Output version (e.g., "v001")
    OFX::BooleanParam *_enableCache;
    OFX::IntParam *_timeout;

    // Instance identification
    std::string _instanceName;                 // OFX instance name for auto-basename generation

    // ComfyUI client and thread safety
    std::unique_ptr<Client> _comfyClient;
    mutable std::mutex _renderMutex;       // Protect client access in concurrent renders

    // Logging
    std::shared_ptr<spdlog::logger> _logger;

    void initializeLogger();

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
    virtual json buildWorkflow(int frame, const std::string& inputPath) = 0;
    virtual std::vector<std::string> getRequiredModels() = 0;

protected:
    // Helper methods
    void executeWorkflow(const OFX::RenderArguments &args);
    std::string writeInputImage(OFX::Image* img, int frame);
    void copyPixelData(const OFX::Image* src, OFX::Image* dst);
    std::string parseOutputPath(const json& history, int frame);
    std::string convertPathForComfyUI(const std::string& localPath);
    std::string constructExpectedOutputPath(int frame);
    std::string getEffectiveBasename();  // Get basename (auto-generated or manual)

public:
    // Static methods for parameter definition (called by derived plugin factories)
    static void describeCommonParameters(OFX::ImageEffectDescriptor &desc,
                                         OFX::ContextEnum context,
                                         OFX::PageParamDescriptor *projectPage,
                                         OFX::PageParamDescriptor *processingPage,
                                         OFX::PageParamDescriptor *serverPage);
};

} // namespace ComfyUI

#endif // COMFYUI_BASE_PLUGIN_H
