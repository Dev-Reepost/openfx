// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "anycomfy_plugin.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <cctype>
#include <filesystem>

#ifdef __APPLE__
#include <cstdlib>
#elif defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <cstdlib>
#endif

namespace fs = std::filesystem;

namespace ComfyUI {

// ============================================================================
// AnyComfyPlugin Implementation
// ============================================================================

AnyComfyPlugin::AnyComfyPlugin(OfxImageEffectHandle handle)
    : BasePlugin(handle)
    , _createNewWorkflow(nullptr)
    , _workflowsDirectory(nullptr)
{
    // Fetch AnyComfy-specific parameters
    _createNewWorkflow = fetchPushButtonParam("createNewWorkflow");
    _workflowsDirectory = fetchStringParam("workflowsDirectory");
}

AnyComfyPlugin::~AnyComfyPlugin()
{
}

void AnyComfyPlugin::changedParam(const OFX::InstanceChangedArgs &args,
                                   const std::string &paramName)
{
    // Call base class first (handles server address changes)
    BasePlugin::changedParam(args, paramName);

    // Handle AnyComfy-specific parameter changes
    if (paramName == "createNewWorkflow") {
        // Button pressed - create template workflow and open browser
        if (_logger) _logger->info("Create New Workflow button pressed");

        try {
            createTemplateWorkflow();
        } catch (const std::exception& e) {
            if (_logger) _logger->error("Failed to create template workflow: {}", e.what());
            // Could use sendMessage() to show error to user if needed
        }
    }
    else if (paramName == "workflowFilePath") {
        // Workflow file path changed - auto-derive workflow name from filename
        std::string workflowPath;
        _workflowFilePath->getValue(workflowPath);

        if (!workflowPath.empty()) {
            std::string derivedName = deriveWorkflowNameFromFilename(workflowPath);

            if (!derivedName.empty() && _workflowName) {
                // Get current workflow name to check if we should update
                std::string currentName;
                _workflowName->getValue(currentName);

                // Only auto-update if current name is the default or empty
                // This preserves user's manual changes
                if (currentName.empty() || currentName == "segmentation") {
                    _workflowName->setValue(derivedName);
                    if (_logger) {
                        _logger->info("Auto-derived workflow name from filename: '{}' -> '{}'",
                                     workflowPath, derivedName);
                    }
                } else {
                    if (_logger) {
                        _logger->debug("Workflow name not auto-updated (user has custom value: '{}')", currentName);
                    }
                }
            }
        }
    }
}

json AnyComfyPlugin::buildWorkflow(int frame, const std::string& inputPath)
{
    if (_logger) _logger->info("Building generic workflow for frame {}", frame);

    // Load workflow from file
    std::string workflowPath;
    _workflowFilePath->getValue(workflowPath);

    if (workflowPath.empty()) {
        throw std::runtime_error("No workflow file specified. Please select a workflow or create a new one.");
    }

    if (_logger) _logger->info("Loading workflow from file: {}", workflowPath);

    // Resolve the path (handles bundle resources and absolute paths)
    std::string resolvedPath = resolveWorkflowPath(workflowPath);

    if (resolvedPath.empty()) {
        throw std::runtime_error("Could not resolve workflow path: " + workflowPath);
    }

    // Load workflow from file
    json baseWorkflow = loadWorkflowFromFile(resolvedPath);

    // First, try template variable replacement (for templated workflows)
    json customized = customizeWorkflow(baseWorkflow, frame, inputPath);

    // Then, inject paths directly into LoadEXR/SaveEXR nodes (for non-templated workflows)
    // This ensures the plugin works with BOTH templated and raw ComfyUI workflows
    std::string mountPath, project, workflowName, version;
    _sharedMountPath->getValue(mountPath);
    _projectName->getValue(project);
    _workflowName->getValue(workflowName);
    _outputVersion->getValue(version);

    std::string basename = getEffectiveBasename();
    std::ostringstream outputPrefix;
    outputPrefix << mountPath << "/out/" << project << "/" << workflowName
                 << "/" << version << "/" << basename;

    json final = injectPathsIntoWorkflow(customized, frame, inputPath, outputPrefix.str());

    if (_logger) _logger->info("Successfully loaded and customized workflow from file");
    return final;
}

std::vector<std::string> AnyComfyPlugin::getRequiredModels()
{
    // Generic plugin doesn't know what models are required
    // Each workflow may use different models
    return {};
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string AnyComfyPlugin::getWorkflowsPath() const
{
    std::string sharedMount;
    _sharedMountPath->getValue(sharedMount);

    std::string workflowsDir;
    _workflowsDirectory->getValue(workflowsDir);

    // If workflowsDir is empty, default to "workflows"
    if (workflowsDir.empty()) {
        workflowsDir = "workflows";
    }

    // Build full path
    fs::path fullPath = fs::path(sharedMount) / workflowsDir;
    return fullPath.string();
}

std::vector<std::string> AnyComfyPlugin::scanWorkflowFiles()
{
    std::vector<std::string> workflows;
    std::string workflowsPath = getWorkflowsPath();

    if (_logger) _logger->info("Scanning for workflows in: {}", workflowsPath);

    try {
        if (!fs::exists(workflowsPath)) {
            if (_logger) _logger->warn("Workflows directory does not exist: {}", workflowsPath);
            return workflows;
        }

        for (const auto& entry : fs::directory_iterator(workflowsPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                workflows.push_back(entry.path().filename().string());
            }
        }

        if (_logger) _logger->info("Found {} workflow files", workflows.size());
    } catch (const std::exception& e) {
        if (_logger) _logger->error("Error scanning workflows directory: {}", e.what());
    }

    return workflows;
}

std::string AnyComfyPlugin::generateUniqueWorkflowName()
{
    // Use instance name to create unique workflow name
    // Format: anycomfy_<instance>_<timestamp>.json

    // Get current timestamp
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << "anycomfy_" << _instanceName << "_" << now << ".json";

    return oss.str();
}

void AnyComfyPlugin::createTemplateWorkflow()
{
    if (_logger) _logger->info("Creating template workflow");

    // Generate unique workflow name
    std::string workflowName = generateUniqueWorkflowName();
    std::string workflowsPath = getWorkflowsPath();

    // Ensure workflows directory exists
    if (!fs::exists(workflowsPath)) {
        if (_logger) _logger->info("Creating workflows directory: {}", workflowsPath);
        fs::create_directories(workflowsPath);
    }

    fs::path workflowPath = fs::path(workflowsPath) / workflowName;

    // Create minimal workflow with LoadEXR and SaveEXR nodes
    json workflow;

    // Node 1: LoadEXR
    workflow["1"] = {
        {"inputs", {
            {"filepath", "${INPUT_PATH}"},
            {"linear_to_sRGB", false},
            {"image_load_cap", 0},
            {"skip_first_images", 0},
            {"select_every_nth", 1}
        }},
        {"class_type", "LoadEXR"}
    };

    // Node 2: SaveEXR (linked to LoadEXR output)
    workflow["2"] = {
        {"inputs", {
            {"filename_prefix", "${OUTPUT_PREFIX}"},
            {"sRGB_to_linear", false},
            {"version", -1},
            {"start_frame", "${FRAME}"},
            {"frame_pad", 4},
            {"images", json::array({"1", 0})}  // Link to node 1, output 0
        }},
        {"class_type", "SaveEXR"}
    };

    // Add metadata comment
    workflow["_meta"] = {
        {"description", "Template workflow created by AnyComfy OFX plugin"},
        {"instance", _instanceName},
        {"created", std::time(nullptr)},
        {"note", "Add your custom nodes between LoadEXR (node 1) and SaveEXR (node 2). Make sure to maintain the image chain."}
    };

    // Write workflow to file
    try {
        std::ofstream outFile(workflowPath.string());
        if (!outFile.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + workflowPath.string());
        }

        outFile << std::setw(2) << workflow << std::endl;
        outFile.close();

        if (_logger) _logger->info("Created template workflow: {}", workflowPath.string());

        // Update the workflow file path parameter to point to the new workflow
        _workflowFilePath->setValue(workflowPath.string());

        // Open ComfyUI in browser with this workflow
        openComfyUIInBrowser(workflowPath.string());

    } catch (const std::exception& e) {
        if (_logger) _logger->error("Failed to write template workflow: {}", e.what());
        throw;
    }
}

void AnyComfyPlugin::openComfyUIInBrowser(const std::string& workflowPath)
{
    if (_logger) _logger->info("Opening ComfyUI in browser with workflow: {}", workflowPath);

    // Get server address and port
    std::string serverAddress;
    int serverPort;
    _serverAddress->getValue(serverAddress);
    _serverPort->getValue(serverPort);

    // Construct URL
    // ComfyUI doesn't have a direct URL parameter to load workflows from filesystem
    // So we just open the main ComfyUI UI and the user can load the workflow manually
    // Alternative: We could implement a workflow upload via the API
    std::ostringstream urlStream;
    urlStream << "http://" << serverAddress << ":" << serverPort;
    std::string url = urlStream.str();

    if (_logger) _logger->info("Opening URL: {}", url);
    if (_logger) _logger->info("User should load workflow from: {}", workflowPath);

    // Open browser - platform specific
#ifdef __APPLE__
    // macOS
    std::string command = "open \"" + url + "\"";
    int result = std::system(command.c_str());
    if (result != 0 && _logger) {
        _logger->warn("Failed to open browser (exit code: {})", result);
    }
#elif defined(_WIN32)
    // Windows
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
    // Linux
    std::string command = "xdg-open \"" + url + "\" &";
    int result = std::system(command.c_str());
    if (result != 0 && _logger) {
        _logger->warn("Failed to open browser (exit code: {})", result);
    }
#endif

    // Note: We could enhance this by:
    // 1. Using ComfyUI API to upload the workflow
    // 2. Getting the workflow ID from the upload response
    // 3. Opening URL with workflow ID: http://server:port/?workflow=<id>
    // This would automatically load the workflow in the UI
}

json AnyComfyPlugin::injectPathsIntoWorkflow(const json& workflow, int frame,
                                              const std::string& inputPath,
                                              const std::string& outputPrefix)
{
    if (_logger) _logger->info("Injecting paths into workflow nodes (smart injection for non-templated workflows)");

    // Create a mutable copy of the workflow
    json modifiedWorkflow = workflow;

    // Convert paths to ComfyUI format (Windows paths if needed)
    // IMPORTANT: We need the RAW Windows path WITHOUT manual JSON escaping
    // because nlohmann_json will automatically escape backslashes when serializing
    std::string clientMount, serverMount;
    _sharedMountPath->getValue(clientMount);
    _serverMountPoint->getValue(serverMount);

    // Convert input path (replace mount + forward slashes → backslashes)
    std::string comfyInputPath = inputPath;
    if (comfyInputPath.find(clientMount) == 0) {
        comfyInputPath.replace(0, clientMount.length(), serverMount);
    }
    std::replace(comfyInputPath.begin(), comfyInputPath.end(), '/', '\\');

    // Convert output prefix (replace mount + forward slashes → backslashes)
    std::string comfyOutputPrefix = outputPrefix;
    if (comfyOutputPrefix.find(clientMount) == 0) {
        comfyOutputPrefix.replace(0, clientMount.length(), serverMount);
    }
    std::replace(comfyOutputPrefix.begin(), comfyOutputPrefix.end(), '/', '\\');

    if (_logger) {
        _logger->info("Injecting input path (raw Windows): {}", comfyInputPath);
        _logger->info("Injecting output prefix (raw Windows): {}", comfyOutputPrefix);
        _logger->info("Injecting frame: {}", frame);
        _logger->info("(nlohmann_json will auto-escape backslashes in JSON output)");
    }

    int loadEXRCount = 0;
    int saveEXRCount = 0;

    // Iterate through all nodes in the workflow
    if (modifiedWorkflow.is_object()) {
        for (auto& [nodeId, nodeData] : modifiedWorkflow.items()) {
            // Skip non-object nodes or metadata
            if (!nodeData.is_object()) continue;
            if (nodeId == "_meta" || nodeId.rfind("_", 0) == 0) continue;

            // Check if node has class_type
            if (!nodeData.contains("class_type")) continue;

            std::string classType = nodeData["class_type"];

            // Find and modify LoadEXR nodes
            if (classType == "LoadEXR") {
                if (_logger) _logger->info("Found LoadEXR node: {}", nodeId);

                // Ensure inputs object exists
                if (!nodeData.contains("inputs")) {
                    nodeData["inputs"] = json::object();
                }

                // Inject the input path
                nodeData["inputs"]["filepath"] = comfyInputPath;
                loadEXRCount++;

                if (_logger) _logger->info("  → Injected filepath: {}", comfyInputPath);
            }

            // Find and modify SaveEXR nodes
            if (classType == "SaveEXR") {
                if (_logger) _logger->info("Found SaveEXR node: {}", nodeId);

                // Ensure inputs object exists
                if (!nodeData.contains("inputs")) {
                    nodeData["inputs"] = json::object();
                }

                // Inject the output prefix and frame
                nodeData["inputs"]["filename_prefix"] = comfyOutputPrefix;
                nodeData["inputs"]["start_frame"] = frame;
                // CRITICAL: Set version to -1 to prevent adding version suffix to filename
                // The version is already in the directory path (e.g., .../v001/)
                // so we don't want it duplicated in the filename
                nodeData["inputs"]["version"] = -1;
                saveEXRCount++;

                if (_logger) {
                    _logger->info("  → Injected filename_prefix: {}", comfyOutputPrefix);
                    _logger->info("  → Injected start_frame: {}", frame);
                    _logger->info("  → Injected version: -1 (no filename suffix)");
                }
            }
        }
    }

    if (_logger) {
        _logger->info("Smart injection complete: {} LoadEXR nodes, {} SaveEXR nodes modified",
                      loadEXRCount, saveEXRCount);
    }

    // Warn if no nodes were found (workflow might be invalid for AnyComfy)
    if (loadEXRCount == 0) {
        if (_logger) _logger->warn("No LoadEXR nodes found in workflow! Workflow may not have input.");
    }
    if (saveEXRCount == 0) {
        if (_logger) _logger->warn("No SaveEXR nodes found in workflow! Workflow may not produce output.");
    }

    // Log the FINAL workflow that will be submitted to ComfyUI
    if (_logger) {
        _logger->debug("=== FINAL WORKFLOW (after smart injection) ===");
        _logger->debug("Complete workflow JSON that will be sent to ComfyUI:");
        _logger->debug("{}", modifiedWorkflow.dump(2));
        _logger->debug("=== END FINAL WORKFLOW ===");
    }

    return modifiedWorkflow;
}

std::string AnyComfyPlugin::deriveWorkflowNameFromFilename(const std::string& filepath)
{
    // Extract filename from path (handles both Unix and Windows paths)
    std::string filename = filepath;
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = filepath.substr(lastSlash + 1);
    }

    // Remove .json extension
    size_t dotPos = filename.rfind(".json");
    if (dotPos != std::string::npos) {
        filename = filename.substr(0, dotPos);
    }

    // Split filename into words using multiple separators: _ - . (space)
    std::vector<std::string> words;
    std::string currentWord;

    for (char c : filename) {
        if (c == '_' || c == '-' || c == '.' || c == ' ') {
            if (!currentWord.empty()) {
                words.push_back(currentWord);
                currentWord.clear();
            }
        } else {
            currentWord += std::tolower(c);  // Convert to lowercase for comparison
        }
    }
    if (!currentWord.empty()) {
        words.push_back(currentWord);
    }

    // Filter out common keywords
    std::vector<std::string> filteredWords;
    for (const auto& word : words) {
        if (word != "workflow" && word != "wf" && word != "api" &&
            word != "comfyui" && word != "json" && word != "comfy") {
            filteredWords.push_back(word);
        }
    }

    // If all words were filtered out, use original filename (without extension)
    if (filteredWords.empty()) {
        if (_logger) {
            _logger->warn("All keywords filtered from filename '{}', using original", filename);
        }
        return filename;
    }

    // Join remaining words with underscores
    std::string result;
    for (size_t i = 0; i < filteredWords.size(); ++i) {
        if (i > 0) result += "_";
        result += filteredWords[i];
    }

    if (_logger) {
        _logger->debug("Derived workflow name: '{}' -> '{}'", filename, result);
    }

    return result;
}

// ============================================================================
// Plugin Factory Implementation
// ============================================================================

AnyComfyPluginFactory::AnyComfyPluginFactory()
    : OFX::PluginFactoryHelper<AnyComfyPluginFactory>(
        "com.comfyui.AnyComfy",     // Plugin identifier
        1,                           // Version major
        0                            // Version minor
    )
{
}

void AnyComfyPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    if (auto logger = spdlog::get("AnyComfy")) {
        logger->info("Describing AnyComfy plugin");
    }

    // Basic properties
    desc.setLabels(
        "AnyComfy",                                    // Label
        "AnyComfy",                                    // Short label
        "Generic ComfyUI Workflow Executor"            // Long label
    );

    desc.setPluginGrouping("ComfyUI");
    desc.setPluginDescription(
        "Execute any ComfyUI workflow that uses EXR input/output.\n\n"
        "This is a generic plugin that can run any ComfyUI workflow, as long as:\n"
        "• The workflow has a LoadEXR node (receives input image)\n"
        "• The workflow has a SaveEXR node (outputs result image)\n\n"
        "Features:\n"
        "• Select workflow files from shared server\n"
        "• Create template workflows with 'New Workflow' button\n"
        "• Automatically opens ComfyUI in browser for workflow editing\n"
        "• No workflow-specific parameters - configure everything in ComfyUI UI\n\n"
        "Use this when you want complete flexibility to use any ComfyUI workflow "
        "without creating a custom OFX plugin."
    );

    // Supported contexts
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedContext(OFX::eContextGeneral);

    // Supported pixel depths
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    desc.addSupportedBitDepth(OFX::eBitDepthUShort);
    desc.addSupportedBitDepth(OFX::eBitDepthUByte);

    // Flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    desc.setSupportsTiles(false);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
    desc.setRenderThreadSafety(OFX::eRenderInstanceSafe);
}

void AnyComfyPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                               OFX::ContextEnum context)
{
    if (auto logger = spdlog::get("AnyComfy")) {
        logger->info("Describing AnyComfy plugin in context");
    }

    // Source clip
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(false);
    srcClip->setIsMask(false);

    // Output clip
    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->setSupportsTiles(false);

    // Create parameter pages
    OFX::PageParamDescriptor *workflowPage = desc.definePageParam("Workflow");
    OFX::PageParamDescriptor *projectPage = desc.definePageParam("Project");
    OFX::PageParamDescriptor *processingPage = desc.definePageParam("Processing");
    OFX::PageParamDescriptor *serverPage = desc.definePageParam("Server");

    // ========================================================================
    // Workflow Page Parameters
    // ========================================================================

    // Create a group for workflow parameters
    OFX::GroupParamDescriptor *workflowGroup = desc.defineGroupParam("workflowGroup");
    workflowGroup->setLabel("Workflow Management");
    workflowGroup->setOpen(true);
    if (workflowPage) {
        workflowPage->addChild(*workflowGroup);
    }

    // Create New Workflow button
    {
        OFX::PushButtonParamDescriptor *param = desc.definePushButtonParam("createNewWorkflow");
        param->setLabels("New Workflow", "New Workflow", "Create New Workflow");
        param->setHint(
            "Create a new template workflow with LoadEXR and SaveEXR nodes.\n"
            "The workflow will be saved to the workflows directory and ComfyUI will open in your browser.\n\n"
            "Edit the workflow in ComfyUI by:\n"
            "1. Adding your custom nodes between LoadEXR and SaveEXR\n"
            "2. Maintaining the image processing chain\n"
            "3. Saving the workflow in ComfyUI\n\n"
            "The workflow file path will be automatically updated to use your new workflow."
        );
        param->setParent(*workflowGroup);
    }

    // Workflows Directory
    {
        OFX::StringParamDescriptor *param = desc.defineStringParam("workflowsDirectory");
        param->setLabels("Workflows Directory", "Workflows Dir", "Workflows Directory");
        param->setHint(
            "Directory containing workflow files (relative to shared mount path).\n"
            "Default: 'workflows'\n\n"
            "Workflow files should be .json files with LoadEXR and SaveEXR nodes."
        );
        param->setStringType(OFX::eStringTypeDirectoryPath);
        param->setDefault("workflows");
        param->setAnimates(false);
        param->setParent(*workflowGroup);
    }

    // ========================================================================
    // Use base class to define common parameters
    // ========================================================================
    BasePlugin::describeCommonParameters(desc, context, projectPage, processingPage, serverPage);

    if (auto logger = spdlog::get("AnyComfy")) {
        logger->info("AnyComfy plugin description complete");
    }
}

OFX::ImageEffect* AnyComfyPluginFactory::createInstance(OfxImageEffectHandle handle,
                                                         OFX::ContextEnum /*context*/)
{
    return new AnyComfyPlugin(handle);
}

} // namespace ComfyUI

// ============================================================================
// Plugin Registration
// ============================================================================

namespace OFX {
namespace Plugin {

void getPluginIDs(OFX::PluginFactoryArray &ids)
{
    static ComfyUI::AnyComfyPluginFactory factory;
    ids.push_back(&factory);
}

} // namespace Plugin
} // namespace OFX
