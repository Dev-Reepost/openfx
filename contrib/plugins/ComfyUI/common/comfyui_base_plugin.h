// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_BASE_PLUGIN_H
#define COMFYUI_BASE_PLUGIN_H

#include "ofxsImageEffect.h"
#include "comfyui_client.h"
#include "async_job_manager.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

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
    OFX::StringParam *_macMountPath;           // macOS client mount path
    OFX::StringParam *_winMountPath;           // Windows server mount path (UNC, e.g., "\\\\192.168.1.110\\share")
    OFX::StringParam *_projectName;            // Project name for file organization (e.g., "my_commercial")
    OFX::StringParam *_workflowName;           // Workflow subdirectory (e.g., "segmentation")
    OFX::StringParam *_outputVersion;          // Output version (e.g., "v001")
    OFX::StringParam *_workflowFilePath;       // Path to workflow JSON file (supports bundle resources)
    OFX::BooleanParam *_enableCache;
    OFX::IntParam *_timeout;

    // Async rendering parameters
    OFX::ChoiceParam *_asyncMode;              // Blocking vs Non-blocking rendering
    OFX::ChoiceParam *_placeholderMode;        // What to show while processing
    OFX::DoubleParam *_refreshTrigger;         // Hidden parameter for cache invalidation
    OFX::StringParam *_jobStatus;              // Read-only job status display
    OFX::RGBParam *_jobStatusColor;            // Visual status indicator (color swatch)

    // Instance identification
    std::string _instanceName;                 // OFX instance name for auto-basename generation

    // ComfyUI client and thread safety
    std::unique_ptr<Client> _comfyClient;
    std::unique_ptr<AsyncJobManager> _jobManager;  // Async job management
    mutable std::mutex _renderMutex;               // Protect client access in concurrent renders

    // Cache optimization (avoid slow network file existence checks)
    mutable std::unordered_set<std::string> _cacheFileExists;  // Files known to exist
    mutable std::unordered_map<int, std::pair<int, int>> _cacheDimensions;  // Frame -> (width, height)
    mutable std::mutex _cacheMutex;                            // Protect cache access

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

    // Override in derived classes to control basename generation
    // Returns true for generic plugins (AnyComfy) where workflow name varies
    // Returns false for specialized plugins (SAMSegmentation) where workflow is fixed
    virtual bool includeWorkflowInBasename() const { return false; }

protected:
    // Helper methods - Blocking workflow execution
    void executeWorkflow(const OFX::RenderArguments &args);
    std::string writeInputImage(OFX::Image* img, int frame);
    void renderPassthrough(const OFX::RenderArguments &args);  // Fast passthrough for proxy renders
    void copyPixelData(const OFX::Image* src, OFX::Image* dst);
    std::string parseOutputPath(const json& history, int frame);
    std::string convertPathForComfyUI(const std::string& localPath);
    std::string constructExpectedOutputPath(int frame);
    std::string constructInputPath(int frame);  // Construct path to input EXR file
    std::string getEffectiveBasename();  // Get basename (auto-generated or manual)

    // Workflow file management
    std::string getBundleResourcePath(const std::string& resourceName);
    std::string resolveWorkflowPath(const std::string& workflowPath);
    json loadWorkflowFromFile(const std::string& filepath);
    json customizeWorkflow(const json& baseWorkflow, int frame, const std::string& inputPath);

    // Async rendering helper methods
    void renderBlocking(const OFX::RenderArguments &args);
    void renderAsync(const OFX::RenderArguments &args);
    void returnPlaceholder(const OFX::RenderArguments &args, int frame);
    void loadCachedResult(const OFX::RenderArguments &args, const std::string& cachedPath);
    void onJobComplete(int frame, bool success);
    void updateJobStatusDisplay();

    // Placeholder rendering modes
    void renderCheckerboard(OFX::Image* dst);
    void renderSolidColor(OFX::Image* dst, double r, double g, double b);
    int findLastValidFrame(double currentTime);

public:
    // Static methods for parameter definition (called by derived plugin factories)
    static void describeCommonParameters(OFX::ImageEffectDescriptor &desc,
                                         OFX::ContextEnum context,
                                         OFX::PageParamDescriptor *projectPage,
                                         OFX::PageParamDescriptor *processingPage,
                                         OFX::PageParamDescriptor *serverPage,
                                         const json* configDefaults = nullptr);

    // Configuration file management (public for factory access)
    static json loadConfigDefaults();
};

} // namespace ComfyUI

#endif // COMFYUI_BASE_PLUGIN_H
