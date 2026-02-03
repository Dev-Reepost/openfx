// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "anycomfy_plugin.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <chrono>

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
    , _openWorkflow(nullptr)
    , _comfyUIInputDir(nullptr)
    , _newWorkflowName(nullptr)
    , _newWorkflowInputCount(nullptr)
{
    // Fetch AnyComfy-specific parameters
    _createNewWorkflow = fetchPushButtonParam("createNewWorkflow");
    _openWorkflow = fetchPushButtonParam("openWorkflow");
    _comfyUIInputDir = fetchStringParam("comfyUIInputDir");
    _newWorkflowName = fetchStringParam("newWorkflowName");
    _newWorkflowInputCount = fetchChoiceParam("newWorkflowInputCount");
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
    else if (paramName == "openWorkflow") {
        // Button pressed - open existing workflow in ComfyUI browser for editing
        if (_logger) _logger->info("Open Workflow button pressed");

        try {
            openExistingWorkflow();
        } catch (const std::exception& e) {
            if (_logger) _logger->error("Failed to open workflow: {}", e.what());
        }
    }
    else if (paramName == "workflowFilePath") {
        // Workflow file path changed - auto-derive workflow name from path
        std::string workflowPath;
        _workflowFilePath->getValue(workflowPath);

        if (!workflowPath.empty() && _workflowName) {
            std::string derivedName;

            // Parse the path to extract workflow name
            // If in standard location: workflows/<name>/<name>_api.json -> name = directory name
            // Otherwise: derive from filename
            fs::path filePath(workflowPath);
            fs::path parentDir = filePath.parent_path();
            std::string parentDirName = parentDir.filename().string();

            fs::path grandparentDir = parentDir.parent_path();
            std::string grandparentDirName = grandparentDir.filename().string();

            if (grandparentDirName == "workflows") {
                // Standard location: workflows/<name>/<name>_api.json
                derivedName = parentDirName;
                if (_logger) _logger->info("Auto-derived workflow name from directory: '{}'", derivedName);
            } else {
                // Custom location: derive from filename
                derivedName = deriveWorkflowNameFromFilename(workflowPath);
                if (_logger) _logger->info("Auto-derived workflow name from filename: '{}'", derivedName);
            }

            if (!derivedName.empty()) {
                _workflowName->setValue(derivedName);
                if (_logger) {
                    _logger->info("Workflow name set to: '{}'", derivedName);
                }
            }
        }
    }
}

json AnyComfyPlugin::buildWorkflow(int frame, const std::map<std::string, std::string>& inputPaths)
{
    if (_logger) _logger->info("Building generic workflow for frame {} with {} input(s)", frame, inputPaths.size());

    // Load workflow from file (should be API format: workflows/<name>/<name>_api.json)
    std::string workflowFilePath;
    _workflowFilePath->getValue(workflowFilePath);

    if (workflowFilePath.empty()) {
        throw std::runtime_error("No workflow file specified. Please select a workflow or create a new one.");
    }

    if (_logger) _logger->info("Loading workflow: {}", workflowFilePath);

    // Build absolute path from relative path
    // workflowFilePath should be like: "workflows/<name>/<name>_api.json"
    std::string comfyInputDir;
    _comfyUIInputDir->getValue(comfyInputDir);

    fs::path resolvedPath;
    bool usingApiFormat = false;

    // Check if it's an absolute path or relative path
    bool isAbsolutePath = !workflowFilePath.empty() &&
                          (workflowFilePath[0] == '/' || workflowFilePath.find(":\\") != std::string::npos);

    if (isAbsolutePath) {
        // Absolute path provided directly
        resolvedPath = workflowFilePath;
    } else {
        // Relative path - resolve against ComfyUI input directory
        if (comfyInputDir.empty()) {
            throw std::runtime_error("ComfyUI Input Directory not configured. Please set it in Server settings.");
        }
        resolvedPath = fs::path(comfyInputDir) / workflowFilePath;
    }

    // Check if the path exists and determine format
    if (!fs::exists(resolvedPath)) {
        throw std::runtime_error("Workflow file not found: " + resolvedPath.string());
    }

    // Detect if it's API format by checking filename
    std::string filename = resolvedPath.filename().string();
    usingApiFormat = (filename.find("_api.json") != std::string::npos);

    if (!usingApiFormat) {
        // User selected UI format file - try to find API version
        std::string apiPath = resolvedPath.string();
        size_t dotPos = apiPath.rfind(".json");
        if (dotPos != std::string::npos) {
            apiPath.insert(dotPos, "_api");
            if (fs::exists(apiPath)) {
                resolvedPath = apiPath;
                usingApiFormat = true;
                if (_logger) _logger->info("Found API format workflow: {}", resolvedPath.string());
            }
        }
    }

    if (usingApiFormat) {
        if (_logger) _logger->info("Using API format workflow (no conversion needed): {}", resolvedPath.string());
    } else {
        if (_logger) _logger->info("Using UI format workflow (will convert): {}", resolvedPath.string());
    }

    // Load workflow from file
    json baseWorkflow = loadWorkflowFromFile(resolvedPath.string());

    // ========================================================================
    // WORKFLOW ANALYSIS: Log workflow structure for debugging/validation
    // ========================================================================
    if (_logger) {
        _logger->info("=== WORKFLOW ANALYSIS ===");
        _logger->info("Workflow file: {}", resolvedPath.string());
        _logger->info("Format: {}", usingApiFormat ? "API" : "UI");

        // Count nodes by type
        std::map<std::string, int> nodeTypeCounts;
        int totalNodes = 0;
        int loadEXRCount = 0;
        int saveEXRCount = 0;

        if (baseWorkflow.is_object()) {
            if (baseWorkflow.contains("nodes") && baseWorkflow["nodes"].is_array()) {
                // UI format
                totalNodes = baseWorkflow["nodes"].size();
                for (const auto& node : baseWorkflow["nodes"]) {
                    if (node.contains("type")) {
                        std::string nodeType = node["type"];
                        nodeTypeCounts[nodeType]++;
                        if (nodeType == "LoadEXR") loadEXRCount++;
                        if (nodeType == "SaveEXR") saveEXRCount++;
                    }
                }
            } else {
                // API format
                for (const auto& [nodeId, nodeData] : baseWorkflow.items()) {
                    if (nodeId == "_meta" || nodeId.rfind("_", 0) == 0) continue;
                    if (!nodeData.is_object() || !nodeData.contains("class_type")) continue;

                    totalNodes++;
                    std::string classType = nodeData["class_type"];
                    nodeTypeCounts[classType]++;
                    if (classType == "LoadEXR") loadEXRCount++;
                    if (classType == "SaveEXR") saveEXRCount++;
                }
            }
        }

        _logger->info("Total nodes: {}", totalNodes);
        _logger->info("LoadEXR nodes: {} (inputs from OFX)", loadEXRCount);
        _logger->info("SaveEXR nodes: {} (outputs to OFX)", saveEXRCount);

        // Log all node types
        _logger->info("Node types in workflow:");
        for (const auto& [nodeType, count] : nodeTypeCounts) {
            _logger->info("  - {}: {}", nodeType, count);
        }

        // Validation warnings
        _logger->info("--- Validation ---");
        _logger->info("Connected OFX inputs: {} (InputA={}, InputB={}, InputC={})",
                     inputPaths.size(),
                     inputPaths.count("InputA") ? "yes" : "no",
                     inputPaths.count("InputB") ? "yes" : "no",
                     inputPaths.count("InputC") ? "yes" : "no");

        if (loadEXRCount == 0 && inputPaths.size() > 0) {
            _logger->warn("⚠ GENERATOR WORKFLOW: No LoadEXR nodes but {} OFX input(s) connected", inputPaths.size());
            _logger->warn("  Input images will be written but not used by workflow");
        } else if (loadEXRCount < static_cast<int>(inputPaths.size())) {
            _logger->warn("⚠ INPUT MISMATCH: {} LoadEXR nodes but {} OFX inputs connected", loadEXRCount, inputPaths.size());
            _logger->warn("  Some connected inputs will not be used");
        } else if (loadEXRCount > static_cast<int>(inputPaths.size())) {
            _logger->warn("⚠ INPUT MISMATCH: {} LoadEXR nodes but only {} OFX input(s) connected", loadEXRCount, inputPaths.size());
            _logger->warn("  Some LoadEXR nodes will have no input path");
        } else if (loadEXRCount == static_cast<int>(inputPaths.size())) {
            _logger->info("✓ Input count matches: {} LoadEXR node(s) = {} OFX input(s)", loadEXRCount, inputPaths.size());
        }

        if (saveEXRCount == 0) {
            _logger->error("✗ NO OUTPUT: Workflow has no SaveEXR nodes!");
            _logger->error("  Workflow will not produce any output image");
        } else if (saveEXRCount > 1) {
            _logger->warn("⚠ MULTIPLE OUTPUTS: Workflow has {} SaveEXR nodes", saveEXRCount);
            _logger->warn("  Only one output will be used by OFX plugin");
        } else {
            _logger->info("✓ Single output: 1 SaveEXR node");
        }

        _logger->info("=== END WORKFLOW ANALYSIS ===");
    }

    // First, try template variable replacement (for templated workflows)
    json customized = customizeWorkflow(baseWorkflow, frame, inputPaths);

    // Convert UI format to API format BEFORE injecting paths
    // This is critical because injectPathsIntoWorkflow expects API format
    if (!usingApiFormat && isUIFormat(customized)) {
        if (_logger) _logger->info("Converting UI format to API format for execution");
        customized = convertUIFormatToAPI(customized);
    } else if (usingApiFormat) {
        if (_logger) _logger->info("Using API format directly (no conversion needed)");
    }

    // Then, inject paths directly into LoadEXR/SaveEXR nodes
    // This ensures the plugin works with BOTH templated and raw ComfyUI workflows
    // CRITICAL: This must happen AFTER UI→API conversion because it expects API format
    std::string mountPath, project, workflowName, version;
    _macMountPath->getValue(mountPath);
    _projectName->getValue(project);
    _workflowName->getValue(workflowName);
    _outputVersion->getValue(version);

    std::string basename = getEffectiveBasename();
    std::ostringstream outputPrefix;
    outputPrefix << mountPath << "/out/" << project << "/" << workflowName
                 << "/" << version << "/" << basename;

    json final = injectPathsIntoWorkflow(customized, frame, inputPaths, outputPrefix.str());

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
    // Workflows now live in: <COMFYUI_INPUT_PATH>/workflows/
    // Each workflow has its own subdirectory: <COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/
    std::string comfyInputDir;
    _comfyUIInputDir->getValue(comfyInputDir);

    if (comfyInputDir.empty()) {
        // Fallback to shared mount path if input dir not configured
        std::string sharedMount;
        _macMountPath->getValue(sharedMount);
        return (fs::path(sharedMount) / "workflows").string();
    }

    // Build full path: <COMFYUI_INPUT_PATH>/workflows/
    fs::path fullPath = fs::path(comfyInputDir) / "workflows";
    return fullPath.string();
}

std::string AnyComfyPlugin::getWorkflowPath(const std::string& workflowName) const
{
    // Workflow path: <COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/<WORKFLOW_NAME>.json
    fs::path workflowsPath = getWorkflowsPath();
    fs::path workflowDir = workflowsPath / workflowName;
    fs::path workflowFile = workflowDir / (workflowName + ".json");
    return workflowFile.string();
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

        // New structure: each workflow is a subdirectory containing <name>.json
        // Structure: <COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/<WORKFLOW_NAME>.json
        for (const auto& entry : fs::directory_iterator(workflowsPath)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();
                // Check if the expected workflow file exists in this directory
                fs::path expectedWorkflow = entry.path() / (dirName + ".json");
                fs::path expectedApiWorkflow = entry.path() / (dirName + "_api.json");

                if (fs::exists(expectedWorkflow) || fs::exists(expectedApiWorkflow)) {
                    workflows.push_back(dirName);
                    if (_logger) _logger->debug("Found workflow: {}", dirName);
                }
            }
        }

        if (_logger) _logger->info("Found {} workflows", workflows.size());
    } catch (const std::exception& e) {
        if (_logger) _logger->error("Error scanning workflows directory: {}", e.what());
    }

    return workflows;
}

std::string AnyComfyPlugin::generateUniqueWorkflowName()
{
    // Generate unique workflow name using instance name and timestamp
    // Format: <instanceName>_<timestamp>
    // Note: .json extension is removed here (added later during workflow creation)

    // Get current timestamp
    auto now = std::time(nullptr);
    std::ostringstream oss;

    // Use instance name if available, otherwise use "workflow"
    if (!_instanceName.empty()) {
        oss << _instanceName << "_" << now;
    } else {
        oss << "workflow_" << now;
    }

    return oss.str();
}

std::string AnyComfyPlugin::getEffectiveBasename()
{
    // Override base class to avoid redundant instance name in basename
    // Since workflow name already contains instance identifier (e.g., "AnyComfy_1769610105"),
    // we generate: {project}_{workflow} instead of {project}_{workflow}_{instance}
    // This avoids: "testinputs_AnyComfy_1769610105_AnyComfy" → "testinputs_AnyComfy_1769610105"

    std::string project;
    _projectName->getValue(project);

    std::string workflow;
    _workflowName->getValue(workflow);

    // Sanitize workflow name: replace non-alphanumeric characters with underscores
    std::string sanitizedWorkflow = workflow;
    for (char& c : sanitizedWorkflow) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            c = '_';
        }
    }

    // Remove leading/trailing underscores
    while (!sanitizedWorkflow.empty() && sanitizedWorkflow.front() == '_') {
        sanitizedWorkflow.erase(0, 1);
    }
    while (!sanitizedWorkflow.empty() && sanitizedWorkflow.back() == '_') {
        sanitizedWorkflow.pop_back();
    }

    // Generate basename: {project}_{workflow} (no instance name)
    std::string generatedBasename;
    if (!sanitizedWorkflow.empty()) {
        generatedBasename = project + "_" + sanitizedWorkflow;
    } else {
        // Fallback if no workflow name
        generatedBasename = project;
    }

    if (_logger) {
        _logger->info("Auto-generated AnyComfy basename: {} (project='{}', workflow='{}')",
                     generatedBasename, project, sanitizedWorkflow);
    }

    return generatedBasename;
}

void AnyComfyPlugin::createTemplateWorkflow()
{
    if (_logger) _logger->info("Creating template workflow");

    // Get workflow name from user or generate unique name
    std::string workflowName;
    std::string userSpecifiedName;
    _newWorkflowName->getValue(userSpecifiedName);

    if (!userSpecifiedName.empty()) {
        // User specified a name - use it (strip .json if present, we'll add it back)
        workflowName = userSpecifiedName;
        // Remove .json extension if present (we use the name for the directory)
        size_t jsonPos = workflowName.rfind(".json");
        if (jsonPos != std::string::npos) {
            workflowName = workflowName.substr(0, jsonPos);
        }
        if (_logger) _logger->info("Using user-specified workflow name: {}", workflowName);

        // Clear the field so it's ready for next time
        _newWorkflowName->setValue("");
    } else {
        // Generate unique workflow name automatically (without extension)
        std::string fullName = generateUniqueWorkflowName();
        size_t jsonPos = fullName.rfind(".json");
        if (jsonPos != std::string::npos) {
            workflowName = fullName.substr(0, jsonPos);
        } else {
            workflowName = fullName;
        }
        if (_logger) _logger->info("Generated unique workflow name: {}", workflowName);
    }

    // New directory structure: <COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/<WORKFLOW_NAME>.json
    std::string workflowsPath = getWorkflowsPath();

    // Ensure base workflows directory exists
    if (!fs::exists(workflowsPath)) {
        if (_logger) _logger->info("Creating workflows directory: {}", workflowsPath);
        fs::create_directories(workflowsPath);
    }

    // Create workflow subdirectory: <COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/
    fs::path workflowDir = fs::path(workflowsPath) / workflowName;
    if (!fs::exists(workflowDir)) {
        if (_logger) _logger->info("Creating workflow subdirectory: {}", workflowDir.string());
        fs::create_directories(workflowDir);
    }

    // Workflow file path: <COMFYUI_INPUT_PATH>/workflows/<WORKFLOW_NAME>/<WORKFLOW_NAME>.json
    fs::path workflowFilePath = workflowDir / (workflowName + ".json");

    // Get selected input count from choice parameter
    int inputCount = 1;  // Default to 1 input (standard filter workflow)
    if (_newWorkflowInputCount) {
        _newWorkflowInputCount->getValue(inputCount);
        if (_logger) _logger->info("User selected {} input(s) for new workflow", inputCount);
    }

    // Select appropriate template based on input count
    std::string templateFilename;
    switch (inputCount) {
        case 0:
            templateFilename = "workflows/template_0inputs.json";
            break;
        case 1:
            templateFilename = "workflows/template.json";  // Standard 1-input template
            break;
        case 2:
            templateFilename = "workflows/template_2inputs.json";
            break;
        case 3:
            templateFilename = "workflows/template_3inputs.json";
            break;
        default:
            templateFilename = "workflows/template.json";
            if (_logger) _logger->warn("Invalid input count {}, defaulting to 1-input template", inputCount);
            break;
    }

    // Load template workflow from bundle resources
    json workflow;
    std::string templatePath = getBundleResourcePath(templateFilename);

    if (templatePath.empty() || !fs::exists(templatePath)) {
        if (_logger) _logger->error("Template file not found at: {}", templatePath);
        throw std::runtime_error("Template workflow file not found in bundle resources: " + templateFilename);
    }

    if (_logger) _logger->info("Loading {}-input template from: {}", inputCount, templatePath);

    try {
        std::ifstream templateFile(templatePath);
        if (!templateFile.is_open()) {
            throw std::runtime_error("Failed to open template file: " + templatePath);
        }

        templateFile >> workflow;
        templateFile.close();

        if (_logger) _logger->info("Template loaded successfully with {} nodes", workflow.size());
    } catch (const std::exception& e) {
        if (_logger) _logger->error("Failed to parse template file: {}", e.what());
        throw std::runtime_error("Failed to load template workflow: " + std::string(e.what()));
    }

    // Write workflow files (both UI and API formats)
    try {
        // Check if workflow already exists and warn
        bool workflowExists = fs::exists(workflowFilePath);
        if (workflowExists && _logger) {
            _logger->warn("Workflow '{}' already exists and will be overwritten", workflowName);
        }

        // Write UI format (for editing in ComfyUI)
        std::ofstream outFile(workflowFilePath.string());
        if (!outFile.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + workflowFilePath.string());
        }

        outFile << std::setw(2) << workflow << std::endl;
        outFile.flush();  // Ensure data is written to disk
        outFile.close();

        // Verify UI file was written correctly
        if (!fs::exists(workflowFilePath) || fs::file_size(workflowFilePath) == 0) {
            throw std::runtime_error("Failed to write UI format workflow file: " + workflowFilePath.string());
        }

        if (_logger) _logger->info("Created UI format workflow: {} ({} bytes)",
                                   workflowFilePath.string(), fs::file_size(workflowFilePath));

        // Write API format (same as UI format for template)
        fs::path apiWorkflowFilePath = workflowDir / (workflowName + "_api.json");
        std::ofstream apiOutFile(apiWorkflowFilePath.string());
        if (!apiOutFile.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + apiWorkflowFilePath.string());
        }

        apiOutFile << std::setw(2) << workflow << std::endl;
        apiOutFile.flush();  // Ensure data is written to disk
        apiOutFile.close();

        // Verify API file was written correctly
        if (!fs::exists(apiWorkflowFilePath) || fs::file_size(apiWorkflowFilePath) == 0) {
            throw std::runtime_error("Failed to write API format workflow file: " + apiWorkflowFilePath.string());
        }

        if (_logger) _logger->info("Created API format workflow: {} ({} bytes)",
                                   apiWorkflowFilePath.string(), fs::file_size(apiWorkflowFilePath));

        // Update the workflow file path parameter to point to the API workflow file
        // This is what will be used for execution
        std::string apiFilePath = "workflows/" + workflowName + "/" + workflowName + "_api.json";
        _workflowFilePath->setValue(apiFilePath);

        // Also update the workflow name parameter (used for output directory)
        if (_workflowName) {
            _workflowName->setValue(workflowName);
        }

        if (_logger) {
            _logger->info("Workflow created at: {}", workflowDir.string());
            _logger->info("  - UI format: {}", workflowFilePath.filename().string());
            _logger->info("  - API format: {}", apiWorkflowFilePath.filename().string());
            _logger->info("Workflow file path set to: {}", apiFilePath);
        }

        // Small delay to ensure filesystem sync before opening browser
        // This prevents race condition where browser loads old file content
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Verify UI file can be read back correctly
        try {
            std::ifstream verifyFile(workflowFilePath);
            json verifyContent;
            verifyFile >> verifyContent;
            if (_logger) {
                _logger->info("Verified UI file can be read back, contains {} top-level keys",
                             verifyContent.size());
            }
        } catch (const std::exception& e) {
            if (_logger) _logger->warn("Warning: Could not verify UI file content: {}", e.what());
        }

        // Open ComfyUI in browser with this workflow (will auto-load if extension is installed)
        openComfyUIInBrowser(workflowName);

    } catch (const std::exception& e) {
        if (_logger) _logger->error("Failed to write template workflow: {}", e.what());
        throw;
    }
}

void AnyComfyPlugin::openExistingWorkflow()
{
    if (_logger) _logger->info("Opening existing workflow for editing");

    // Get currently selected workflow file path (should be API format)
    std::string workflowFilePath;
    _workflowFilePath->getValue(workflowFilePath);

    if (workflowFilePath.empty()) {
        if (_logger) _logger->warn("No workflow selected to open");
        throw std::runtime_error("No workflow selected. Please select a workflow first.");
    }

    // Build absolute path from relative path
    std::string comfyInputDir;
    _comfyUIInputDir->getValue(comfyInputDir);

    fs::path apiWorkflowPath;
    bool isAbsolutePath = !workflowFilePath.empty() &&
                          (workflowFilePath[0] == '/' || workflowFilePath.find(":\\") != std::string::npos);

    if (isAbsolutePath) {
        apiWorkflowPath = workflowFilePath;
    } else {
        if (comfyInputDir.empty()) {
            throw std::runtime_error("ComfyUI Input Directory not configured. Please set it in Server settings.");
        }
        apiWorkflowPath = fs::path(comfyInputDir) / workflowFilePath;
    }

    if (!fs::exists(apiWorkflowPath)) {
        throw std::runtime_error("Workflow file not found: " + apiWorkflowPath.string());
    }

    // Derive UI path from API path
    // e.g., workflows/my_workflow/my_workflow_api.json -> workflows/my_workflow/my_workflow.json
    std::string uiPathStr = apiWorkflowPath.string();
    size_t apiPos = uiPathStr.rfind("_api.json");
    if (apiPos != std::string::npos) {
        uiPathStr.replace(apiPos, 9, ".json");  // Replace "_api.json" with ".json"
    } else {
        // Not an API file, assume it's already UI format
        uiPathStr = apiWorkflowPath.string();
    }
    fs::path uiWorkflowPath = uiPathStr;

    // Extract workflow name from path
    // If in standard location: workflows/<name>/<name>.json -> workflow name = directory name
    // Otherwise: derive from filename
    std::string workflowName;
    fs::path parentDir = uiWorkflowPath.parent_path();
    std::string parentDirName = parentDir.filename().string();
    std::string filename = uiWorkflowPath.stem().string();  // Gets filename without extension

    // Check if parent directory is "workflows" - if so, grandparent should be workflow name
    fs::path grandparentDir = parentDir.parent_path();
    std::string grandparentDirName = grandparentDir.filename().string();

    if (grandparentDirName == "workflows") {
        // Standard location: workflows/<name>/<name>.json
        workflowName = parentDirName;
        if (_logger) _logger->info("Workflow in standard location, using directory name: {}", workflowName);
    } else {
        // Custom location: derive from filename
        workflowName = filename;
        if (_logger) _logger->info("Workflow in custom location, using filename: {}", workflowName);
    }

    if (_logger) {
        _logger->info("API workflow: {}", apiWorkflowPath.string());
        _logger->info("UI workflow: {}", uiWorkflowPath.string());
        _logger->info("Workflow name: {}", workflowName);
    }

    // Prefer UI format for editing (that's what ComfyUI uses)
    if (fs::exists(uiWorkflowPath)) {
        if (_logger) _logger->info("Found UI format workflow, opening for editing");
        openComfyUIInBrowser(workflowName);
    }
    else if (fs::exists(apiWorkflowPath)) {
        // Only API format exists - try to convert to UI format
        if (_logger) _logger->info("Only API format exists, converting to UI format for editing");

        try {
            // Load API format workflow
            json apiWorkflow = loadWorkflowFromFile(apiWorkflowPath.string());

            // Convert to UI format
            json uiWorkflow = convertAPIFormatToUI(apiWorkflow);

            // Save UI format workflow
            std::ofstream outFile(uiWorkflowPath.string());
            if (!outFile.is_open()) {
                throw std::runtime_error("Failed to create UI workflow file: " + uiWorkflowPath.string());
            }
            outFile << std::setw(2) << uiWorkflow << std::endl;
            outFile.close();

            if (_logger) _logger->info("Created UI format workflow: {}", uiWorkflowPath.string());

            // Now open in browser
            openComfyUIInBrowser(workflowName);
        } catch (const std::exception& e) {
            if (_logger) _logger->error("Failed to convert API to UI format: {}", e.what());
            // Still try to open browser - user can load manually
            openComfyUIInBrowser(workflowName);
        }
    }
    else {
        if (_logger) _logger->error("Workflow not found: {}", workflowName);
        throw std::runtime_error("Workflow not found: " + workflowName);
    }
}

void AnyComfyPlugin::openComfyUIInBrowser(const std::string& workflowName)
{
    if (_logger) _logger->info("Opening ComfyUI in browser with workflow: {}", workflowName);

    // Get server address and port
    std::string serverAddress;
    int serverPort;
    _serverAddress->getValue(serverAddress);
    _serverPort->getValue(serverPort);

    // Get ComfyUI input directory
    std::string comfyInputDir;
    _comfyUIInputDir->getValue(comfyInputDir);

    // Construct URL with optional auto-load parameter
    std::ostringstream urlStream;
    urlStream << "http://" << serverAddress << ":" << serverPort;

    // If ComfyUI input directory is configured, add auto-load URL parameter
    // The workflow is loaded directly from its subdirectory (not copied)
    // This requires the OFX.AutoLoader extension to be installed in ComfyUI
    if (!comfyInputDir.empty()) {
        // Workflow path relative to input dir: workflows/<WORKFLOW_NAME>/<WORKFLOW_NAME>.json
        // The autoloader JS will load from this path and ComfyUI will save back to the same location
        std::string relativePath = "workflows/" + workflowName + "/" + workflowName + ".json";

        // Verify the workflow file exists
        fs::path workflowPath = fs::path(comfyInputDir) / "workflows" / workflowName / (workflowName + ".json");
        if (!fs::exists(workflowPath)) {
            if (_logger) _logger->warn("Workflow file not found: {}", workflowPath.string());
        }

        urlStream << "?load_local_json=" << relativePath;
        if (_logger) _logger->info("Opening URL with auto-load: {}", urlStream.str());
        if (_logger) _logger->info("Workflow will be loaded from and saved to: {}", workflowPath.string());
        if (_logger) _logger->info("Note: Requires OFX.AutoLoader extension in ComfyUI/web/extensions/");
    } else {
        if (_logger) _logger->info("Opening URL (manual load required): {}", urlStream.str());
        if (_logger) _logger->info("User should manually load workflow using Ctrl+O or drag-and-drop");
    }

    std::string url = urlStream.str();

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
}

json AnyComfyPlugin::injectPathsIntoWorkflow(const json& workflow, int frame,
                                              const std::map<std::string, std::string>& inputPaths,
                                              const std::string& outputPrefix)
{
    if (_logger) _logger->info("=== INJECTING PATHS INTO WORKFLOW ===");
    if (_logger) _logger->info("Available OFX inputs: {}", inputPaths.size());
    for (const auto& [inputId, path] : inputPaths) {
        if (_logger) _logger->info("  {} -> {}", inputId, path);
    }

    // Create a mutable copy of the workflow
    json modifiedWorkflow = workflow;

    // Get mount path info for path conversion
    std::string clientMount, serverMount;
    _macMountPath->getValue(clientMount);
    _winMountPath->getValue(serverMount);

    // Helper lambda to convert paths to ComfyUI format (Windows paths)
    auto convertPath = [&](const std::string& path) -> std::string {
        std::string converted = path;
        if (converted.find(clientMount) == 0) {
            converted.replace(0, clientMount.length(), serverMount);
        }
        std::replace(converted.begin(), converted.end(), '/', '\\');
        return converted;
    };

    // Convert output prefix
    std::string comfyOutputPrefix = convertPath(outputPrefix);

    if (_logger) {
        _logger->info("Output prefix (Windows format): {}", comfyOutputPrefix);
    }

    // ========================================================================
    // Step 1: Collect all LoadEXR nodes with their positions (for sorting)
    // ========================================================================
    struct LoadEXRNode {
        std::string nodeId;
        std::string title;      // From _meta.title or properties
        double yPos;            // Y position for vertical sorting (topmost first)
        double xPos;            // X position as secondary sort
        int numericId;          // Numeric node ID as fallback for sorting
    };

    std::vector<LoadEXRNode> loadEXRNodes;

    if (modifiedWorkflow.is_object()) {
        for (auto& [nodeId, nodeData] : modifiedWorkflow.items()) {
            if (!nodeData.is_object()) continue;
            if (nodeId == "_meta" || nodeId.rfind("_", 0) == 0) continue;
            if (!nodeData.contains("class_type")) continue;

            std::string classType = nodeData["class_type"];

            if (classType == "LoadEXR") {
                LoadEXRNode node;
                node.nodeId = nodeId;
                node.yPos = 0.0;
                node.xPos = 0.0;
                node.numericId = 0;

                // Try to parse numeric ID for fallback sorting
                try {
                    node.numericId = std::stoi(nodeId);
                } catch (...) {}

                // Try to get title from _meta.title
                if (nodeData.contains("_meta") && nodeData["_meta"].contains("title")) {
                    node.title = nodeData["_meta"]["title"];
                }

                // Try to get position from pos array (UI format embedded in API)
                // pos[0] = X coordinate, pos[1] = Y coordinate
                if (nodeData.contains("pos") && nodeData["pos"].is_array() && nodeData["pos"].size() >= 2) {
                    node.xPos = nodeData["pos"][0].get<double>();
                    node.yPos = nodeData["pos"][1].get<double>();  // Y coordinate (vertical position)
                }

                loadEXRNodes.push_back(node);

                if (_logger) {
                    _logger->info("Found LoadEXR node: id={}, title='{}', pos=({}, {}), numericId={}",
                                 node.nodeId, node.title, node.xPos, node.yPos, node.numericId);
                }
            }
        }
    }

    if (_logger) _logger->info("Total LoadEXR nodes found: {}", loadEXRNodes.size());

    // ========================================================================
    // Step 2: Sort LoadEXR nodes by VERTICAL position (topmost first, then by X, then by ID)
    // ========================================================================
    if (_logger) _logger->info("Sorting LoadEXR nodes by vertical position (top-to-bottom)...");

    std::sort(loadEXRNodes.begin(), loadEXRNodes.end(),
              [](const LoadEXRNode& a, const LoadEXRNode& b) {
                  // Primary: sort by Y position (topmost first = smallest Y value)
                  if (std::abs(a.yPos - b.yPos) > 10.0) {  // 10px threshold for "same row"
                      return a.yPos < b.yPos;
                  }
                  // Secondary: sort by X position (leftmost first if same Y)
                  if (std::abs(a.xPos - b.xPos) > 10.0) {  // 10px threshold for "same column"
                      return a.xPos < b.xPos;
                  }
                  // Tertiary: sort by numeric ID (lower first)
                  return a.numericId < b.numericId;
              });

    if (_logger && !loadEXRNodes.empty()) {
        _logger->info("LoadEXR nodes sorted (top-to-bottom):");
        for (size_t i = 0; i < loadEXRNodes.size(); ++i) {
            _logger->info("  [{}] id={}, title='{}', pos=({:.0f}, {:.0f}) [Y={:.0f}=vertical]",
                         i, loadEXRNodes[i].nodeId, loadEXRNodes[i].title,
                         loadEXRNodes[i].xPos, loadEXRNodes[i].yPos, loadEXRNodes[i].yPos);
        }
    }

    // ========================================================================
    // Step 3: Map LoadEXR nodes to input IDs (InputA, InputB, InputC)
    // ========================================================================
    // Priority:
    // 1. Match by title (if title contains "InputA", "InputB", etc.)
    // 2. Fall back to position order (top = InputA, 2nd from top = InputB, 3rd = InputC)

    if (_logger) _logger->info("--- MAPPING LoadEXR nodes to OFX inputs ---");

    const std::vector<std::string> inputOrder = {"InputA", "InputB", "InputC"};
    std::map<std::string, std::string> nodeIdToInputId;  // nodeId -> inputId

    // First pass: try to match by title
    if (_logger) _logger->info("Pass 1: Matching by title/name...");
    for (const auto& node : loadEXRNodes) {
        std::string titleUpper = node.title;
        std::transform(titleUpper.begin(), titleUpper.end(), titleUpper.begin(), ::toupper);

        for (const auto& inputId : inputOrder) {
            std::string inputIdUpper = inputId;
            std::transform(inputIdUpper.begin(), inputIdUpper.end(), inputIdUpper.begin(), ::toupper);

            // Check for various naming conventions
            if (titleUpper.find(inputIdUpper) != std::string::npos ||
                (inputId == "InputA" && (titleUpper.find("INPUT1") != std::string::npos ||
                                         titleUpper.find("SOURCE") != std::string::npos ||
                                         titleUpper.find("MAIN") != std::string::npos)) ||
                (inputId == "InputB" && (titleUpper.find("INPUT2") != std::string::npos ||
                                         titleUpper.find("SECONDARY") != std::string::npos ||
                                         titleUpper.find("BACKGROUND") != std::string::npos)) ||
                (inputId == "InputC" && (titleUpper.find("INPUT3") != std::string::npos ||
                                         titleUpper.find("TERTIARY") != std::string::npos ||
                                         titleUpper.find("FOREGROUND") != std::string::npos))) {
                // Only assign if not already assigned and input exists
                if (nodeIdToInputId.find(node.nodeId) == nodeIdToInputId.end() &&
                    inputPaths.count(inputId) > 0) {
                    nodeIdToInputId[node.nodeId] = inputId;
                    if (_logger) {
                        _logger->info("Matched LoadEXR '{}' (title='{}') to {} by title",
                                     node.nodeId, node.title, inputId);
                    }
                    break;
                }
            }
        }
    }

    // Second pass: assign remaining nodes by position order (top-to-bottom)
    if (_logger) _logger->info("Pass 2: Assigning by position order (top-to-bottom)...");
    size_t inputIndex = 0;
    for (const auto& node : loadEXRNodes) {
        if (nodeIdToInputId.find(node.nodeId) == nodeIdToInputId.end()) {
            // Find next available input
            while (inputIndex < inputOrder.size()) {
                const std::string& inputId = inputOrder[inputIndex];

                // Check if this input exists and isn't already used
                if (inputPaths.count(inputId) > 0) {
                    bool alreadyUsed = false;
                    for (const auto& [nid, iid] : nodeIdToInputId) {
                        if (iid == inputId) {
                            alreadyUsed = true;
                            break;
                        }
                    }

                    if (!alreadyUsed) {
                        nodeIdToInputId[node.nodeId] = inputId;
                        if (_logger) {
                            _logger->info("✓ Assigned LoadEXR '{}' (title='{}', Y={:.0f}) to {} by vertical position",
                                         node.nodeId, node.title, node.yPos, inputId);
                        }
                        inputIndex++;
                        break;
                    }
                }
                inputIndex++;
            }

            // If we've run out of inputs, warn
            if (nodeIdToInputId.find(node.nodeId) == nodeIdToInputId.end()) {
                if (_logger) {
                    _logger->warn("✗ LoadEXR node '{}' (title='{}') has no corresponding OFX input clip!",
                                 node.nodeId, node.title);
                }
            }
        }
    }

    // Summary of mapping
    if (_logger) {
        _logger->info("--- MAPPING SUMMARY ---");
        _logger->info("Total mappings: {}", nodeIdToInputId.size());
        for (const auto& [nodeId, inputId] : nodeIdToInputId) {
            _logger->info("  LoadEXR node '{}' → OFX {}", nodeId, inputId);
        }
    }

    // ========================================================================
    // Step 4: Inject paths into LoadEXR nodes
    // ========================================================================
    int loadEXRCount = 0;
    int saveEXRCount = 0;

    if (modifiedWorkflow.is_object()) {
        for (auto& [nodeId, nodeData] : modifiedWorkflow.items()) {
            if (!nodeData.is_object()) continue;
            if (nodeId == "_meta" || nodeId.rfind("_", 0) == 0) continue;
            if (!nodeData.contains("class_type")) continue;

            std::string classType = nodeData["class_type"];

            // Inject path into LoadEXR nodes
            if (classType == "LoadEXR") {
                if (!nodeData.contains("inputs")) {
                    nodeData["inputs"] = json::object();
                }

                // Find the input ID for this node
                auto it = nodeIdToInputId.find(nodeId);
                if (it != nodeIdToInputId.end()) {
                    const std::string& inputId = it->second;
                    auto pathIt = inputPaths.find(inputId);
                    if (pathIt != inputPaths.end()) {
                        std::string comfyInputPath = convertPath(pathIt->second);
                        nodeData["inputs"]["filepath"] = comfyInputPath;
                        loadEXRCount++;

                        if (_logger) {
                            _logger->info("LoadEXR '{}' <- {} : {}", nodeId, inputId, comfyInputPath);
                        }
                    }
                }
            }

            // Inject path into SaveEXR nodes (same as before)
            if (classType == "SaveEXR") {
                if (!nodeData.contains("inputs")) {
                    nodeData["inputs"] = json::object();
                }

                nodeData["inputs"]["filename_prefix"] = comfyOutputPrefix;
                nodeData["inputs"]["start_frame"] = frame;
                nodeData["inputs"]["version"] = -1;
                saveEXRCount++;

                if (_logger) {
                    _logger->info("SaveEXR '{}' <- prefix: {}, frame: {}", nodeId, comfyOutputPrefix, frame);
                }
            }
        }
    }

    if (_logger) {
        _logger->info("Path injection complete: {} LoadEXR nodes, {} SaveEXR nodes modified",
                      loadEXRCount, saveEXRCount);
    }

    // Warnings for mismatches
    if (loadEXRCount == 0 && inputPaths.size() > 0) {
        if (_logger) _logger->warn("No LoadEXR nodes found but {} input(s) provided! Workflow may be a generator.",
                                   inputPaths.size());
    }
    if (loadEXRCount < static_cast<int>(inputPaths.size())) {
        if (_logger) _logger->warn("Workflow has {} LoadEXR nodes but {} inputs connected. Some inputs unused.",
                                   loadEXRCount, inputPaths.size());
    }
    if (loadEXRCount > static_cast<int>(inputPaths.size())) {
        if (_logger) _logger->warn("Workflow has {} LoadEXR nodes but only {} inputs connected. Some nodes have no input.",
                                   loadEXRCount, inputPaths.size());
    }
    if (saveEXRCount == 0) {
        if (_logger) _logger->warn("No SaveEXR nodes found in workflow! Workflow may not produce output.");
    }

    // Log the FINAL workflow
    if (_logger) {
        _logger->debug("=== FINAL WORKFLOW (after path injection) ===");
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

bool AnyComfyPlugin::isUIFormat(const json& workflow)
{
    // UI format has "nodes" array, "links" array, "version", etc.
    // API format has node IDs as top-level keys ("1", "2", "10", "11", etc.)
    return workflow.contains("nodes") && workflow["nodes"].is_array();
}

json AnyComfyPlugin::convertUIFormatToAPI(const json& uiWorkflow)
{
    if (_logger) _logger->info("Converting UI format to API format");

    if (!uiWorkflow.contains("nodes") || !uiWorkflow["nodes"].is_array()) {
        // Already in API format or invalid
        return uiWorkflow;
    }

    json apiWorkflow;
    const auto& nodes = uiWorkflow["nodes"];

    for (const auto& node : nodes) {
        if (!node.contains("id") || !node.contains("type")) {
            continue;  // Skip invalid nodes
        }

        std::string nodeId = std::to_string(node["id"].get<int>());
        std::string nodeType = node["type"];

        // Build API format node
        json apiNode;
        apiNode["class_type"] = nodeType;
        apiNode["inputs"] = json::object();

        // Convert widget_values to inputs based on node type
        if (node.contains("widgets_values") && node["widgets_values"].is_array()) {
            const auto& widgets = node["widgets_values"];

            if (nodeType == "LoadEXR" && widgets.size() >= 5) {
                apiNode["inputs"]["filepath"] = widgets[0];
                apiNode["inputs"]["linear_to_sRGB"] = widgets[1];
                apiNode["inputs"]["image_load_cap"] = widgets[2];
                apiNode["inputs"]["skip_first_images"] = widgets[3];
                apiNode["inputs"]["select_every_nth"] = widgets[4];
            }
            else if (nodeType == "SaveEXR" && widgets.size() >= 5) {
                apiNode["inputs"]["filename_prefix"] = widgets[0];
                apiNode["inputs"]["sRGB_to_linear"] = widgets[1];
                apiNode["inputs"]["version"] = widgets[2];
                apiNode["inputs"]["start_frame"] = widgets[3];
                apiNode["inputs"]["frame_pad"] = widgets[4];
            }
        }

        // Handle node inputs/connections (from links)
        if (node.contains("inputs") && node["inputs"].is_array()) {
            for (const auto& input : node["inputs"]) {
                if (input.contains("link") && !input["link"].is_null()) {
                    // Find the source node from links
                    if (uiWorkflow.contains("links") && uiWorkflow["links"].is_array()) {
                        int linkId = input["link"];
                        for (const auto& link : uiWorkflow["links"]) {
                            if (link.is_array() && link.size() >= 6 && link[0] == linkId) {
                                // Link format: [id, source_node, source_slot, target_node, target_slot, type]
                                apiNode["inputs"][input["name"]] = {
                                    std::to_string(link[1].get<int>()),  // source node ID
                                    link[2].get<int>()                    // source output slot
                                };
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Add _meta field with title (required by ancient ComfyUI versions)
        std::string nodeTitle;
        if (nodeType == "LoadEXR") {
            nodeTitle = "Load EXR";
        } else if (nodeType == "SaveEXR") {
            nodeTitle = "Save EXR";
        } else {
            // Generic title: convert "SomeNodeType" -> "Some Node Type"
            nodeTitle = nodeType;
            for (size_t i = 1; i < nodeTitle.length(); i++) {
                if (std::isupper(nodeTitle[i]) && std::islower(nodeTitle[i-1])) {
                    nodeTitle.insert(i, " ");
                    i++;
                }
            }
        }
        apiNode["_meta"] = {{"title", nodeTitle}};

        apiWorkflow[nodeId] = apiNode;
    }

    if (_logger) _logger->info("Converted {} nodes from UI to API format", apiWorkflow.size());
    return apiWorkflow;
}

json AnyComfyPlugin::convertAPIFormatToUI(const json& apiWorkflow)
{
    if (_logger) _logger->info("Converting API format to UI format");

    // Check if already in UI format
    if (apiWorkflow.contains("nodes") && apiWorkflow["nodes"].is_array()) {
        return apiWorkflow;
    }

    json uiWorkflow;
    uiWorkflow["nodes"] = json::array();
    uiWorkflow["links"] = json::array();
    uiWorkflow["groups"] = json::array();
    uiWorkflow["config"] = json::object();
    uiWorkflow["extra"] = {{"ds", {{"scale", 1.0}, {"offset", {{"0", 0}, {"1", 0}}}}}};
    uiWorkflow["version"] = 0.4;

    int lastNodeId = 0;
    int lastLinkId = 0;
    int nodeY = 300;  // Starting Y position for nodes
    int nodeX = 200;  // Starting X position

    // First pass: create nodes and track connections
    std::map<std::string, int> nodeIdMap;  // API node ID -> UI node ID (as int)
    std::vector<std::tuple<int, std::string, int, int>> pendingLinks;  // target_node, input_name, source_node, source_slot

    for (auto& [apiNodeId, nodeData] : apiWorkflow.items()) {
        // Skip metadata nodes
        if (apiNodeId == "_meta" || apiNodeId.rfind("_", 0) == 0) continue;
        if (!nodeData.is_object() || !nodeData.contains("class_type")) continue;

        int nodeId;
        try {
            nodeId = std::stoi(apiNodeId);
        } catch (...) {
            continue;  // Skip non-numeric node IDs
        }

        if (nodeId > lastNodeId) lastNodeId = nodeId;
        nodeIdMap[apiNodeId] = nodeId;

        std::string nodeType = nodeData["class_type"];

        json uiNode;
        uiNode["id"] = nodeId;
        uiNode["type"] = nodeType;
        uiNode["pos"] = {nodeX, nodeY};
        uiNode["size"] = {{"0", 315}, {"1", 150}};
        uiNode["flags"] = json::object();
        uiNode["order"] = static_cast<int>(uiWorkflow["nodes"].size());
        uiNode["mode"] = 0;
        uiNode["properties"] = {{"Node name for S&R", nodeType}};

        // Build widgets_values from inputs
        json widgetsValues = json::array();
        json inputs = json::array();
        json outputs = json::array();

        if (nodeData.contains("inputs")) {
            for (auto& [inputName, inputValue] : nodeData["inputs"].items()) {
                // Check if input is a connection (array with node ID and slot)
                if (inputValue.is_array() && inputValue.size() >= 2) {
                    // This is a connection - add to inputs array and track for link creation
                    json inputDef;
                    inputDef["name"] = inputName;
                    inputDef["type"] = "*";  // Generic type
                    inputDef["link"] = nullptr;  // Will be filled in later
                    inputs.push_back(inputDef);

                    // Store pending link info
                    std::string sourceNodeId = inputValue[0].get<std::string>();
                    int sourceSlot = inputValue[1].get<int>();
                    pendingLinks.push_back({nodeId, inputName, std::stoi(sourceNodeId), sourceSlot});
                } else {
                    // This is a widget value
                    widgetsValues.push_back(inputValue);
                }
            }
        }

        // Add standard outputs based on node type
        if (nodeType == "LoadEXR") {
            outputs.push_back({{"name", "RGB"}, {"type", "IMAGE"}, {"links", nullptr}, {"shape", 3}});
            outputs.push_back({{"name", "alpha"}, {"type", "MASK"}, {"links", nullptr}, {"shape", 3}});
            outputs.push_back({{"name", "batch_size"}, {"type", "INT"}, {"links", nullptr}, {"shape", 3}});
        } else if (nodeType == "SaveEXR") {
            inputs.insert(inputs.begin(), {{"name", "images"}, {"type", "IMAGE"}, {"link", nullptr}});
        } else {
            // Generic: assume one IMAGE output
            outputs.push_back({{"name", "IMAGE"}, {"type", "IMAGE"}, {"links", nullptr}, {"shape", 3}});
        }

        uiNode["widgets_values"] = widgetsValues;
        if (!inputs.empty()) uiNode["inputs"] = inputs;
        if (!outputs.empty()) uiNode["outputs"] = outputs;

        uiWorkflow["nodes"].push_back(uiNode);

        // Move position for next node
        nodeX += 400;
        if (nodeX > 1600) {
            nodeX = 200;
            nodeY += 300;
        }
    }

    // Second pass: create links
    for (const auto& [targetNode, inputName, sourceNode, sourceSlot] : pendingLinks) {
        lastLinkId++;

        // Find target input slot
        int targetSlot = 0;
        for (auto& node : uiWorkflow["nodes"]) {
            if (node["id"] == targetNode && node.contains("inputs")) {
                for (size_t i = 0; i < node["inputs"].size(); i++) {
                    if (node["inputs"][i]["name"] == inputName) {
                        node["inputs"][i]["link"] = lastLinkId;
                        targetSlot = static_cast<int>(i);
                        break;
                    }
                }
            }
        }

        // Find source output slot and update links array
        for (auto& node : uiWorkflow["nodes"]) {
            if (node["id"] == sourceNode && node.contains("outputs") && sourceSlot < node["outputs"].size()) {
                if (node["outputs"][sourceSlot]["links"].is_null()) {
                    node["outputs"][sourceSlot]["links"] = json::array();
                }
                node["outputs"][sourceSlot]["links"].push_back(lastLinkId);
            }
        }

        // Link format: [link_id, source_node, source_slot, target_node, target_slot, type]
        uiWorkflow["links"].push_back({lastLinkId, sourceNode, sourceSlot, targetNode, targetSlot, "*"});
    }

    uiWorkflow["last_node_id"] = lastNodeId;
    uiWorkflow["last_link_id"] = lastLinkId;

    if (_logger) _logger->info("Converted {} nodes from API to UI format", uiWorkflow["nodes"].size());
    return uiWorkflow;
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
    desc.addSupportedContext(OFX::eContextFilter);    // Standard filter (requires input)
    desc.addSupportedContext(OFX::eContextGeneral);   // General (optional input)
    desc.addSupportedContext(OFX::eContextGenerator); // Generator (no input required)

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
        logger->info("=== AnyComfyPluginFactory::describeInContext() called ===");
    }

    // Load config defaults from bundle
    if (auto logger = spdlog::get("AnyComfy")) {
        logger->info("Loading config defaults from bundle...");
    }
    json configDefaults = BasePlugin::loadConfigDefaults();

    if (auto logger = spdlog::get("AnyComfy")) {
        if (!configDefaults.empty()) {
            logger->info("Config loaded successfully, size: {}", configDefaults.size());
            logger->info("Config keys: server={}, controls={}, project={}",
                        configDefaults.contains("server"),
                        configDefaults.contains("controls"),
                        configDefaults.contains("project"));
        } else {
            logger->warn("Config is empty, will use hardcoded defaults");
        }
    }

    // Source clip (primary input)
    // - Required in Filter context
    // - Optional in General/Generator contexts (for generator workflows with 0 inputs)
    if (context != OFX::eContextGenerator) {
        OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
        srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
        srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
        srcClip->setTemporalClipAccess(false);
        srcClip->setSupportsTiles(false);
        srcClip->setIsMask(false);
        // Make optional in General context to support generator workflows
        if (context == OFX::eContextGeneral) {
            srcClip->setOptional(true);
        }
    }

    // Source2 clip (secondary input - optional, maps to InputB in ComfyUI)
    // Not available in Generator context
    if (context != OFX::eContextGenerator) {
        OFX::ClipDescriptor *src2Clip = desc.defineClip("Source2");
        src2Clip->addSupportedComponent(OFX::ePixelComponentRGBA);
        src2Clip->addSupportedComponent(OFX::ePixelComponentRGB);
        src2Clip->setTemporalClipAccess(false);
        src2Clip->setSupportsTiles(false);
        src2Clip->setIsMask(false);
        src2Clip->setOptional(true);
        src2Clip->setLabel("Source2");
    }

    // Source3 clip (tertiary input - optional, maps to InputC in ComfyUI)
    // Not available in Generator context
    if (context != OFX::eContextGenerator) {
        OFX::ClipDescriptor *src3Clip = desc.defineClip("Source3");
        src3Clip->addSupportedComponent(OFX::ePixelComponentRGBA);
        src3Clip->addSupportedComponent(OFX::ePixelComponentRGB);
        src3Clip->setTemporalClipAccess(false);
        src3Clip->setSupportsTiles(false);
        src3Clip->setIsMask(false);
        src3Clip->setOptional(true);
        src3Clip->setLabel("Source3");
    }

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

    // New Workflow Name (text field for user input)
    {
        OFX::StringParamDescriptor *param = desc.defineStringParam("newWorkflowName");
        param->setLabels("New Workflow Name", "Workflow Name", "New Workflow Name");
        param->setHint(
            "Name for the next workflow to be created (optional).\n\n"
            "If empty, a unique name will be generated automatically.\n"
            "Format: anycomfy_<instance>_<timestamp>.json\n\n"
            "If specified, this name will be used and the field will be cleared.\n"
            "Extension .json will be added automatically if not present.\n\n"
            "Examples:\n"
            "  'my_denoise' → my_denoise.json\n"
            "  'upscale_4x.json' → upscale_4x.json"
        );
        param->setStringType(OFX::eStringTypeSingleLine);
        param->setDefault("");
        param->setAnimates(false);
        param->setParent(*workflowGroup);
    }

    // New Workflow Input Count (choice for selecting template type)
    {
        OFX::ChoiceParamDescriptor *param = desc.defineChoiceParam("newWorkflowInputCount");
        param->setLabels("Inputs", "Inputs", "New Workflow Input Count");
        param->setHint(
            "Number of input images for the new workflow template.\n\n"
            "• 0 inputs: Generator workflow (text-to-image, procedural, etc.)\n"
            "  Template has only SaveEXR output node.\n\n"
            "• 1 input: Standard filter workflow (Recommended)\n"
            "  Template has LoadEXR (InputA) and SaveEXR nodes.\n"
            "  Use Source clip for input.\n\n"
            "• 2 inputs: Compositing workflow\n"
            "  Template has LoadEXR (InputA, InputB) and SaveEXR nodes.\n"
            "  Use Source and Source2 clips for inputs.\n\n"
            "• 3 inputs: Multi-input workflow\n"
            "  Template has LoadEXR (InputA, InputB, InputC) and SaveEXR nodes.\n"
            "  Use Source, Source2, and Source3 clips for inputs.\n\n"
            "LoadEXR nodes are mapped to OFX clips by title or position."
        );
        param->appendOption("0 inputs (Generator)");
        param->appendOption("1 input (Standard)");
        param->appendOption("2 inputs (Compositing)");
        param->appendOption("3 inputs (Multi-input)");
        param->setDefault(1);  // Default to 1 input (standard filter workflow)
        param->setAnimates(false);
        param->setParent(*workflowGroup);
    }

    // Create New Workflow button
    {
        OFX::PushButtonParamDescriptor *param = desc.definePushButtonParam("createNewWorkflow");
        param->setLabels("New Workflow", "New Workflow", "Create New Workflow");
        param->setHint(
            "Create a new template workflow with LoadEXR and SaveEXR nodes.\n"
            "The workflow will be saved to the workflows directory and ComfyUI will open in your browser.\n\n"
            "Workflow Name:\n"
            "- If 'New Workflow Name' field above is filled, that name will be used\n"
            "- Otherwise, a unique name will be generated automatically\n\n"
            "Edit the workflow in ComfyUI by:\n"
            "1. Adding your custom nodes between LoadEXR and SaveEXR\n"
            "2. Maintaining the image processing chain\n"
            "3. Saving the workflow in ComfyUI\n\n"
            "The workflow file path will be automatically updated to use your new workflow."
        );
        param->setParent(*workflowGroup);
    }

    // ========================================================================
    // Use base class to define common parameters (includes workflowFilePath)
    // Pass the config defaults so parameters can use them
    // ========================================================================
    BasePlugin::describeCommonParameters(desc, context, projectPage, processingPage, serverPage, &configDefaults);

    // ========================================================================
    // Move workflowFilePath from Project page to Workflow page
    // ========================================================================
    // The base class defines workflowFilePath on the Project page
    // We redefine it here to move it to the Workflow page and position it between buttons
    {
        OFX::StringParamDescriptor *param = desc.defineStringParam("workflowFilePath");
        param->setLabel("Workflow File Path");
        param->setHint(
            "Path to workflow API file (relative to ComfyUI Input Directory).\n\n"
            "Format: workflows/<workflow_name>/<workflow_name>_api.json\n\n"
            "Examples:\n"
            "  workflows/my_denoise/my_denoise_api.json\n"
            "  workflows/upscale_4x/upscale_4x_api.json\n\n"
            "This path is automatically set when you:\n"
            "• Create a new workflow with 'New Workflow' button\n"
            "• Select an existing workflow from the file browser\n\n"
            "Leave empty if no workflow is selected."
        );
        param->setStringType(OFX::eStringTypeFilePath);
        param->setDefault("");  // Empty by default for AnyComfy
        param->setAnimates(false);
        param->setParent(*workflowGroup);  // Move to Workflow page
    }

    // Open Workflow button
    {
        OFX::PushButtonParamDescriptor *param = desc.definePushButtonParam("openWorkflow");
        param->setLabels("Open Workflow", "Open Workflow", "Open Workflow in ComfyUI");
        param->setHint(
            "Open the currently selected workflow in ComfyUI browser for editing.\n\n"
            "This allows you to modify an existing workflow at any time.\n\n"
            "The plugin will:\n"
            "1. Look for the UI format version of the workflow (editable in ComfyUI)\n"
            "2. If only API format exists, it will be converted to UI format\n"
            "3. Open ComfyUI in your browser with the workflow loaded\n\n"
            "After editing, save the workflow in ComfyUI to update it."
        );
        param->setParent(*workflowGroup);
    }

    // ========================================================================
    // Override workflow name for AnyComfy - hide it from UI
    // ========================================================================
    // The base plugin defines workflowName as a visible parameter
    // For AnyComfy, we hide it because it's automatically derived from the
    // workflow file path (see changedParam handler for "workflowFilePath")
    // NOTE: We do NOT use setEnabled(false) because the plugin needs to SET
    // this value programmatically when building output paths
    {
        OFX::StringParamDescriptor *param = desc.defineStringParam("workflowName");
        param->setDefault("");  // Empty by default, auto-filled from workflow selection
        param->setIsSecret(true);  // Hide from UI - automatically managed by plugin
        param->setAnimates(false);
        param->setEvaluateOnChange(false);
        param->setHint("Auto-derived from workflow file path and used for output path construction (hidden in AnyComfy UI)");
    }

    // ========================================================================
    // Add ComfyUI Input Directory parameter to Server page (after base params)
    // ========================================================================
    {
        OFX::StringParamDescriptor *param = desc.defineStringParam("comfyUIInputDir");
        param->setLabels("ComfyUI Input Directory", "ComfyUI Input", "ComfyUI Input Directory");
        param->setHint(
            "Path to ComfyUI's input directory (for auto-loading workflows).\n\n"
            "IMPORTANT: This should match the directory specified in ComfyUI's\n"
            "--input-directory flag when starting the server.\n\n"
            "Current Setup (Windows server → macOS client):\n"
            "  Server: --input-directory S:\\002_COMFYUI\\in\n"
            "  Client: /Volumes/silo2/002_COMFYUI/in\n\n"
            "Path Mapping Examples:\n"
            "  S:\\002_COMFYUI\\in → /Volumes/silo2/002_COMFYUI/in\n"
            "  Z:\\comfy\\in → /Volumes/storage/comfy/in\n\n"
            "The plugin will copy workflow files here so ComfyUI's /view\n"
            "endpoint can serve them for auto-loading in the browser.\n\n"
            "Requires: OFX.AutoLoader extension in ComfyUI/web/extensions/\n"
            "Leave empty to disable auto-loading (manual load required)."
        );
        param->setStringType(OFX::eStringTypeDirectoryPath);
        // Use config default if available, otherwise fallback to hardcoded default
        std::string comfyUIInputDirDefault = "/Volumes/silo2/002_COMFYUI/in";
        if (configDefaults.contains("server") && configDefaults["server"].contains("comfyUIInputDir")) {
            comfyUIInputDirDefault = configDefaults["server"]["comfyUIInputDir"].get<std::string>();
        }
        param->setDefault(comfyUIInputDirDefault.c_str());
        param->setAnimates(false);
        if (serverPage) {
            serverPage->addChild(*param);
        }
    }

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
