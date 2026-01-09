// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "comfyui_base_plugin.h"
#include "comfyui_image_io.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <thread>
#include <cstring>

namespace ComfyUI {

void BasePlugin::initializeLogger()
{
    try {
        // Check if HOME environment variable exists
        const char* home = getenv("HOME");
        if (!home) {
            std::cerr << "WARNING: HOME environment variable not set, logging disabled" << std::endl;
            return;
        }

        // Create daily log file (YYYYMMDD format - one log per day)
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm* now_tm = std::localtime(&now_c);

        char dateStamp[16];
        std::strftime(dateStamp, sizeof(dateStamp), "%Y%m%d", now_tm);

        std::string logPath = std::string(home) + "/comfyui_plugin_" +
                             std::string(dateStamp) + ".log";

        // Check if logger already exists (reuse same logger for the day)
        if (spdlog::get("comfyui_plugin")) {
            _logger = spdlog::get("comfyui_plugin");
        } else {
            // Create new logger for this day
            _logger = spdlog::basic_logger_mt("comfyui_plugin", logPath);
            _logger->set_level(spdlog::level::trace);  // Log everything
            _logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            _logger->flush_on(spdlog::level::info);  // Flush on every info message
        }

        if (_logger) {
            char timeStamp[32];
            std::strftime(timeStamp, sizeof(timeStamp), "%H:%M:%S", now_tm);
            _logger->info("=== ComfyUI Plugin Session Started at {} ===", timeStamp);
            _logger->info("Log file: {}", logPath);
        }
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        _logger.reset(); // Make sure logger is null if initialization failed
    } catch (const std::exception& ex) {
        std::cerr << "Log initialization exception: " << ex.what() << std::endl;
        _logger.reset();
    } catch (...) {
        std::cerr << "Unknown exception during log initialization" << std::endl;
        _logger.reset();
    }
}

BasePlugin::BasePlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _srcClip(nullptr)
    , _dstClip(nullptr)
    , _enableProcessing(nullptr)
    , _serverAddress(nullptr)
    , _serverPort(nullptr)
    , _sharedMountPath(nullptr)
    , _projectName(nullptr)
    , _workflowName(nullptr)
    , _outputVersion(nullptr)
    , _enableCache(nullptr)
    , _timeout(nullptr)
    , _instanceName("")
{
    // Initialize logger first
    initializeLogger();

    if (_logger) _logger->info("BasePlugin constructor started");

    // Try to get instance name from OFX properties (store for auto-basename generation)
    try {
        _instanceName = getPropertySet().propGetString(kOfxPropName, false);
        if (_logger && !_instanceName.empty()) {
            _logger->info("OFX Instance Name (kOfxPropName): {}", _instanceName);
        }
    } catch (...) {
        if (_logger) _logger->warn("Could not retrieve kOfxPropName from instance");
    }

    // Fall back to label if name is empty
    if (_instanceName.empty()) {
        try {
            _instanceName = getPropertySet().propGetString(kOfxPropLabel, false);
            if (_logger && !_instanceName.empty()) {
                _logger->info("OFX Instance Label (kOfxPropLabel): {}", _instanceName);
            }
        } catch (...) {
            if (_logger) _logger->warn("Could not retrieve kOfxPropLabel from instance");
        }
    }

    _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);

    // Log all available OFX context properties for testing
    if (_logger) {
        _logger->info("=======================================================");
        _logger->info("=== COMPREHENSIVE OFX ENVIRONMENT DISCOVERY ===");
        _logger->info("=======================================================");

        // Effect context
        try {
            OFX::ContextEnum ctx = getContext();
            std::string ctxStr = "UNKNOWN";
            if (ctx == OFX::eContextFilter) ctxStr = "Filter";
            else if (ctx == OFX::eContextGeneral) ctxStr = "General";
            else if (ctx == OFX::eContextGenerator) ctxStr = "Generator";
            else if (ctx == OFX::eContextTransition) ctxStr = "Transition";
            else if (ctx == OFX::eContextPaint) ctxStr = "Paint";
            else if (ctx == OFX::eContextRetimer) ctxStr = "Retimer";
            _logger->info("Effect Context: {}", ctxStr);
        } catch (...) { _logger->warn("Effect Context: NOT AVAILABLE"); }

        // Plugin handle properties
        _logger->info("--- Plugin Descriptor Properties ---");
        try {
            std::string pluginId = getPropertySet().propGetString(kOfxPluginPropFilePath, false);
            _logger->info("Plugin File Path: '{}'", pluginId);
        } catch (...) { _logger->warn("Plugin File Path: NOT AVAILABLE"); }

        // Instance properties
        _logger->info("--- Instance Properties ---");
        try {
            std::string name = getPropertySet().propGetString(kOfxPropName, false);
            _logger->info("Instance kOfxPropName: '{}'", name);
        } catch (...) { _logger->warn("Instance kOfxPropName: NOT AVAILABLE"); }

        try {
            std::string label = getPropertySet().propGetString(kOfxPropLabel, false);
            _logger->info("Instance kOfxPropLabel: '{}'", label);
        } catch (...) { _logger->warn("Instance kOfxPropLabel: NOT AVAILABLE"); }

        try {
            std::string shortLabel = getPropertySet().propGetString(kOfxPropShortLabel, false);
            _logger->info("Instance kOfxPropShortLabel: '{}'", shortLabel);
        } catch (...) { _logger->warn("Instance kOfxPropShortLabel: NOT AVAILABLE"); }

        try {
            std::string longLabel = getPropertySet().propGetString(kOfxPropLongLabel, false);
            _logger->info("Instance kOfxPropLongLabel: '{}'", longLabel);
        } catch (...) { _logger->warn("Instance kOfxPropLongLabel: NOT AVAILABLE"); }

        try {
            std::string type = getPropertySet().propGetString(kOfxPropType, false);
            _logger->info("Instance kOfxPropType: '{}'", type);
        } catch (...) { _logger->warn("Instance kOfxPropType: NOT AVAILABLE"); }

        // Effect Instance Handle (pointer - NOT stable across runs)
        try {
            void* instanceHandle = getPropertySet().propGetPointer(kOfxPropEffectInstance, false);
            if (instanceHandle) {
                std::ostringstream hexStr;
                hexStr << "0x" << std::hex << reinterpret_cast<uintptr_t>(instanceHandle);
                _logger->info("kOfxPropEffectInstance: {} (RUNTIME POINTER - not stable)", hexStr.str());
            } else {
                _logger->warn("kOfxPropEffectInstance: NULL");
            }
        } catch (...) { _logger->warn("kOfxPropEffectInstance: NOT AVAILABLE"); }

        // Instance Data Pointer (custom data - NOT stable across runs)
        try {
            void* instanceData = getPropertySet().propGetPointer(kOfxPropInstanceData, false);
            if (instanceData) {
                std::ostringstream hexStr;
                hexStr << "0x" << std::hex << reinterpret_cast<uintptr_t>(instanceData);
                _logger->info("kOfxPropInstanceData: {} (RUNTIME POINTER - not stable)", hexStr.str());
            } else {
                _logger->info("kOfxPropInstanceData: NULL (expected if not set)");
            }
        } catch (...) { _logger->warn("kOfxPropInstanceData: NOT AVAILABLE"); }

        // Project size/extent properties
        _logger->info("--- Project Properties ---");
        try {
            int projectWidth = getPropertySet().propGetInt(kOfxImageEffectPropProjectSize, 0, false);
            int projectHeight = getPropertySet().propGetInt(kOfxImageEffectPropProjectSize, 1, false);
            _logger->info("Project Size: {}x{}", projectWidth, projectHeight);
        } catch (...) { _logger->warn("Project Size: NOT AVAILABLE"); }

        try {
            int extentWidth = getPropertySet().propGetInt(kOfxImageEffectPropProjectExtent, 0, false);
            int extentHeight = getPropertySet().propGetInt(kOfxImageEffectPropProjectExtent, 1, false);
            _logger->info("Project Extent: {}x{}", extentWidth, extentHeight);
        } catch (...) { _logger->warn("Project Extent: NOT AVAILABLE"); }

        try {
            int offsetX = getPropertySet().propGetInt(kOfxImageEffectPropProjectOffset, 0, false);
            int offsetY = getPropertySet().propGetInt(kOfxImageEffectPropProjectOffset, 1, false);
            _logger->info("Project Offset: ({},{})", offsetX, offsetY);
        } catch (...) { _logger->warn("Project Offset: NOT AVAILABLE"); }

        try {
            double pixelAspect = getPropertySet().propGetDouble(kOfxImageEffectPropProjectPixelAspectRatio, false);
            _logger->info("Project Pixel Aspect Ratio: {}", pixelAspect);
        } catch (...) { _logger->warn("Project Pixel Aspect Ratio: NOT AVAILABLE"); }

        try {
            double duration = getPropertySet().propGetDouble(kOfxImageEffectInstancePropEffectDuration, false);
            _logger->info("Effect Duration: {}", duration);
        } catch (...) { _logger->warn("Effect Duration: NOT AVAILABLE"); }

        try {
            double frameRate = getPropertySet().propGetDouble(kOfxImageEffectPropFrameRate, false);
            _logger->info("Frame Rate: {}", frameRate);
        } catch (...) { _logger->warn("Frame Rate: NOT AVAILABLE"); }

        try {
            double frameMin = getPropertySet().propGetDouble(kOfxImageEffectPropFrameRange, 0, false);
            double frameMax = getPropertySet().propGetDouble(kOfxImageEffectPropFrameRange, 1, false);
            _logger->info("Frame Range: {} - {}", frameMin, frameMax);
        } catch (...) { _logger->warn("Frame Range: NOT AVAILABLE"); }

        // Source clip properties
        if (_srcClip) {
            _logger->info("--- Source Clip Properties ---");
            try {
                std::string clipName = _srcClip->getPropertySet().propGetString(kOfxPropName, false);
                _logger->info("Source Clip kOfxPropName: '{}'", clipName);
            } catch (...) { _logger->warn("Source Clip kOfxPropName: NOT AVAILABLE"); }

            try {
                std::string clipLabel = _srcClip->getPropertySet().propGetString(kOfxPropLabel, false);
                _logger->info("Source Clip kOfxPropLabel: '{}'", clipLabel);
            } catch (...) { _logger->warn("Source Clip kOfxPropLabel: NOT AVAILABLE"); }

            try {
                std::string clipConnected = _srcClip->isConnected() ? "YES" : "NO";
                _logger->info("Source Clip Connected: {}", clipConnected);
            } catch (...) { _logger->warn("Source Clip Connected: UNKNOWN"); }
        } else {
            _logger->warn("Source Clip: NULL");
        }

        // Output clip properties
        if (_dstClip) {
            _logger->info("--- Output Clip Properties ---");
            try {
                std::string clipName = _dstClip->getPropertySet().propGetString(kOfxPropName, false);
                _logger->info("Output Clip kOfxPropName: '{}'", clipName);
            } catch (...) { _logger->warn("Output Clip kOfxPropName: NOT AVAILABLE"); }

            try {
                std::string clipLabel = _dstClip->getPropertySet().propGetString(kOfxPropLabel, false);
                _logger->info("Output Clip kOfxPropLabel: '{}'", clipLabel);
            } catch (...) { _logger->warn("Output Clip kOfxPropLabel: NOT AVAILABLE"); }
        } else {
            _logger->warn("Output Clip: NULL");
        }

        // Environment/System info
        _logger->info("--- System Environment ---");
        try {
            const char* home = getenv("HOME");
            if (home) _logger->info("HOME: '{}'", home);
            else _logger->warn("HOME: NOT SET");
        } catch (...) { _logger->warn("HOME: ERROR"); }

        try {
            const char* user = getenv("USER");
            if (user) _logger->info("USER: '{}'", user);
            else _logger->warn("USER: NOT SET");
        } catch (...) { _logger->warn("USER: ERROR"); }

        try {
            const char* hostname = getenv("HOSTNAME");
            if (hostname) _logger->info("HOSTNAME: '{}'", hostname);
            else _logger->warn("HOSTNAME: NOT SET");
        } catch (...) { _logger->warn("HOSTNAME: ERROR"); }

        try {
            const char* pwd = getenv("PWD");
            if (pwd) _logger->info("PWD: '{}'", pwd);
            else _logger->warn("PWD: NOT SET");
        } catch (...) { _logger->warn("PWD: ERROR"); }

        // Additional render properties
        _logger->info("--- Render Properties ---");
        try {
            double scaleX = getPropertySet().propGetDouble(kOfxImageEffectPropRenderScale, 0, false);
            double scaleY = getPropertySet().propGetDouble(kOfxImageEffectPropRenderScale, 1, false);
            _logger->info("Render Scale: {}x{}", scaleX, scaleY);
        } catch (...) { _logger->warn("Render Scale: NOT AVAILABLE"); }

        try {
            int seqRender = getPropertySet().propGetInt(kOfxImageEffectInstancePropSequentialRender, false);
            _logger->info("Sequential Render: {}", seqRender ? "YES" : "NO");
        } catch (...) { _logger->warn("Sequential Render: NOT AVAILABLE"); }

        // Plugin capabilities
        _logger->info("--- Plugin Capabilities ---");
        try {
            int supportsTiles = getPropertySet().propGetInt(kOfxImageEffectPropSupportsTiles, false);
            _logger->info("Supports Tiles: {}", supportsTiles ? "YES" : "NO");
        } catch (...) { _logger->warn("Supports Tiles: NOT AVAILABLE"); }

        try {
            int supportsMultiRes = getPropertySet().propGetInt(kOfxImageEffectPropSupportsMultiResolution, false);
            _logger->info("Supports Multi-Resolution: {}", supportsMultiRes ? "YES" : "NO");
        } catch (...) { _logger->warn("Supports Multi-Resolution: NOT AVAILABLE"); }

        try {
            int temporalAccess = getPropertySet().propGetInt(kOfxImageEffectPropTemporalClipAccess, false);
            _logger->info("Temporal Clip Access: {}", temporalAccess ? "YES" : "NO");
        } catch (...) { _logger->warn("Temporal Clip Access: NOT AVAILABLE"); }

        // GPU Support (properties may not be available in all contexts)
        _logger->info("--- GPU Support ---");
        _logger->info("(GPU render properties require GPU rendering extensions)");

        // Clip format properties (Source clip)
        if (_srcClip && _srcClip->isConnected()) {
            _logger->info("--- Source Clip Format ---");
            try {
                std::string components = _srcClip->getPropertySet().propGetString(kOfxImageEffectPropComponents, false);
                _logger->info("Source Components: '{}'", components);
            } catch (...) { _logger->warn("Source Components: NOT AVAILABLE"); }

            try {
                std::string pixelDepth = _srcClip->getPropertySet().propGetString(kOfxImageEffectPropPixelDepth, false);
                _logger->info("Source Pixel Depth: '{}'", pixelDepth);
            } catch (...) { _logger->warn("Source Pixel Depth: NOT AVAILABLE"); }

            try {
                std::string preMultiplication = _srcClip->getPropertySet().propGetString(kOfxImageEffectPropPreMultiplication, false);
                _logger->info("Source PreMultiplication: '{}'", preMultiplication);
            } catch (...) { _logger->warn("Source PreMultiplication: NOT AVAILABLE"); }

            try {
                std::string fieldOrder = _srcClip->getPropertySet().propGetString(kOfxImageClipPropFieldOrder, false);
                _logger->info("Source Field Order: '{}'", fieldOrder);
            } catch (...) { _logger->warn("Source Field Order: NOT AVAILABLE"); }

            try {
                double frameRate = _srcClip->getPropertySet().propGetDouble(kOfxImageEffectPropFrameRate, false);
                _logger->info("Source Frame Rate: {}", frameRate);
            } catch (...) { _logger->warn("Source Frame Rate: NOT AVAILABLE"); }

            try {
                std::string unmappedComponents = _srcClip->getPropertySet().propGetString(kOfxImageClipPropUnmappedComponents, false);
                _logger->info("Source Unmapped Components: '{}'", unmappedComponents);
            } catch (...) { _logger->warn("Source Unmapped Components: NOT AVAILABLE"); }

            try {
                int optional = _srcClip->getPropertySet().propGetInt(kOfxImageClipPropOptional, false);
                _logger->info("Source Clip Optional: {}", optional ? "YES" : "NO");
            } catch (...) { _logger->warn("Source Clip Optional: NOT AVAILABLE"); }

            try {
                int isMask = _srcClip->getPropertySet().propGetInt(kOfxImageClipPropIsMask, false);
                _logger->info("Source Clip Is Mask: {}", isMask ? "YES" : "NO");
            } catch (...) { _logger->warn("Source Clip Is Mask: NOT AVAILABLE"); }

            try {
                int continuousSamples = _srcClip->getPropertySet().propGetInt(kOfxImageClipPropContinuousSamples, false);
                _logger->info("Source Continuous Samples: {}", continuousSamples ? "YES" : "NO");
            } catch (...) { _logger->warn("Source Continuous Samples: NOT AVAILABLE"); }
        }

        _logger->info("=======================================================");
        _logger->info("=== END COMPREHENSIVE OFX ENVIRONMENT DISCOVERY ===");
        _logger->info("=======================================================");
    }

    // Fetch common parameters
    _enableProcessing = fetchBooleanParam("enableProcessing");
    _serverAddress = fetchStringParam("serverAddress");
    _serverPort = fetchIntParam("serverPort");
    _sharedMountPath = fetchStringParam("sharedMountPath");
    _serverMountPoint = fetchStringParam("serverMountPoint");
    _projectName = fetchStringParam("projectName");
    _workflowName = fetchStringParam("workflowName");
    _outputVersion = fetchStringParam("outputVersion");
    _workflowFilePath = fetchStringParam("workflowFilePath");
    _enableCache = fetchBooleanParam("enableCache");
    _timeout = fetchIntParam("timeout");

    // Fetch async rendering parameters
    _asyncMode = fetchChoiceParam("asyncMode");
    _placeholderMode = fetchChoiceParam("placeholderMode");
    _refreshTrigger = fetchDoubleParam("refreshTrigger");
    _jobStatus = fetchStringParam("jobStatus");

    // Initialize project status indicator color based on initial project name state
    if (_projectName) {
        std::string projectName;
        _projectName->getValue(projectName);

        if (_logger) _logger->info("Initializing project status indicator. Project name is: '{}'", projectName);

        OFX::RGBParam *indicatorParam = fetchRGBParam("projectNameIndicator");
        if (indicatorParam) {
            if (_logger) _logger->info("Indicator parameter fetched successfully");

            if (projectName.empty()) {
                // RED when empty (warning)
                if (_logger) _logger->info("Setting indicator to RED (project is empty)");
                indicatorParam->setValue(1.0, 0.0, 0.0);
                if (_logger) _logger->info("Project indicator initialized: RED (project name is empty)");
            } else {
                // BLACK when set (normal)
                if (_logger) _logger->info("Setting indicator to BLACK (project is: '{}')", projectName);
                indicatorParam->setValue(0.0, 0.0, 0.0);
                if (_logger) _logger->info("Project indicator initialized: BLACK (project name: '{}')", projectName);
            }
        } else {
            if (_logger) _logger->error("FAILED to fetch projectNameIndicator parameter!");
        }
    } else {
        if (_logger) _logger->error("FAILED to fetch projectName parameter!");
    }

    // NOTE: Job manager will be initialized lazily when needed (after _comfyClient is created)
    // We can't create it here because _comfyClient might not exist yet

    if (_logger) _logger->info("BasePlugin constructor completed successfully");
}

BasePlugin::~BasePlugin()
{
    try {
        if (_logger) {
            _logger->info("=== ComfyUI Plugin Session Ended ===");
            _logger->flush();
            _logger.reset();
        }
    } catch (...) {
        // Silently ignore exceptions during destruction
    }
}

void BasePlugin::changedParam(const OFX::InstanceChangedArgs &args,
                              const std::string &paramName)
{
    if (_logger) _logger->info("Parameter changed: {}", paramName);

    // Update project status indicator color based on whether project is set
    if (paramName == "projectName") {
        std::string projectName;
        _projectName->getValue(projectName);

        // Get the color indicator parameter
        OFX::RGBParam *indicatorParam = fetchRGBParam("projectNameIndicator");
        if (indicatorParam) {
            if (projectName.empty()) {
                // RED when empty (warning)
                indicatorParam->setValue(1.0, 0.0, 0.0);
                if (_logger) _logger->info("Project indicator set to RED (project name is empty)");
            } else {
                // BLACK when set (normal)
                indicatorParam->setValue(0.0, 0.0, 0.0);
                if (_logger) _logger->info("Project indicator set to BLACK (project name: '{}')", projectName);
            }
        }
    }

    // Recreate ComfyUI client if server parameters change
    if (paramName == "serverAddress" || paramName == "serverPort") {
        // Get current values
        std::string address;
        _serverAddress->getValue(address);
        int port = _serverPort->getValue();

        if (_logger) _logger->info("Updating ComfyUI server to {}:{}", address, port);

        // Create new client with updated server info
        std::string serverUrl = address + ":" + std::to_string(port);
        _comfyClient.reset(new Client(serverUrl));

        if (_logger) _logger->info("ComfyUI client recreated successfully");
    }
}

void BasePlugin::render(const OFX::RenderArguments &args)
{
    int frame = static_cast<int>(args.time);

    if (_logger) {
        _logger->info("========================================");
        _logger->info("RENDER STARTED - Frame: {}", frame);
        _logger->info("Render window: ({},{}) to ({},{})",
                     args.renderWindow.x1, args.renderWindow.y1,
                     args.renderWindow.x2, args.renderWindow.y2);
        _logger->info("Render scale: {}", args.renderScale.x);
        _logger->info("Interactive: {}, Draft: {}, Sequential: {}",
                     args.interactiveRenderStatus,
                     args.renderQualityDraft,
                     args.sequentialRenderStatus);

        // Log runtime clip information
        if (_srcClip) {
            _logger->info("--- Runtime Source Clip Info ---");
            _logger->info("Source Clip Connected: {}", _srcClip->isConnected() ? "YES" : "NO");
            if (_srcClip->isConnected()) {
                try {
                    std::string clipName = _srcClip->getPropertySet().propGetString(kOfxPropName, false);
                    _logger->info("Source Clip Name: '{}'", clipName);
                } catch (...) {}
                try {
                    std::string clipLabel = _srcClip->getPropertySet().propGetString(kOfxPropLabel, false);
                    _logger->info("Source Clip Label: '{}'", clipLabel);
                } catch (...) {}
                try {
                    std::string components = _srcClip->getPixelComponentsProperty();
                    _logger->info("Source Clip Components: {}", components);
                } catch (...) {}
            }
        }

        _logger->info("========================================");
    }

    std::lock_guard<std::mutex> lock(_renderMutex);  // Thread-safe for concurrent renders

    // CRITICAL FIX: Detect proxy/preview renders (Resolve thumbnails, scrubbing, etc.)
    // For proxy renders, use fast passthrough instead of full ComfyUI workflow
    // This prevents crashes from NULL buffers and provides instant feedback
    bool isProxyRender = (args.renderScale.x < 1.0 || args.renderScale.y < 1.0);

    if (isProxyRender) {
        if (_logger) {
            _logger->info("=== PROXY/PREVIEW RENDER DETECTED ===");
            _logger->info("Render scale: {}x{} (< 1.0 = proxy mode)",
                         args.renderScale.x, args.renderScale.y);
            _logger->info("Using fast passthrough (no ComfyUI processing)");
            _logger->info("This is expected for thumbnails, timeline scrubbing, and draft previews");
        }

        renderPassthrough(args);

        if (_logger) {
            _logger->info("Proxy render completed successfully");
        }
        return;
    }

    if (_logger) {
        _logger->info("Full resolution render (scale: {}x{}) - will execute ComfyUI workflow",
                     args.renderScale.x, args.renderScale.y);
    }

    // Check if processing is enabled - if not, just passthrough
    bool processingEnabled = _enableProcessing->getValue();
    if (!processingEnabled) {
        if (_logger) _logger->info("ComfyUI processing DISABLED - passthrough mode");

        // Check if source is connected
        if (_srcClip && _srcClip->isConnected()) {
            std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
            std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));

            if (src.get() && dst.get()) {
                copyPixelData(src.get(), dst.get());
            }
        }
        return;
    }

    if (_logger) _logger->info("ComfyUI processing ENABLED");

    // Always use non-blocking mode (asyncModeValue = 1) - parameter is hidden
    int asyncModeValue = 1;
    // _asyncMode->getValue(asyncModeValue);  // No longer reading from parameter

    if (asyncModeValue == 0) {
        // BLOCKING MODE - traditional behavior
        if (_logger) _logger->info("Rendering in BLOCKING mode");
        renderBlocking(args);
    } else {
        // NON-BLOCKING MODE - async rendering with placeholder
        if (_logger) _logger->info("Rendering in NON-BLOCKING mode");
        renderAsync(args);
    }
}

bool BasePlugin::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args,
                                      OfxRectD &rod)
{
    if (_srcClip && _srcClip->isConnected()) {
        rod = _srcClip->getRegionOfDefinition(args.time);
        return true;
    }
    return false;
}

void BasePlugin::executeWorkflow(const OFX::RenderArguments &args)
{
    if (_logger) _logger->info("Starting workflow execution");

    // Log all parameters
    std::string address, mountPath, serverMount, project, workflowName, version;
    _serverAddress->getValue(address);
    int port = _serverPort->getValue();
    _sharedMountPath->getValue(mountPath);
    _serverMountPoint->getValue(serverMount);
    _projectName->getValue(project);
    _workflowName->getValue(workflowName);
    _outputVersion->getValue(version);

    // Get auto-generated basename
    std::string basename = getEffectiveBasename();

    if (_logger) {
        _logger->info("Parameters:");
        _logger->info("  Server: {}:{}", address, port);
        _logger->info("  Client Mount: {}", mountPath);
        _logger->info("  Server Mount: {}", serverMount);
        _logger->info("  Project: {}", project);
        _logger->info("  Workflow: {}", workflowName);
        _logger->info("  Basename: {} (auto-generated)", basename);
        _logger->info("  Version: {}", version);
    }

    // Initialize ComfyUI client if needed
    if (!_comfyClient) {
        if (_logger) _logger->info("Creating new ComfyUI client");
        std::string serverUrl = address + ":" + std::to_string(port);
        _comfyClient.reset(new Client(serverUrl));
        if (_logger) _logger->info("ComfyUI client created successfully");
    }

    // Get current frame number
    int frame = static_cast<int>(args.time);
    if (_logger) _logger->info("Processing frame: {}", frame);

    // Step 1: Write input image to shared storage
    if (_logger) _logger->info("Step 1: Writing input image to shared storage");
    progressUpdate(0.1);

    // Check if source clip is connected
    if (!_srcClip->isConnected()) {
        if (_logger) _logger->error("ERROR: Source clip is not connected!");
        throw std::runtime_error("Source clip is not connected. Please connect an input to the plugin.");
    }

    std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));

    if (!src.get()) {
        if (_logger) _logger->error("ERROR: Failed to fetch source image!");
        throw std::runtime_error("Failed to fetch source image from clip.");
    }

    if (_logger) _logger->info("Source image fetched successfully");
    if (_logger) _logger->info("Fetched source image from clip");

    std::string inputPath = writeInputImage(src.get(), frame);
    if (_logger) _logger->info("Input image written to: {}", inputPath);

    // Step 2: Check if output already exists (avoid overwrite error)
    if (_logger) _logger->info("Step 2: Checking if output file already exists");
    progressUpdate(0.15);
    std::string expectedOutputPath = constructExpectedOutputPath(frame);
    if (_logger) _logger->info("Expected output path: {}", expectedOutputPath);

    std::ifstream cachedFile(expectedOutputPath);
    if (cachedFile.good()) {
        cachedFile.close();
        if (_logger) _logger->info("✓ Output file already exists (cached): {}", expectedOutputPath);
        if (_logger) _logger->info("Skipping workflow submission, using cached result");

        // Read cached output directly
        progressUpdate(0.9);
        std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
        if (!dst.get()) {
            if (_logger) _logger->error("Failed to fetch destination image");
            throw std::runtime_error("Failed to fetch destination image");
        }

        ImageData outputImageData = ImageIO::readEXR(expectedOutputPath);

        // Convert back to OFX buffer
        progressUpdate(0.95);
        if (_logger) _logger->info("Converting cached image to OFX buffer");

        int dstPixelComponents = dst->getPixelComponentCount();
        OFX::BitDepthEnum dstBitDepth = dst->getPixelDepth();
        int dstBitDepthInt = 32;  // default to float
        if (dstBitDepth == OFX::eBitDepthUByte) dstBitDepthInt = 8;
        else if (dstBitDepth == OFX::eBitDepthUShort) dstBitDepthInt = 16;

        // Critical: Validate pixel data pointer before use
        void* dstPixelData = dst->getPixelData();
        if (!dstPixelData) {
            if (_logger) _logger->error("CRITICAL: Destination pixel data pointer is NULL (cached path)!");
            throw std::runtime_error("Failed to allocate destination image buffer for cached result");
        }

        ImageIO::toOFXBuffer(outputImageData, dstPixelData, dst->getRowBytes(), dstPixelComponents, dstBitDepthInt);
        if (_logger) _logger->info("Cached result loaded successfully");

        progressUpdate(1.0);
        return;
    }

    if (_logger) _logger->info("Output file does not exist, proceeding with workflow submission");

    // Step 3: Build workflow (implemented by derived class)
    if (_logger) _logger->info("Step 3: Building workflow JSON");
    progressUpdate(0.2);
    json workflow = buildWorkflow(frame, inputPath);
    if (_logger) {
        _logger->info("Workflow JSON built: {} characters", workflow.dump().length());
        _logger->info("Workflow JSON content:\n{}", workflow.dump(2));  // Pretty print with 2-space indent
    }

    // Step 4: Queue workflow to ComfyUI
    if (_logger) _logger->info("Step 4: Queueing workflow to ComfyUI server");
    progressUpdate(0.25);
    std::string clientId = _comfyClient->getClientId();
    if (_logger) _logger->info("Client ID: {}", clientId);

    std::string promptId = _comfyClient->queuePrompt(workflow, clientId);
    if (_logger) _logger->info("Workflow queued with prompt ID: {}", promptId);

    // Step 5: Wait for execution (using polling instead of WebSocket to avoid crashes)
    if (_logger) _logger->info("Step 5: Waiting for workflow execution (polling mode)");
    progressUpdate(0.3);

    // Poll for completion instead of using WebSocket
    const int maxAttempts = 300; // 5 minutes at 1 second intervals
    bool completed = false;
    std::string errorMsg;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        try {
            // Check history to see if workflow is done
            json history = _comfyClient->getHistory(promptId);

            // Log what we got back
            if (_logger && attempt == 0) {
                if (history.empty()) {
                    _logger->info("History response is empty");
                } else if (history.is_object()) {
                    _logger->info("History response has {} keys", history.size());
                    if (history.contains(promptId)) {
                        _logger->info("Found promptId {} in history!", promptId);
                    } else {
                        // Log what keys ARE present
                        std::vector<std::string> keys;
                        for (auto& el : history.items()) {
                            keys.push_back(el.key());
                            if (keys.size() >= 3) break;
                        }
                        if (!keys.empty()) {
                            _logger->info("History contains other prompts: {}", nlohmann::json(keys).dump());
                        }
                    }
                } else {
                    _logger->warn("History response is not an object: {}", history.dump().substr(0, 200));
                }
            }

            // When workflow is in history, it means it's done (either cached or executed)
            if (history.contains(promptId)) {
                auto& promptData = history[promptId];

                if (_logger) {
                    _logger->info("✓ Prompt {} found in history on attempt {}", promptId, attempt);
                    // Log the full prompt data to see structure
                    _logger->info("Full prompt data:\n{}", promptData.dump(2));
                }

                // Check status first
                if (promptData.contains("status")) {
                    auto& status = promptData["status"];

                    // Check status_str
                    if (status.contains("status_str")) {
                        std::string statusStr = status["status_str"].get<std::string>();
                        if (_logger) _logger->info("Status: {}", statusStr);

                        if (statusStr == "success") {
                            if (_logger) _logger->info("Workflow completed successfully");
                            completed = true;
                            break;
                        } else if (statusStr == "error") {
                            errorMsg = "Workflow execution failed";
                            if (status.contains("messages")) {
                                errorMsg += ": " + status["messages"].dump();
                            }
                            if (_logger) _logger->error(errorMsg);
                            break;
                        }
                    }

                    // Check completed flag
                    if (status.contains("completed") && status["completed"].get<bool>()) {
                        if (_logger) _logger->info("Workflow marked as completed");
                        completed = true;
                        break;
                    }
                }

                // If we have outputs, workflow is done (even if cached)
                if (promptData.contains("outputs") && !promptData["outputs"].is_null()) {
                    // Count number of output nodes
                    int outputCount = promptData["outputs"].size();
                    if (_logger) _logger->info("Workflow has {} output nodes - marking as completed", outputCount);

                    if (outputCount > 0) {
                        completed = true;
                        break;
                    }
                }
            }

            // Update progress periodically
            if (attempt % 10 == 0) {
                double progress = 0.3 + (0.5 * static_cast<double>(attempt) / maxAttempts);
                progressUpdate(progress);
                if (_logger && attempt > 0) {
                    _logger->info("Waiting for completion... ({}/{})", attempt, maxAttempts);
                }
            }

            // Sleep for 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));

        } catch (const std::exception& ex) {
            if (_logger) _logger->warn("Error polling history: {}", ex.what());
            // Continue polling
        }
    }

    if (!completed) {
        errorMsg = "Workflow execution timed out after " + std::to_string(maxAttempts) + " seconds";
        if (_logger) _logger->error(errorMsg);
    }

    if (!completed || !errorMsg.empty()) {
        throw std::runtime_error("ComfyUI execution failed: " + errorMsg);
    }

    // Step 6: Get history to find output files
    if (_logger) _logger->info("Step 6: Retrieving execution history from ComfyUI");
    progressUpdate(0.85);
    json history = _comfyClient->getHistory(promptId);
    if (_logger) _logger->info("History retrieved: {} characters", history.dump().length());

    // Step 7: Parse output path
    if (_logger) _logger->info("Step 7: Parsing output path from history");
    std::string outputPath = parseOutputPath(history, frame);

    if (outputPath.empty()) {
        if (_logger) _logger->error("Failed to find output file in ComfyUI history");
        throw std::runtime_error("Failed to find output file in ComfyUI history");
    }
    if (_logger) _logger->info("Output path found: {}", outputPath);

    // Step 8: Load result and copy to output buffer
    if (_logger) _logger->info("Step 8: Loading result image and copying to output buffer");
    progressUpdate(0.9);
    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    if (!dst.get()) {
        if (_logger) _logger->error("Failed to fetch destination image");
        throw std::runtime_error("Failed to fetch destination image");
    }
    if (_logger) _logger->info("Destination image buffer allocated");

    if (_logger) _logger->info("Reading EXR file: {}", outputPath);
    ImageData resultImage = ImageIO::readEXR(outputPath);
    if (_logger) _logger->info("EXR file read: {}x{} pixels, {} channels",
                               resultImage.width, resultImage.height, resultImage.channels);

    // Copy to output buffer
    progressUpdate(0.95);
    OfxRectI dstBounds = dst->getBounds();
    int dstWidth = dstBounds.x2 - dstBounds.x1;
    int dstHeight = dstBounds.y2 - dstBounds.y1;
    int dstRowBytes = dst->getRowBytes();
    int dstPixelComponents = dst->getPixelComponentCount();
    OFX::BitDepthEnum dstBitDepth = dst->getPixelDepth();

    int dstBitDepthInt = 32;
    if (dstBitDepth == OFX::eBitDepthUByte) dstBitDepthInt = 8;
    else if (dstBitDepth == OFX::eBitDepthUShort) dstBitDepthInt = 16;

    if (_logger) _logger->info("Destination buffer: {}x{}, {} components, {} bits",
                               dstWidth, dstHeight, dstPixelComponents, dstBitDepthInt);

    if (resultImage.width != dstWidth || resultImage.height != dstHeight) {
        if (_logger) _logger->error("Output image size mismatch: expected {}x{}, got {}x{}",
                                   dstWidth, dstHeight, resultImage.width, resultImage.height);
        throw std::runtime_error("Output image size mismatch: expected " +
                                 std::to_string(dstWidth) + "x" + std::to_string(dstHeight) +
                                 ", got " + std::to_string(resultImage.width) + "x" +
                                 std::to_string(resultImage.height));
    }

    // Critical: Validate pixel data pointer before use (prevents SIGSEGV crash)
    void* dstPixelData = dst->getPixelData();
    if (!dstPixelData) {
        if (_logger) _logger->error("CRITICAL: Destination pixel data pointer is NULL!");
        if (_logger) _logger->error("This may indicate buffer allocation failure or unsupported format");
        if (_logger) _logger->error("Buffer details: {}x{}, {} components, {} bits, {} bytes/row",
                                   dstWidth, dstHeight, dstPixelComponents, dstBitDepthInt, dstRowBytes);
        throw std::runtime_error("Failed to allocate destination image buffer. "
                                "This may be caused by unsupported image format, resolution, or insufficient memory. "
                                "Check DaVinci Resolve project settings and available system memory.");
    }

    // Additional validation: Check buffer size is reasonable
    size_t expectedBufferSize = static_cast<size_t>(dstHeight) * dstRowBytes;
    size_t requiredSize = static_cast<size_t>(resultImage.width) * resultImage.height *
                         dstPixelComponents * (dstBitDepthInt / 8);

    if (_logger) _logger->info("Buffer validation: expected {} bytes, required {} bytes",
                               expectedBufferSize, requiredSize);

    if (expectedBufferSize < requiredSize) {
        if (_logger) _logger->error("Buffer size mismatch: expected {} bytes, but need {} bytes",
                                   expectedBufferSize, requiredSize);
        throw std::runtime_error("Destination buffer is too small for image data");
    }

    if (_logger) _logger->info("Copying image data to OFX buffer (pointer: {})", dstPixelData);
    ImageIO::toOFXBuffer(resultImage, dstPixelData,
                        dstRowBytes, dstPixelComponents, dstBitDepthInt);

    if (_logger) _logger->info("Image data copied successfully");
    progressUpdate(1.0);
}

void BasePlugin::renderPassthrough(const OFX::RenderArguments &args)
{
    // Fast passthrough for proxy/preview renders
    // This provides instant visual feedback without executing the full ComfyUI workflow

    if (_logger) {
        _logger->info("renderPassthrough: Fetching source and destination images");
    }

    // Fetch source and destination images
    std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));

    if (!src.get()) {
        if (_logger) _logger->error("renderPassthrough: Failed to fetch source image");
        return;  // Graceful failure for previews
    }

    if (!dst.get()) {
        if (_logger) _logger->error("renderPassthrough: Failed to fetch destination image");
        return;  // Graceful failure for previews
    }

    // Critical: Validate destination pixel data (prevents Resolve crash)
    void* dstPixels = dst->getPixelData();
    if (!dstPixels) {
        if (_logger) {
            _logger->warn("renderPassthrough: Destination buffer is NULL");
            _logger->warn("This is acceptable for proxy renders - skipping frame");
        }
        return;  // Graceful failure - no crash
    }

    if (_logger) {
        _logger->info("renderPassthrough: Copying source to destination");
    }

    // Use existing copyPixelData function
    copyPixelData(src.get(), dst.get());

    if (_logger) {
        _logger->info("renderPassthrough: Completed successfully");
    }
}

void BasePlugin::copyPixelData(const OFX::Image* src, OFX::Image* dst)
{
    // Simple passthrough copy for thumbnails/previews
    if (!src || !dst) {
        if (_logger) _logger->warn("copyPixelData: src or dst is null");
        return;
    }

    // Get source and destination info
    OfxRectI srcBounds = src->getBounds();
    OfxRectI dstBounds = dst->getBounds();

    int srcWidth = srcBounds.x2 - srcBounds.x1;
    int srcHeight = srcBounds.y2 - srcBounds.y1;
    int dstWidth = dstBounds.x2 - dstBounds.x1;
    int dstHeight = dstBounds.y2 - dstBounds.y1;

    // Check if sizes match
    if (srcWidth != dstWidth || srcHeight != dstHeight) {
        if (_logger) _logger->warn("copyPixelData: size mismatch {}x{} vs {}x{}",
                                   srcWidth, srcHeight, dstWidth, dstHeight);
        return;
    }

    int srcRowBytes = src->getRowBytes();
    int dstRowBytes = dst->getRowBytes();
    const void* srcData = src->getPixelData();
    void* dstData = dst->getPixelData();

    if (!srcData || !dstData) {
        if (_logger) _logger->warn("copyPixelData: pixel data is null");
        return;
    }

    // Simple row-by-row copy
    const uint8_t* srcPtr = static_cast<const uint8_t*>(srcData);
    uint8_t* dstPtr = static_cast<uint8_t*>(dstData);

    int bytesPerRow = std::min(srcRowBytes, dstRowBytes);

    for (int y = 0; y < srcHeight; ++y) {
        std::memcpy(dstPtr + y * dstRowBytes,
                   srcPtr + y * srcRowBytes,
                   bytesPerRow);
    }
}

std::string BasePlugin::writeInputImage(OFX::Image* img, int frame)
{
    if (!img) {
        if (_logger) _logger->error("No input image to write");
        throw std::runtime_error("No input image to write");
    }

    // Get path components
    std::string mountPath, project, workflow, version;
    _sharedMountPath->getValue(mountPath);
    _projectName->getValue(project);
    _workflowName->getValue(workflow);
    _outputVersion->getValue(version);

    // Get effective basename (auto-generated or manual)
    std::string basename = getEffectiveBasename();

    // Build path matching output pattern: basename.frame.exr
    // Input and output now use the same naming convention for consistency
    // Example: /Volumes/silo2/002_COMFYUI/in/acme_spot/segmentation/v001/shot01.0001.exr
    std::ostringstream filename;
    filename << mountPath << "/in/" 
             << project << "/" 
             << workflow << "/" 
             << version << "/"
             << basename << "."
             << std::setw(4) << std::setfill('0') << frame << ".exr";

    if (_logger) _logger->info("Writing input image to: {}", filename.str());

    // Convert OFX image to ImageData
    OfxRectI bounds = img->getBounds();
    int width = bounds.x2 - bounds.x1;
    int height = bounds.y2 - bounds.y1;
    int rowBytes = img->getRowBytes();
    int pixelComponents = img->getPixelComponentCount();
    OFX::BitDepthEnum bitDepth = img->getPixelDepth();

    // Convert bit depth enum to integer
    int bitDepthInt = 32; // default to float
    if (bitDepth == OFX::eBitDepthUByte) {
        bitDepthInt = 8;
    } else if (bitDepth == OFX::eBitDepthUShort) {
        bitDepthInt = 16;
    }

    if (_logger) _logger->info("Input image specs: {}x{}, {} components, {} bits",
                               width, height, pixelComponents, bitDepthInt);

    // Convert OFX buffer to ImageData
    if (_logger) _logger->info("Converting OFX buffer to ImageData");

    const void* pixelData = img->getPixelData();

    ImageData imageData = ImageIO::fromOFXBuffer(
        pixelData,
        width,
        height,
        rowBytes,
        pixelComponents,
        bitDepthInt
    );

    // Log first pixel after safe conversion
    if (_logger && imageData.pixels.size() >= 4) {
        _logger->info("First pixel after conversion (RGBA): [{:.4f}, {:.4f}, {:.4f}, {:.4f}]",
                     imageData.pixels[0], imageData.pixels[1], imageData.pixels[2],
                     pixelComponents >= 4 ? imageData.pixels[3] : 1.0f);

        // Check if image appears to be blank/white
        float sum = 0.0f;
        int sampleCount = std::min(1000, static_cast<int>(imageData.pixels.size() / pixelComponents));
        for (int i = 0; i < sampleCount; ++i) {
            sum += imageData.pixels[i * pixelComponents + 0]; // Sample R channel
        }
        float avg = sum / sampleCount;
        _logger->info("Average pixel value (sampled): {:.4f}", avg);

        if (avg > 0.95f) {
            _logger->warn("WARNING: Input image appears to be mostly white! Check source connection in host.");
        } else if (avg < 0.05f) {
            _logger->warn("WARNING: Input image appears to be mostly black! Check source connection in host.");
        }
    }

    // Write EXR file
    if (_logger) _logger->info("Writing EXR file");
    ImageIO::writeEXR(filename.str(), imageData);
    if (_logger) _logger->info("EXR file written successfully");

    return filename.str();
}

std::string BasePlugin::parseOutputPath(const json& history, int frame)
{
    if (_logger) _logger->info("Parsing output path from history JSON");

    // ComfyUI history structure:
    // {
    //   "prompt_id": {
    //     "outputs": {
    //       "node_id": {
    //         "images": [ { "filename": "...", "subfolder": "...", "type": "output" } ]
    //       }
    //     }
    //   }
    // }

    // Get path components
    std::string mountPath, project, workflow, version;
    _sharedMountPath->getValue(mountPath);
    _projectName->getValue(project);
    _workflowName->getValue(workflow);
    _outputVersion->getValue(version);

    // Get auto-generated basename
    std::string basename = getEffectiveBasename();

    try {
        // History is a dict with prompt_id as key
        for (auto& [promptId, promptData] : history.items()) {
            if (_logger) _logger->info("Checking prompt ID: {}", promptId);

            if (!promptData.contains("outputs")) {
                if (_logger) _logger->warn("Prompt {} has no outputs", promptId);
                continue;
            }

            const json& outputs = promptData["outputs"];

            // Iterate through all output nodes
            for (auto& [nodeId, nodeData] : outputs.items()) {
                if (_logger) _logger->info("Checking output node: {}", nodeId);

                if (!nodeData.contains("images")) {
                    if (_logger) _logger->warn("Node {} has no images", nodeId);
                    continue;
                }

                const json& images = nodeData["images"];
                if (!images.is_array() || images.empty()) {
                    if (_logger) _logger->warn("Node {} has empty images array - likely cached workflow", nodeId);

                    // CACHED WORKFLOW HANDLING:
                    // When ComfyUI caches a workflow (0.00s execution), it returns empty image arrays
                    // because the files already exist on disk from a previous run.
                    // We need to construct the expected output filename from the workflow JSON.

                    if (_logger) _logger->info("Extracting filename pattern from workflow for cached result");

                    // The history contains the full workflow in prompt[2] (third element)
                    if (!promptData.contains("prompt") || !promptData["prompt"].is_array() || promptData["prompt"].size() < 3) {
                        if (_logger) _logger->warn("Cannot extract workflow from prompt - invalid structure");
                        continue;
                    }

                    const json& workflowNodes = promptData["prompt"][2];
                    if (!workflowNodes.contains(nodeId)) {
                        if (_logger) _logger->warn("Node {} not found in workflow JSON", nodeId);
                        continue;
                    }

                    const json& node = workflowNodes[nodeId];
                    if (!node.contains("inputs") || !node["inputs"].contains("filename_prefix")) {
                        if (_logger) _logger->warn("Node {} has no filename_prefix in workflow", nodeId);
                        continue;
                    }

                    std::string filenamePrefix = node["inputs"]["filename_prefix"].get<std::string>();
                    if (_logger) _logger->info("Extracted filename_prefix from node {}: {}", nodeId, filenamePrefix);

                    // Get SaveEXR version and frame padding from workflow
                    int saveVersion = node["inputs"].value("version", 1);
                    int framePad = node["inputs"].value("frame_pad", 4);

                    if (_logger) _logger->info("SaveEXR version: {}, frame_pad: {}", saveVersion, framePad);

                    // SaveEXR pattern depends on version parameter:
                    // - If version >= 0: {prefix}_v{version:03d}.{frame:0{pad}d}.exr
                    // - If version == -1: {prefix}.{frame:0{pad}d}.exr (no version suffix)
                    // Example (version=-1): Z:\out\TEST_SAM\segmentation\v001\shot01.0056.exr
                    // Extract just the filename part (last component after last slash/backslash)
                    size_t lastSlash = filenamePrefix.find_last_of("/\\");
                    std::string prefixBasename = (lastSlash != std::string::npos)
                        ? filenamePrefix.substr(lastSlash + 1)
                        : filenamePrefix;

                    // Construct filename based on SaveEXR version parameter
                    std::ostringstream filename;
                    filename << prefixBasename;

                    // Add version suffix only if version >= 0
                    if (saveVersion >= 0) {
                        filename << "_v" << std::setfill('0') << std::setw(3) << saveVersion;
                    }

                    filename << "."
                             << std::setfill('0') << std::setw(framePad) << frame
                             << ".exr";

                    std::string constructedFilename = filename.str();
                    if (_logger) _logger->info("Constructed SaveEXR filename: {}", constructedFilename);

                    // Build full path
                    // Output: /Volumes/silo2/002_COMFYUI/out/<PROJECT>/<WORKFLOW>/<VERSION>/{filename}
                    std::ostringstream fullPath;
                    fullPath << mountPath << "/out/" << project << "/"
                             << workflow << "/" << version << "/" << constructedFilename;

                    std::string constructedPath = fullPath.str();
                    if (_logger) _logger->info("Constructed output path: {}", constructedPath);

                    // Verify file exists (cached workflows should have file on disk)
                    std::ifstream testFile(constructedPath);
                    if (testFile.good()) {
                        if (_logger) _logger->info("✓ Verified cached output file exists on disk");
                        testFile.close();
                        return constructedPath;
                    } else {
                        if (_logger) _logger->warn("✗ Constructed path does not exist on disk: {}", constructedPath);
                        // Continue checking other nodes - maybe another node has the file
                        continue;
                    }
                }

                // Get the first image (normal workflow with filename in history)
                const json& firstImage = images[0];
                if (firstImage.contains("filename")) {
                    std::string filename = firstImage["filename"].get<std::string>();
                    if (_logger) _logger->info("Found output filename from history: {}", filename);

                    // Build full path matching Python pattern
                    // Output: /Volumes/silo2/002_COMFYUI/out/<PROJECT>/<WORKFLOW>/<VERSION>/basename_layer_frame_version_.exr
                    std::ostringstream fullPath;
                    fullPath << mountPath << "/out/" << project << "/"
                             << workflow << "/" << version << "/" << filename;

                    if (_logger) _logger->info("Constructed output path: {}", fullPath.str());
                    return fullPath.str();
                }
            }
        }
    } catch (const json::exception& e) {
        if (_logger) _logger->error("JSON parsing error: {}", e.what());
        throw std::runtime_error("Failed to parse ComfyUI history: " + std::string(e.what()));
    }

    // No output found
    if (_logger) _logger->warn("No output found in history and no cached file found on disk");
    return "";
}

std::string BasePlugin::convertPathForComfyUI(const std::string& localPath)
{
    // Convert client-side path to server-side path
    // Example: /Volumes/silo2/002_COMFYUI/in/test.exr -> Z:\in\test.exr

    if (_logger) _logger->info("Converting path for ComfyUI: {}", localPath);

    // Get mount paths
    std::string clientMount, serverMount;
    _sharedMountPath->getValue(clientMount);
    _serverMountPoint->getValue(serverMount);

    if (_logger) {
        _logger->info("  Client mount: {}", clientMount);
        _logger->info("  Server mount: {}", serverMount);
    }

    std::string windowsPath = localPath;

    // Replace client mount path with server mount point
    // Example: /Volumes/silo2/002_COMFYUI -> Z:
    if (windowsPath.find(clientMount) == 0) {
        windowsPath.replace(0, clientMount.length(), serverMount);
        if (_logger) _logger->info("  After mount replacement: {}", windowsPath);
    } else {
        if (_logger) _logger->warn("  Path doesn't start with client mount! Using as-is");
    }

    // Replace all forward slashes with backslashes
    std::replace(windowsPath.begin(), windowsPath.end(), '/', '\\');

    if (_logger) _logger->info("  Windows path (before JSON escaping): {}", windowsPath);

    // CRITICAL: Escape backslashes for JSON (\ -> \\)
    // JSON requires backslashes to be escaped
    // Example: Z:\in\test.exr -> Z:\\in\\test.exr
    std::string jsonPath;
    jsonPath.reserve(windowsPath.length() * 2);  // Reserve space for worst case
    for (char c : windowsPath) {
        if (c == '\\') {
            jsonPath += "\\\\";  // Escape backslash
        } else {
            jsonPath += c;
        }
    }

    if (_logger) _logger->info("  Final JSON-escaped path: {}", jsonPath);
    return jsonPath;
}

std::string BasePlugin::constructExpectedOutputPath(int frame)
{
    // Construct the expected output file path based on SaveEXR naming pattern
    // Pattern: {mountPath}/out/{project}/{workflow}/{version}/{basename}.{frame:04d}.exr
    //
    // NOTE: SaveEXR version is set to -1 (no version suffix) because the directory
    // already contains the version number (e.g., v001, v002).
    //
    // Example: /Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/shot01.0056.exr

    std::string mountPath, project, workflow, version;
    _sharedMountPath->getValue(mountPath);
    _projectName->getValue(project);
    _workflowName->getValue(workflow);
    _outputVersion->getValue(version);

    // Get effective basename (auto-generated or manual)
    std::string basename = getEffectiveBasename();

    // Construct full path (no version suffix in filename, version is in directory)
    std::ostringstream outputPath;
    outputPath << mountPath << "/out/" 
               << project << "/"
               << workflow << "/" 
               << version << "/" 
               << basename << "."
               << std::setfill('0') << std::setw(4) << frame << ".exr";

    return outputPath.str();
}

std::string BasePlugin::getEffectiveBasename()
{
    // Always auto-generate basename from project + instance name (Pybox pattern)
    if (!_instanceName.empty()) {
        std::string project;
        _projectName->getValue(project);

        // Sanitize instance name: replace non-alphanumeric characters with underscores
        std::string sanitizedName = _instanceName;
        for (char& c : sanitizedName) {
            if (!std::isalnum(static_cast<unsigned char>(c))) {
                c = '_';
            }
        }

        // Generate basename: {project}_{instance_name}
        std::string generatedBasename = project + "_" + sanitizedName;

        if (_logger) {
            _logger->info("Auto-generated basename: {} (from project='{}', instance='{}')",
                         generatedBasename, project, sanitizedName);
        }

        return generatedBasename;
    } else {
        // Fallback: if no instance name, use project name as basename
        std::string project;
        _projectName->getValue(project);

        if (_logger) {
            _logger->warn("No instance name available, using project name as basename: {}", project);
        }

        return project;
    }
}

// ============================================================================
// Workflow File Management
// ============================================================================

std::string BasePlugin::getBundleResourcePath(const std::string& resourceName)
{
#ifdef __APPLE__
    // On macOS, use CoreFoundation to locate bundle resources
    // Try to get bundle from plugin binary path
    std::string pluginPath;
    try {
        pluginPath = getPropertySet().propGetString(kOfxPluginPropFilePath, false);
        if (_logger) _logger->debug("Plugin file path: {}", pluginPath);
    } catch (...) {
        if (_logger) _logger->warn("Could not retrieve plugin file path");
        return "";
    }

    // Plugin path format: /path/to/Plugin.ofx.bundle/Contents/MacOS/Plugin.ofx
    // We want: /path/to/Plugin.ofx.bundle/Contents/Resources/{resourceName}

    size_t bundlePos = pluginPath.find(".ofx.bundle");
    if (bundlePos == std::string::npos) {
        if (_logger) _logger->warn("Plugin path does not contain .ofx.bundle: {}", pluginPath);
        return "";
    }

    // Extract bundle path (include .ofx.bundle)
    std::string bundlePath = pluginPath.substr(0, bundlePos + 11); // +11 for ".ofx.bundle"
    std::string resourcePath = bundlePath + "/Contents/Resources/" + resourceName;

    if (_logger) _logger->debug("Resolved bundle resource path: {}", resourcePath);
    return resourcePath;

#elif defined(__linux__)
    // On Linux, bundle structure: Plugin.ofx.bundle/Contents/Linux-{arch}/Plugin.ofx
    //                 Resources: Plugin.ofx.bundle/Resources/{resourceName}
    std::string pluginPath;
    try {
        pluginPath = getPropertySet().propGetString(kOfxPluginPropFilePath, false);
        if (_logger) _logger->debug("Plugin file path: {}", pluginPath);
    } catch (...) {
        if (_logger) _logger->warn("Could not retrieve plugin file path");
        return "";
    }

    size_t bundlePos = pluginPath.find(".ofx.bundle");
    if (bundlePos == std::string::npos) {
        if (_logger) _logger->warn("Plugin path does not contain .ofx.bundle: {}", pluginPath);
        return "";
    }

    std::string bundlePath = pluginPath.substr(0, bundlePos + 11);
    std::string resourcePath = bundlePath + "/Resources/" + resourceName;

    if (_logger) _logger->debug("Resolved bundle resource path: {}", resourcePath);
    return resourcePath;

#else
    // Windows or unsupported platform
    if (_logger) _logger->warn("Bundle resource lookup not implemented for this platform");
    return "";
#endif
}

std::string BasePlugin::resolveWorkflowPath(const std::string& workflowPath)
{
    if (_logger) _logger->info("Resolving workflow path: {}", workflowPath);

    // If path is absolute and exists, use it directly
    if (!workflowPath.empty() && workflowPath[0] == '/') {
        std::ifstream test(workflowPath);
        if (test.good()) {
            test.close();
            if (_logger) _logger->info("Using absolute workflow path: {}", workflowPath);
            return workflowPath;
        }
    }

    // If path starts with "resources/", treat it as bundle-relative
    if (workflowPath.find("resources/") == 0) {
        std::string bundlePath = getBundleResourcePath(workflowPath.substr(10)); // Skip "resources/"
        if (!bundlePath.empty()) {
            std::ifstream test(bundlePath);
            if (test.good()) {
                test.close();
                if (_logger) _logger->info("Found workflow in bundle: {}", bundlePath);
                return bundlePath;
            }
        }
    }

    // Try as bundle-relative path directly
    std::string bundlePath = getBundleResourcePath(workflowPath);
    if (!bundlePath.empty()) {
        std::ifstream test(bundlePath);
        if (test.good()) {
            test.close();
            if (_logger) _logger->info("Found workflow in bundle: {}", bundlePath);
            return bundlePath;
        }
    }

    if (_logger) _logger->warn("Could not resolve workflow path: {}", workflowPath);
    return "";
}

json BasePlugin::loadWorkflowFromFile(const std::string& filepath)
{
    if (_logger) _logger->info("Loading workflow from file: {}", filepath);

    if (filepath.empty()) {
        throw std::runtime_error("Workflow file path is empty");
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::string error = "Failed to open workflow file: " + filepath;
        if (_logger) _logger->error(error);
        throw std::runtime_error(error);
    }

    try {
        // Read entire file as string first (don't parse yet - file may have placeholders)
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string workflowStr = buffer.str();

        if (_logger) {
            _logger->info("Workflow file read successfully: {} bytes", workflowStr.size());
        }

        // Parse as JSON - this will work if file has no placeholders, or
        // will be done after placeholder replacement in customizeWorkflowWithParams()
        json workflow = json::parse(workflowStr);

        if (_logger) {
            _logger->debug("Workflow parsed successfully");
        }

        return workflow;
    } catch (const json::exception& e) {
        std::string error = "Failed to parse workflow JSON: " + std::string(e.what());
        if (_logger) _logger->error(error);
        throw std::runtime_error(error);
    }
}

json BasePlugin::customizeWorkflow(const json& baseWorkflow, int frame, const std::string& inputPath)
{
    // Default implementation: replace placeholders in workflow JSON
    // Derived classes can override for more complex customization

    if (_logger) _logger->debug("Customizing workflow for frame {}", frame);

    // Convert the workflow to a string for placeholder replacement
    std::string workflowStr = baseWorkflow.dump();

    // Get parameters for replacements
    std::string mountPath, project, workflowName, version;
    _sharedMountPath->getValue(mountPath);
    _projectName->getValue(project);
    _workflowName->getValue(workflowName);
    _outputVersion->getValue(version);

    std::string basename = getEffectiveBasename();

    // Build output prefix
    std::ostringstream outputPrefix;
    outputPrefix << mountPath << "/out/" << project << "/" << workflowName
                 << "/" << version << "/" << basename;

    // Convert paths for ComfyUI
    std::string comfyInputPath = convertPathForComfyUI(inputPath);
    std::string comfyOutputPrefix = convertPathForComfyUI(outputPrefix.str());

    // Common placeholder replacements
    // ${INPUT_PATH} - input EXR file path (Windows format for ComfyUI)
    // ${OUTPUT_PREFIX} - output file prefix (Windows format for ComfyUI)
    // ${FRAME} - current frame number

    size_t pos = 0;
    std::string placeholder;

    // Replace ${INPUT_PATH}
    placeholder = "${INPUT_PATH}";
    if (_logger) _logger->debug("Replacing ${{INPUT_PATH}} with: {}", comfyInputPath);
    int replaceCount = 0;
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), comfyInputPath);
        replaceCount++;
    }
    if (_logger && replaceCount == 0) {
        _logger->warn("INPUT_PATH placeholder not found in workflow!");
    }

    // Replace ${OUTPUT_PREFIX}
    placeholder = "${OUTPUT_PREFIX}";
    if (_logger) _logger->debug("Replacing ${{OUTPUT_PREFIX}} with: {}", comfyOutputPrefix);
    replaceCount = 0;
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), comfyOutputPrefix);
        replaceCount++;
    }
    if (_logger && replaceCount == 0) {
        _logger->warn("OUTPUT_PREFIX placeholder not found in workflow!");
    }

    // Replace "${FRAME}" with number (no quotes)
    // Search for the placeholder INCLUDING surrounding quotes to convert string to number
    placeholder = "\"${FRAME}\"";
    std::string frameStr = std::to_string(frame);
    if (_logger) _logger->info("Replacing \"${{FRAME}}\" with numeric: {}", frameStr);
    replaceCount = 0;
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), frameStr);
        replaceCount++;
        if (_logger) _logger->debug("  Replaced FRAME at position {}", pos);
    }
    if (_logger) {
        if (replaceCount == 0) {
            _logger->warn("FRAME placeholder not found in workflow! Search pattern: {}", placeholder);
        } else {
            _logger->info("  Replaced FRAME {} time(s)", replaceCount);
        }
    }

    if (_logger) {
        _logger->debug("Workflow customization complete");
        _logger->trace("Customized workflow:\n{}", workflowStr.substr(0, 500) + "...");
    }

    // Parse back to JSON
    try {
        json finalWorkflow = json::parse(workflowStr);

        // Log the workflow after base customization (before plugin-specific customization)
        if (_logger) {
            _logger->debug("=== INTERMEDIATE WORKFLOW (after base customization) ===");
            _logger->debug("Workflow JSON after base replacements: {}", finalWorkflow.dump(2));
            _logger->debug("=== END INTERMEDIATE WORKFLOW ===");
            _logger->info("Base workflow customization complete (paths and frame replaced)");
        }

        return finalWorkflow;
    } catch (const json::exception& e) {
        std::string error = "Failed to parse customized workflow: " + std::string(e.what());
        if (_logger) _logger->error(error);
        throw std::runtime_error(error);
    }
}

// ============================================================================
// Async Rendering Helper Methods
// ============================================================================

void BasePlugin::renderBlocking(const OFX::RenderArguments &args)
{
    // Traditional blocking render - same as old render() logic
    int frame = static_cast<int>(args.time);

    // Validate required parameters
    std::string projectName;
    _projectName->getValue(projectName);

    if (projectName.empty()) {
        std::string warningMsg = "Project Name is required but not set. Please set the Project Name parameter in the plugin settings.";
        if (_logger) _logger->warn(warningMsg);
        setPersistentMessage(OFX::Message::eMessageWarning, "", warningMsg.c_str());

        // Return passthrough
        if (_srcClip && _srcClip->isConnected()) {
            std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
            std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
            if (src.get() && dst.get()) {
                copyPixelData(src.get(), dst.get());
            }
        }
        return;
    }

    // Check if source is connected
    if (!_srcClip || !_srcClip->isConnected()) {
        if (_logger) _logger->error("Source clip not connected");
        throw std::runtime_error("Source clip not connected");
    }

    // Pre-create output directory on CLIENT side (will sync to SERVER via network mount)
    std::string mountPath, workflowName, version, serverMount;
    _sharedMountPath->getValue(mountPath);
    _workflowName->getValue(workflowName);
    _outputVersion->getValue(version);
    _serverMountPoint->getValue(serverMount);

    if (_logger) {
        _logger->info("========================================");
        _logger->info("DIRECTORY CREATION");
        _logger->info("========================================");
        _logger->info("Client mount: {}", mountPath);
        _logger->info("Server mount: {}", serverMount);
    }

    // First, ensure the base /out directory exists
    std::string baseOutDir = mountPath + "/out";
    try {
        // Check if mount path exists
        struct stat mountStat;
        if (stat(mountPath.c_str(), &mountStat) != 0 || !S_ISDIR(mountStat.st_mode)) {
            std::string errorMsg = "Shared mount path does not exist or is not accessible: " + mountPath;
            errorMsg += "\n\nPlease verify:";
            errorMsg += "\n1. The shared mount is properly configured";
            errorMsg += "\n2. The mount path is accessible from this machine";
            errorMsg += "\n3. You have write permissions to the mount";
            if (_logger) _logger->error(errorMsg);
            setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
            throw std::runtime_error(errorMsg);
        }

        // Create base /out directory
        if (_logger) _logger->info("Creating base output directory: {}", baseOutDir);
        if (!ImageIO::createDirectoryRecursive(baseOutDir)) {
            std::string errorMsg = "Failed to create base output directory: " + baseOutDir;
            if (_logger) _logger->error(errorMsg);
            setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
            throw std::runtime_error(errorMsg);
        }
    } catch (const std::exception& e) {
        std::string errorMsg = std::string("Failed to create base output directory: ") + e.what();
        if (_logger) _logger->error(errorMsg);
        setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
        throw std::runtime_error(errorMsg);
    }

    // Now create the full output directory path
    std::string outputDir = mountPath + "/out/" + projectName + "/" + workflowName + "/" + version;
    if (_logger) _logger->info("Creating full output directory: {}", outputDir);

    try {
        if (!ImageIO::createDirectoryRecursive(outputDir)) {
            std::string errorMsg = "Failed to create output directory: " + outputDir;
            if (_logger) _logger->error(errorMsg);
            setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
            throw std::runtime_error(errorMsg);
        }

        // Verify directory was created successfully
        struct stat st;
        if (stat(outputDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            std::string errorMsg = "Directory creation succeeded but verification failed: " + outputDir;
            if (_logger) _logger->error(errorMsg);
            setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
            throw std::runtime_error(errorMsg);
        }

        if (_logger) {
            _logger->info("Successfully created and verified directory: {}", outputDir);

            // Log what the server-side path will be
            std::string serverPath = outputDir;
            if (!mountPath.empty() && serverPath.find(mountPath) == 0) {
                serverPath.replace(0, mountPath.length(), serverMount);
            }
            std::replace(serverPath.begin(), serverPath.end(), '/', '\\');
            _logger->info("Server-side path should be: {}", serverPath);
            _logger->info("");
            _logger->info("IMPORTANT: If ComfyUI fails with 'path not found' error,");
            _logger->info("manually verify that this directory exists on the Windows server:");
            _logger->info("  {}", serverPath);
            _logger->info("========================================");
        }
    } catch (const std::exception& e) {
        std::string errorMsg = std::string("Failed to create output directory: ") + e.what();
        if (_logger) _logger->error(errorMsg);
        setPersistentMessage(OFX::Message::eMessageError, "", errorMsg.c_str());
        throw std::runtime_error(errorMsg);
    }

    // Execute workflow synchronously (BLOCKING)
    progressStart("Processing with ComfyUI...");

    try {
        executeWorkflow(args);
        progressEnd();
        if (_logger) {
            _logger->info("========================================");
            _logger->info("RENDER COMPLETED SUCCESSFULLY");
            _logger->info("========================================");
        }
    } catch (const std::exception& e) {
        progressEnd();
        if (_logger) {
            _logger->error("========================================");
            _logger->error("RENDER FAILED: {}", e.what());
            _logger->error("========================================");
        }
        throw;
    }
}

void BasePlugin::renderAsync(const OFX::RenderArguments &args)
{
    auto renderStartTime = std::chrono::steady_clock::now();
    int frame = static_cast<int>(args.time);

    if (_logger) {
        _logger->info("========================================");
        _logger->info("renderAsync() START - Frame {}", frame);
    }

    // STEP 1: Check cache first (instant return if available)
    auto cacheCheckStart = std::chrono::steady_clock::now();
    std::string cachedPath = constructExpectedOutputPath(frame);

    // Check in-memory cache first (instant!)
    bool cachedInMemory = false;
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        cachedInMemory = (_cacheFileExists.find(cachedPath) != _cacheFileExists.end());
    }

    bool fileExists = false;
    if (cachedInMemory) {
        // Fast path: we know the file exists from previous check
        fileExists = true;
        if (_logger) {
            _logger->debug("Frame {}: In-memory cache HIT for {}", frame, cachedPath);
        }
    } else {
        // Slow path: check filesystem (only once per file)
        std::ifstream cacheTest(cachedPath);
        fileExists = cacheTest.good();
        if (fileExists) {
            cacheTest.close();
            // Remember this file exists (avoid future slow checks)
            std::lock_guard<std::mutex> lock(_cacheMutex);
            _cacheFileExists.insert(cachedPath);
        }
    }

    auto cacheCheckDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cacheCheckStart);

    if (fileExists) {
        if (_logger) {
            _logger->info("Frame {}: Cache HIT (check took {} ms, cached_in_mem: {})",
                         frame, cacheCheckDuration.count(), cachedInMemory);
            _logger->info("  Path: {}", cachedPath);
        }

        try {
            auto loadStart = std::chrono::steady_clock::now();
            loadCachedResult(args, cachedPath);
            auto loadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - loadStart);

            auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - renderStartTime);

            if (_logger) {
                _logger->info("Frame {}: Loaded from cache (load: {} ms, total: {} ms)",
                             frame, loadDuration.count(), totalDuration.count());
                _logger->info("========================================");
            }
            return;  // ✓ INSTANT, non-blocking
        } catch (const std::exception& e) {
            std::string errorMsg = e.what();
            bool isDimensionMismatch = (errorMsg.find("dimensions") != std::string::npos &&
                                       errorMsg.find("do not match") != std::string::npos);

            if (_logger) {
                if (isDimensionMismatch) {
                    _logger->warn("Frame {}: Cached file has wrong dimensions - will invalidate and re-render", frame);
                    _logger->info("  Error details: {}", errorMsg);
                } else {
                    _logger->error("Frame {}: Failed to load cached result: {}", frame, errorMsg);
                }
            }

            // Remove from memory cache
            {
                std::lock_guard<std::mutex> lock(_cacheMutex);
                _cacheFileExists.erase(cachedPath);
            }

            // If dimension mismatch, rename the wrong-resolution file so it doesn't interfere
            if (isDimensionMismatch) {
                try {
                    std::string renamedPath = cachedPath + ".wrong_resolution";
                    if (_logger) {
                        _logger->info("  Renaming wrong-resolution cached file to: {}", renamedPath);
                    }
                    std::filesystem::rename(cachedPath, renamedPath);
                } catch (const std::exception& renameError) {
                    if (_logger) {
                        _logger->warn("  Could not rename cached file: {}", renameError.what());
                    }
                }
            }

            // Fall through to re-render
        }
    } else {
        if (_logger) {
            _logger->info("Frame {}: Cache MISS (check took {} ms)", frame, cacheCheckDuration.count());
        }
    }

    // Lazy initialize job manager (after _comfyClient exists)
    if (!_jobManager && _comfyClient) {
        if (_logger) _logger->info("Initializing AsyncJobManager");
        _jobManager.reset(new AsyncJobManager(_comfyClient.get(), _logger));

        // Set completion callback
        _jobManager->setCompletionCallback([this](int completedFrame, bool success) {
            this->onJobComplete(completedFrame, success);
        });

        if (_logger) _logger->info("AsyncJobManager initialized successfully");
    }

    // STEP 2: Check if job already pending
    if (_jobManager && _jobManager->isJobPending(frame)) {
        JobStatus status = _jobManager->getJobStatus(frame);
        if (_logger) {
            _logger->info("Frame {}: Job already pending (status: {})",
                         frame, jobStatusToString(status));
        }

        // Return placeholder immediately
        returnPlaceholder(args, frame);
        return;  // ✓ NON-BLOCKING
    }

    // STEP 3: Submit new async job
    if (_logger) _logger->info("Frame {}: Cache MISS - submitting async job", frame);

    try {
        // Validate project name
        std::string projectName;
        _projectName->getValue(projectName);

        if (projectName.empty()) {
            if (_logger) _logger->warn("Frame {}: Project name empty, returning passthrough", frame);
            returnPlaceholder(args, frame);
            return;
        }

        // Get mount path info for directory creation
        std::string mountPath, workflowName, version;
        _sharedMountPath->getValue(mountPath);
        _workflowName->getValue(workflowName);
        _outputVersion->getValue(version);

        // Pre-create output directory on CLIENT side (will sync to SERVER via network mount)
        // This MUST happen before async submission to ensure directories exist
        std::string outputDir = mountPath + "/out/" + projectName + "/" + workflowName + "/" + version;

        try {
            // Check if mount path exists
            struct stat mountStat;
            if (stat(mountPath.c_str(), &mountStat) != 0 || !S_ISDIR(mountStat.st_mode)) {
                std::string errorMsg = "Shared mount path does not exist or is not accessible: " + mountPath;
                if (_logger) _logger->error("Frame {}: {}", frame, errorMsg);
                returnPlaceholder(args, frame);
                return;
            }

            // Create base /out directory
            std::string baseOutDir = mountPath + "/out";
            if (!ImageIO::createDirectoryRecursive(baseOutDir)) {
                std::string errorMsg = "Failed to create base output directory: " + baseOutDir;
                if (_logger) _logger->error("Frame {}: {}", frame, errorMsg);
                returnPlaceholder(args, frame);
                return;
            }

            // Create full output directory path
            if (!ImageIO::createDirectoryRecursive(outputDir)) {
                std::string errorMsg = "Failed to create output directory: " + outputDir;
                if (_logger) _logger->error("Frame {}: {}", frame, errorMsg);
                returnPlaceholder(args, frame);
                return;
            }

            // Verify directory was created successfully
            struct stat st;
            if (stat(outputDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                std::string errorMsg = "Directory creation succeeded but verification failed: " + outputDir;
                if (_logger) _logger->error("Frame {}: {}", frame, errorMsg);
                returnPlaceholder(args, frame);
                return;
            }

            if (_logger) {
                _logger->info("Frame {}: Successfully created output directory: {}", frame, outputDir);
            }
        } catch (const std::exception& e) {
            std::string errorMsg = std::string("Failed to create output directory: ") + e.what();
            if (_logger) _logger->error("Frame {}: {}", frame, errorMsg);
            returnPlaceholder(args, frame);
            return;
        }

        // Fetch source image (fast, in-memory operation)
        if (!_srcClip || !_srcClip->isConnected()) {
            if (_logger) _logger->error("Frame {}: Source clip not connected", frame);
            returnPlaceholder(args, frame);
            return;
        }

        auto fetchStart = std::chrono::steady_clock::now();
        std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
        if (!src.get()) {
            if (_logger) _logger->error("Frame {}: Failed to fetch source image", frame);
            returnPlaceholder(args, frame);
            return;
        }

        auto fetchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - fetchStart);

        // Convert OFX image to ImageData (fast, in-memory copy)
        auto convertStart = std::chrono::steady_clock::now();

        OfxRectI bounds = src->getBounds();
        int width = bounds.x2 - bounds.x1;
        int height = bounds.y2 - bounds.y1;
        int rowBytes = src->getRowBytes();
        int pixelComponents = src->getPixelComponentCount();
        OFX::BitDepthEnum bitDepth = src->getPixelDepth();

        int bitDepthInt = 32; // default to float
        if (bitDepth == OFX::eBitDepthUByte) {
            bitDepthInt = 8;
        } else if (bitDepth == OFX::eBitDepthUShort) {
            bitDepthInt = 16;
        }

        const void* pixelData = src->getPixelData();

        ImageData imageData = ImageIO::fromOFXBuffer(
            pixelData,
            width,
            height,
            rowBytes,
            pixelComponents,
            bitDepthInt
        );

        auto convertDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - convertStart);

        // Construct input file path (reuse variables from above)
        std::string basename = getEffectiveBasename();

        std::ostringstream filename;
        filename << mountPath << "/in/"
                 << projectName << "/"
                 << workflowName << "/"
                 << version << "/"
                 << basename << "."
                 << std::setw(4) << std::setfill('0') << frame << ".exr";

        std::string inputPath = filename.str();

        // Ensure ComfyUI client exists
        if (!_comfyClient) {
            if (_logger) _logger->info("Frame {}: Creating ComfyUI client", frame);

            std::string address;
            _serverAddress->getValue(address);
            int port = _serverPort->getValue();
            std::string serverUrl = address + ":" + std::to_string(port);
            _comfyClient.reset(new Client(serverUrl));

            // Re-initialize job manager with new client
            _jobManager.reset(new AsyncJobManager(_comfyClient.get(), _logger));
            _jobManager->setCompletionCallback([this](int completedFrame, bool success) {
                this->onJobComplete(completedFrame, success);
            });
        }

        // Submit job asynchronously (TRULY NON-BLOCKING!)
        // The slow parts (writing EXR, building workflow, submitting to ComfyUI) happen in background
        if (_jobManager) {
            auto submitStart = std::chrono::steady_clock::now();

            _jobManager->submitJobAsync(frame, imageData, inputPath, cachedPath, this);

            auto submitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - submitStart);

            auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - renderStartTime);

            if (_logger) {
                _logger->info("Frame {}: submitJobAsync() returned in {} ms (non-blocking!)", frame, submitDuration.count());
                _logger->info("Frame {}: renderAsync() TOTAL TIME: {} ms", frame, totalDuration.count());
                _logger->info("  Breakdown: cache={} ms, fetch={} ms, convert={} ms, submitAsync={} ms",
                             cacheCheckDuration.count(), fetchDuration.count(),
                             convertDuration.count(), submitDuration.count());
                _logger->info("  (File write, workflow build, and ComfyUI submission happening in background)");
            }

            updateJobStatusDisplay();
        }

    } catch (const std::exception& e) {
        if (_logger) _logger->error("Frame {}: Failed to submit async job: {}", frame, e.what());
    }

    // Return placeholder immediately
    returnPlaceholder(args, frame);  // ✓ NON-BLOCKING
}

void BasePlugin::returnPlaceholder(const OFX::RenderArguments &args, int frame)
{
    // Always use checkerboard pattern (mode 1) - parameter is hidden
    int placeholderMode = 1;
    // _placeholderMode->getValue(placeholderMode);  // No longer reading from parameter

    if (_logger) _logger->debug("Frame {}: Returning placeholder (mode: {})", frame, placeholderMode);

    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    if (!dst.get()) {
        if (_logger) _logger->error("Frame {}: Failed to fetch destination image", frame);
        return;
    }

    switch (placeholderMode) {
        case 0:  // Source passthrough
            if (_srcClip && _srcClip->isConnected()) {
                std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
                if (src.get()) {
                    copyPixelData(src.get(), dst.get());
                }
            }
            break;

        case 1:  // Checkerboard pattern
            renderCheckerboard(dst.get());
            break;

        case 2:  // Gray frame
            renderSolidColor(dst.get(), 0.5, 0.5, 0.5);
            break;

        case 3:  // Last valid frame
            {
                int lastValidFrame = findLastValidFrame(args.time);
                if (lastValidFrame >= 0) {
                    std::string lastValidPath = constructExpectedOutputPath(lastValidFrame);
                    try {
                        loadCachedResult(args, lastValidPath);
                        return;
                    } catch (...) {
                        // Fall back to passthrough
                    }
                }

                // Fallback to source passthrough
                if (_srcClip && _srcClip->isConnected()) {
                    std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
                    if (src.get()) {
                        copyPixelData(src.get(), dst.get());
                    }
                }
            }
            break;

        default:
            // Default to passthrough
            if (_srcClip && _srcClip->isConnected()) {
                std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
                if (src.get()) {
                    copyPixelData(src.get(), dst.get());
                }
            }
            break;
    }
}

void BasePlugin::loadCachedResult(const OFX::RenderArguments &args, const std::string& cachedPath)
{
    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    if (!dst.get()) {
        throw std::runtime_error("Failed to fetch destination image");
    }

    // Get FULL image dimensions from Region of Definition, not just render window
    // IMPORTANT: getBounds() returns the render window (portion being rendered),
    // but cached EXR files are always full resolution, so we must compare against RoD
    OfxRectD rod = _srcClip->getRegionOfDefinition(args.time);
    int expectedWidth = static_cast<int>(rod.x2 - rod.x1);
    int expectedHeight = static_cast<int>(rod.y2 - rod.y1);

    if (_logger) {
        OfxRectI renderWindow = dst->getBounds();
        _logger->debug("Frame {}: Full image (RoD)={}x{}, Render window=({},{} to {},{})",
                      static_cast<int>(args.time),
                      expectedWidth, expectedHeight,
                      renderWindow.x1, renderWindow.y1,
                      renderWindow.x2, renderWindow.y2);
    }

    // Read EXR file
    ImageData outputImageData = ImageIO::readEXR(cachedPath);

    // CRITICAL: Validate that cached image dimensions match FULL image dimensions
    // Note: We compare against RoD, not render window
    if (outputImageData.width != expectedWidth || outputImageData.height != expectedHeight) {
        std::ostringstream error;
        error << "Cached image dimensions (" << outputImageData.width << "x" << outputImageData.height
              << ") do not match expected full image dimensions (" << expectedWidth << "x" << expectedHeight << "). "
              << "This can happen when:\n"
              << "  1. Timeline/project resolution changed after cache was created\n"
              << "  2. Source clip resolution changed\n"
              << "  3. Render resolution settings changed\n"
              << "Cache file: " << cachedPath << "\n"
              << "SOLUTION: The cached file will be invalidated and re-rendered at the correct resolution.";

        if (_logger) {
            _logger->warn("Frame {}: {}", static_cast<int>(args.time), error.str());
        }

        throw std::runtime_error(error.str());
    }

    // Get render window bounds - we may only need to render a portion
    OfxRectI renderWindow = dst->getBounds();
    int renderWidth = renderWindow.x2 - renderWindow.x1;
    int renderHeight = renderWindow.y2 - renderWindow.y1;

    // Check if we're rendering the full image or just a portion
    bool isFullFrameRender = (renderWindow.x1 == 0 && renderWindow.y1 == 0 &&
                             renderWidth == expectedWidth && renderHeight == expectedHeight);

    if (_logger) {
        _logger->debug("Frame {}: Cached image={}x{}, Render request={}x{}, Full frame={}",
                      static_cast<int>(args.time),
                      outputImageData.width, outputImageData.height,
                      renderWidth, renderHeight,
                      isFullFrameRender);
    }

    // If rendering a sub-region, extract that portion from the cached image
    ImageData regionData = outputImageData;
    if (!isFullFrameRender) {
        // Extract the render window region from the full cached image
        regionData.width = renderWidth;
        regionData.height = renderHeight;

        std::vector<float> extractedPixels;
        extractedPixels.reserve(renderWidth * renderHeight * outputImageData.channels);

        for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
            for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
                int srcIdx = (y * outputImageData.width + x) * outputImageData.channels;
                for (int c = 0; c < outputImageData.channels; ++c) {
                    extractedPixels.push_back(outputImageData.pixels[srcIdx + c]);
                }
            }
        }

        regionData.pixels = std::move(extractedPixels);

        if (_logger) {
            _logger->debug("Frame {}: Extracted region ({},{}) to ({},{}) from cached image",
                          static_cast<int>(args.time),
                          renderWindow.x1, renderWindow.y1,
                          renderWindow.x2, renderWindow.y2);
        }
    }

    // Convert to OFX buffer
    int dstPixelComponents = dst->getPixelComponentCount();
    OFX::BitDepthEnum dstBitDepth = dst->getPixelDepth();
    int dstBitDepthInt = 32;
    if (dstBitDepth == OFX::eBitDepthUByte) dstBitDepthInt = 8;
    else if (dstBitDepth == OFX::eBitDepthUShort) dstBitDepthInt = 16;

    ImageIO::toOFXBuffer(regionData, dst->getPixelData(), dst->getRowBytes(),
                         dstPixelComponents, dstBitDepthInt);
}

void BasePlugin::onJobComplete(int frame, bool success)
{
    if (_logger) {
        _logger->info("========================================");
        _logger->info("Job completion callback: Frame {} - {}", frame, success ? "SUCCESS" : "FAILED");
        _logger->info("========================================");
    }

    if (!success) {
        if (_jobManager) {
            std::string error = _jobManager->getJobError(frame);
            if (_logger) _logger->error("Frame {} error: {}", frame, error);
        }
        return;
    }

    // SUCCESS - Add output file to in-memory cache for fast future lookups
    std::string cachedPath = constructExpectedOutputPath(frame);
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        _cacheFileExists.insert(cachedPath);
        if (_logger) {
            _logger->info("Frame {}: Added to in-memory cache: {}", frame, cachedPath);
        }
    }

    // SUCCESS - Invalidate host cache to trigger re-render
    // Strategy: Modify hidden refresh trigger parameter
    try {
        double currentValue = 0.0;
        _refreshTrigger->getValueAtTime(frame, currentValue);
        _refreshTrigger->setValueAtTime(frame, currentValue + 0.001);

        if (_logger) {
            _logger->info("Frame {}: Cache invalidation triggered (refresh: {} -> {})",
                         frame, currentValue, currentValue + 0.001);
        }
    } catch (const std::exception& e) {
        if (_logger) {
            _logger->error("Frame {}: Failed to invalidate cache: {}", frame, e.what());
        }
    }

    // Update job status display
    updateJobStatusDisplay();
}

void BasePlugin::updateJobStatusDisplay()
{
    if (!_jobManager || !_jobStatus) return;

    try {
        int pendingCount = _jobManager->getPendingJobCount();
        std::vector<int> pendingFrames = _jobManager->getPendingFrames();
        std::vector<int> failedFrames = _jobManager->getFailedFrames();

        std::string statusText;

        // Show failed frames first (most important)
        if (!failedFrames.empty()) {
            statusText = "FAILED frames: ";
            for (size_t i = 0; i < std::min(failedFrames.size(), size_t(5)); ++i) {
                if (i > 0) statusText += ", ";
                statusText += std::to_string(failedFrames[i]);
            }
            if (failedFrames.size() > 5) {
                statusText += " (+" + std::to_string(failedFrames.size() - 5) + " more)";
            }
            statusText += " | ";
        }

        // Then show pending frames
        if (pendingCount == 0) {
            if (failedFrames.empty()) {
                statusText = "No jobs pending";
            } else {
                statusText += "No jobs pending";
            }
        } else if (pendingCount <= 5) {
            statusText += "Pending: ";
            for (size_t i = 0; i < pendingFrames.size(); ++i) {
                if (i > 0) statusText += ", ";
                statusText += std::to_string(pendingFrames[i]);
            }
        } else {
            statusText += std::to_string(pendingCount) + " frames pending";
        }

        _jobStatus->setValue(statusText);

    } catch (const std::exception& e) {
        if (_logger) _logger->error("Failed to update job status display: {}", e.what());
    }
}

void BasePlugin::renderCheckerboard(OFX::Image* dst)
{
    if (!dst) return;

    OfxRectI bounds = dst->getBounds();
    int width = bounds.x2 - bounds.x1;
    int height = bounds.y2 - bounds.y1;
    int rowBytes = dst->getRowBytes();
    int pixelComponents = dst->getPixelComponentCount();
    OFX::BitDepthEnum bitDepth = dst->getPixelDepth();

    // Calculate bytes per pixel component
    int bytesPerComponent = 1;  // default for UByte
    if (bitDepth == OFX::eBitDepthUShort) {
        bytesPerComponent = 2;
    } else if (bitDepth == OFX::eBitDepthFloat) {
        bytesPerComponent = 4;
    }

    unsigned char* pixels = static_cast<unsigned char*>(dst->getPixelData());

    // Checkerboard pattern (32x32 squares for better visibility)
    int squareSize = 32;

    for (int y = 0; y < height; ++y) {
        unsigned char* row = pixels + y * rowBytes;
        for (int x = 0; x < width; ++x) {
            bool dark = ((x / squareSize) + (y / squareSize)) % 2 == 0;
            float value = dark ? 0.4f : 0.6f;

            unsigned char* pixel = row + x * pixelComponents * bytesPerComponent;

            if (bitDepth == OFX::eBitDepthUByte) {
                unsigned char byteValue = static_cast<unsigned char>(value * 255);
                for (int c = 0; c < pixelComponents; ++c) {
                    pixel[c] = byteValue;
                }
            } else if (bitDepth == OFX::eBitDepthUShort) {
                unsigned short* shortPixel = reinterpret_cast<unsigned short*>(pixel);
                unsigned short shortValue = static_cast<unsigned short>(value * 65535);
                for (int c = 0; c < pixelComponents; ++c) {
                    shortPixel[c] = shortValue;
                }
            } else {  // float
                float* floatPixel = reinterpret_cast<float*>(pixel);
                for (int c = 0; c < pixelComponents; ++c) {
                    floatPixel[c] = value;
                }
            }
        }
    }
}

void BasePlugin::renderSolidColor(OFX::Image* dst, double r, double g, double b)
{
    if (!dst) return;

    OfxRectI bounds = dst->getBounds();
    int width = bounds.x2 - bounds.x1;
    int height = bounds.y2 - bounds.y1;
    int rowBytes = dst->getRowBytes();
    int pixelComponents = dst->getPixelComponentCount();
    OFX::BitDepthEnum bitDepth = dst->getPixelDepth();

    // Calculate bytes per pixel component
    int bytesPerComponent = 1;  // default for UByte
    if (bitDepth == OFX::eBitDepthUShort) {
        bytesPerComponent = 2;
    } else if (bitDepth == OFX::eBitDepthFloat) {
        bytesPerComponent = 4;
    }

    unsigned char* pixels = static_cast<unsigned char*>(dst->getPixelData());

    for (int y = 0; y < height; ++y) {
        unsigned char* row = pixels + y * rowBytes;
        for (int x = 0; x < width; ++x) {
            unsigned char* pixel = row + x * pixelComponents * bytesPerComponent;

            if (bitDepth == OFX::eBitDepthUByte) {
                pixel[0] = static_cast<unsigned char>(r * 255);
                if (pixelComponents > 1) pixel[1] = static_cast<unsigned char>(g * 255);
                if (pixelComponents > 2) pixel[2] = static_cast<unsigned char>(b * 255);
                if (pixelComponents > 3) pixel[3] = 255;  // Alpha = 1.0
            } else if (bitDepth == OFX::eBitDepthUShort) {
                unsigned short* shortPixel = reinterpret_cast<unsigned short*>(pixel);
                shortPixel[0] = static_cast<unsigned short>(r * 65535);
                if (pixelComponents > 1) shortPixel[1] = static_cast<unsigned short>(g * 65535);
                if (pixelComponents > 2) shortPixel[2] = static_cast<unsigned short>(b * 65535);
                if (pixelComponents > 3) shortPixel[3] = 65535;
            } else {  // float
                float* floatPixel = reinterpret_cast<float*>(pixel);
                floatPixel[0] = static_cast<float>(r);
                if (pixelComponents > 1) floatPixel[1] = static_cast<float>(g);
                if (pixelComponents > 2) floatPixel[2] = static_cast<float>(b);
                if (pixelComponents > 3) floatPixel[3] = 1.0f;
            }
        }
    }
}

int BasePlugin::findLastValidFrame(double currentTime)
{
    int currentFrame = static_cast<int>(currentTime);

    // Search backwards for the nearest completed frame
    for (int f = currentFrame - 1; f >= currentFrame - 100 && f >= 0; --f) {
        std::string path = constructExpectedOutputPath(f);
        std::ifstream testFile(path);
        if (testFile.good()) {
            testFile.close();
            return f;
        }
    }

    return -1;  // No valid frame found
}

// ============================================================================
// Parameter Description
// ============================================================================

void BasePlugin::describeCommonParameters(OFX::ImageEffectDescriptor &desc,
                                          OFX::ContextEnum /*context*/,
                                          OFX::PageParamDescriptor *page,
                                          OFX::PageParamDescriptor * /*unused2*/,
                                          OFX::PageParamDescriptor * /*unused3*/)
{
    // Single page with groups for Flame compatibility

    // ==== PROJECT GROUP ====
    OFX::GroupParamDescriptor *projectGroup = desc.defineGroupParam("projectGroup");
    projectGroup->setLabel("Project");
    projectGroup->setOpen(true);
    page->addChild(*projectGroup);

    // Project name (REQUIRED parameter)
    OFX::StringParamDescriptor *project = desc.defineStringParam("projectName");
    project->setLabel("Project Name (REQUIRED)");
    project->setHint("REQUIRED: Project name for organizing files (in/<PROJECT>/<WORKFLOW>/). Plugin will not process if empty.");
    project->setDefault("");  // No default - user must set this
    project->setEvaluateOnChange(true);  // Trigger updates when changed
    project->setParent(*projectGroup);
    page->addChild(*project);

    // Visual status indicator - RED when empty, BLACK when set
    OFX::RGBParamDescriptor *projectIndicator = desc.defineRGBParam("projectNameIndicator");
    projectIndicator->setLabel("Project Status");
    projectIndicator->setHint("RED = Project name required. BLACK = Project name is set.");
    projectIndicator->setDefault(1.0, 0.0, 0.0);  // Start with RED (warning)
    projectIndicator->setAnimates(false);  // Not animatable
    projectIndicator->setEvaluateOnChange(false);  // Don't trigger renders
    projectIndicator->setParent(*projectGroup);
    page->addChild(*projectIndicator);

    OFX::StringParamDescriptor *workflow = desc.defineStringParam("workflowName");
    workflow->setLabel("Workflow Name");
    workflow->setHint("Workflow subdirectory name (e.g., 'segmentation', 'upscale')");
    workflow->setDefault("segmentation");
    workflow->setParent(*projectGroup);
    page->addChild(*workflow);

    OFX::StringParamDescriptor *version = desc.defineStringParam("outputVersion");
    version->setLabel("Output Version");
    version->setHint("Version identifier for output files (e.g., 'v001', 'v002')");
    version->setDefault("v001");
    version->setParent(*projectGroup);
    page->addChild(*version);

    OFX::StringParamDescriptor *workflowFile = desc.defineStringParam("workflowFilePath");
    workflowFile->setLabel("Workflow File");
    workflowFile->setHint("Path to workflow JSON file. Use 'resources/workflows/filename.json' for bundled workflows, or absolute path for custom workflows. Leave empty to use hardcoded workflow.");
    workflowFile->setStringType(OFX::eStringTypeFilePath);
    workflowFile->setDefault("resources/workflows/sam_segmentation.json");
    workflowFile->setParent(*projectGroup);
    page->addChild(*workflowFile);

    // ==== PROCESSING GROUP ====
    OFX::GroupParamDescriptor *processingGroup = desc.defineGroupParam("processingGroup");
    processingGroup->setLabel("Processing");
    processingGroup->setOpen(true);
    page->addChild(*processingGroup);

    // Master enable/disable checkbox
    OFX::BooleanParamDescriptor *enableProcessing = desc.defineBooleanParam("enableProcessing");
    enableProcessing->setLabel("Enable ComfyUI Processing");
    enableProcessing->setHint("Enable or disable ComfyUI workflow execution. When disabled, input is passed through unchanged. DISABLE THIS during plugin initialization to prevent blocking UI load!");
    enableProcessing->setDefault(false);  // DEFAULT TO OFF for smooth UI loading
    enableProcessing->setAnimates(false);
    enableProcessing->setParent(*processingGroup);
    page->addChild(*enableProcessing);

    // Async rendering mode (hidden - always non-blocking)
    OFX::ChoiceParamDescriptor *asyncMode = desc.defineChoiceParam("asyncMode");
    asyncMode->setLabel("Rendering Mode");
    asyncMode->setHint("Blocking: Traditional behavior (UI freezes, waits for result). "
                       "Non-Blocking: Progressive rendering (returns placeholder immediately, updates when ready)");
    asyncMode->appendOption("Blocking (Wait for Result)");
    asyncMode->appendOption("Non-Blocking (Progressive)");
    asyncMode->setDefault(1);  // Non-blocking by default for better UX
    asyncMode->setAnimates(false);
    asyncMode->setIsSecret(true);  // Hidden from UI
    asyncMode->setParent(*processingGroup);
    // page->addChild(*asyncMode);  // Removed from UI

    // Placeholder display mode (hidden - always checkerboard)
    OFX::ChoiceParamDescriptor *placeholderMode = desc.defineChoiceParam("placeholderMode");
    placeholderMode->setLabel("Placeholder Display");
    placeholderMode->setHint("What to show while ComfyUI is processing in non-blocking mode");
    placeholderMode->appendOption("Source Passthrough");
    placeholderMode->appendOption("Checkerboard Pattern");
    placeholderMode->appendOption("Gray Frame");
    placeholderMode->appendOption("Last Valid Result");
    placeholderMode->setDefault(1);  // Checkerboard pattern
    placeholderMode->setAnimates(false);
    placeholderMode->setIsSecret(true);  // Hidden from UI
    placeholderMode->setParent(*processingGroup);
    // page->addChild(*placeholderMode);  // Removed from UI

    // Job status display (read-only)
    OFX::StringParamDescriptor *jobStatus = desc.defineStringParam("jobStatus");
    jobStatus->setLabel("Job Status");
    jobStatus->setHint("Current async job queue status");
    jobStatus->setStringType(OFX::eStringTypeLabel);  // Read-only label
    jobStatus->setDefault("No jobs pending");
    jobStatus->setEnabled(false);  // Read-only
    jobStatus->setParent(*processingGroup);
    page->addChild(*jobStatus);

    // Hidden refresh trigger for cache invalidation
    OFX::DoubleParamDescriptor *refreshTrigger = desc.defineDoubleParam("refreshTrigger");
    refreshTrigger->setLabel("Refresh Trigger");
    refreshTrigger->setIsSecret(true);  // Hidden from UI
    refreshTrigger->setAnimates(true);  // Can be keyframed per-frame
    refreshTrigger->setDefault(0.0);
    refreshTrigger->setParent(*processingGroup);
    page->addChild(*refreshTrigger);

    OFX::BooleanParamDescriptor *enableCache = desc.defineBooleanParam("enableCache");
    enableCache->setLabel("Enable Cache");
    enableCache->setHint("Use ComfyUI's caching system to speed up repeated renders");
    enableCache->setDefault(true);
    enableCache->setParent(*processingGroup);
    page->addChild(*enableCache);

    OFX::IntParamDescriptor *timeout = desc.defineIntParam("timeout");
    timeout->setLabel("Timeout (s)");
    timeout->setHint("Maximum time to wait for ComfyUI processing");
    timeout->setDefault(300);
    timeout->setRange(10, 3600);
    timeout->setDisplayRange(30, 600);
    timeout->setParent(*processingGroup);
    page->addChild(*timeout);

    // ==== SERVER GROUP ====
    OFX::GroupParamDescriptor *serverGroup = desc.defineGroupParam("serverGroup");
    serverGroup->setLabel("Server");
    serverGroup->setOpen(false);  // Collapsed by default
    page->addChild(*serverGroup);

    OFX::StringParamDescriptor *serverAddr = desc.defineStringParam("serverAddress");
    serverAddr->setLabel("Server Address");
    serverAddr->setHint("Hostname or IP address of ComfyUI server");
    serverAddr->setDefault("localhost");
    serverAddr->setParent(*serverGroup);
    page->addChild(*serverAddr);

    OFX::IntParamDescriptor *serverPort = desc.defineIntParam("serverPort");
    serverPort->setLabel("Port");
    serverPort->setHint("ComfyUI server port number");
    serverPort->setDefault(8188);
    serverPort->setRange(1, 65535);
    serverPort->setDisplayRange(8000, 9000);
    serverPort->setParent(*serverGroup);
    page->addChild(*serverPort);

    OFX::StringParamDescriptor *mountPath = desc.defineStringParam("sharedMountPath");
    mountPath->setLabel("Client Mount Path");
    mountPath->setHint("Client-side path to shared storage (macOS: /Volumes/silo2/002_COMFYUI, Linux: /mnt/storage)");
    mountPath->setStringType(OFX::eStringTypeDirectoryPath);
    mountPath->setDefault("/Volumes/silo2/002_COMFYUI");
    mountPath->setParent(*serverGroup);
    page->addChild(*mountPath);

    OFX::StringParamDescriptor *serverMount = desc.defineStringParam("serverMountPoint");
    serverMount->setLabel("Server Mount Point");
    serverMount->setHint("Server-side mount point (Windows: Z:, Linux: /mnt/storage)");
    serverMount->setDefault("Z:");
    serverMount->setParent(*serverGroup);
    page->addChild(*serverMount);
}

} // namespace ComfyUI
