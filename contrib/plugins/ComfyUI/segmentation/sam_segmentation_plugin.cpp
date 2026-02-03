// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "sam_segmentation_plugin.h"
#include <sstream>
#include <iomanip>

namespace ComfyUI {

// ============================================================================
// SAMSegmentationPlugin Implementation
// ============================================================================

SAMSegmentationPlugin::SAMSegmentationPlugin(OfxImageEffectHandle handle)
    : BasePlugin(handle)
    , _segmentPrompt(nullptr)
    , _threshold(nullptr)
    , _samModel(nullptr)
    , _dinoModel(nullptr)
    , _resolutionMode(nullptr)
    , _customResolution(nullptr)
    , _linearToSRGB(nullptr)
    , _sRGBToLinear(nullptr)
{
    // Fetch segmentation-specific parameters
    _segmentPrompt = fetchStringParam("segmentPrompt");
    _threshold = fetchDoubleParam("threshold");
    _samModel = fetchChoiceParam("samModel");
    _dinoModel = fetchChoiceParam("dinoModel");
    _resolutionMode = fetchChoiceParam("resolutionMode");
    _customResolution = fetchIntParam("customResolution");
    _linearToSRGB = fetchBooleanParam("linearToSRGB");
    _sRGBToLinear = fetchBooleanParam("sRGBToLinear");
}

SAMSegmentationPlugin::~SAMSegmentationPlugin()
{
}

void SAMSegmentationPlugin::changedParam(const OFX::InstanceChangedArgs &args,
                                         const std::string &paramName)
{
    // Call base class first (handles server address changes)
    BasePlugin::changedParam(args, paramName);

    // Handle segmentation-specific parameter changes
    if (paramName == "resolutionMode") {
        // Show/hide custom resolution parameter based on selection
        int modeIndex;
        _resolutionMode->getValueAtTime(args.time, modeIndex);
        bool isCustom = (modeIndex == 5);  // "Custom" is index 5
        _customResolution->setIsSecret(!isCustom);
        _customResolution->setEnabled(isCustom);
    } else if (paramName == "samModel") {
        // Could validate model availability on server
    } else if (paramName == "dinoModel") {
        // Could validate model availability on server
    }
}

std::string SAMSegmentationPlugin::getSAMModelName() const
{
    int modelIndex;
    _samModel->getValue(modelIndex);
    switch (modelIndex) {
        case 0: return "sam_vit_h (2.56GB)";
        case 1: return "sam_vit_l (1.25GB)";
        case 2: return "sam_vit_b (375MB)";
        default: return "sam_vit_h (2.56GB)";
    }
}

std::string SAMSegmentationPlugin::getDinoModelName() const
{
    int modelIndex;
    _dinoModel->getValue(modelIndex);
    switch (modelIndex) {
        case 0: return "GroundingDINO_SwinT_OGC (694MB)";
        case 1: return "GroundingDINO_SwinB (938MB)";
        default: return "GroundingDINO_SwinT_OGC (694MB)";
    }
}

json SAMSegmentationPlugin::buildWorkflow(int frame, const std::map<std::string, std::string>& inputPaths)
{
    if (_logger) _logger->info("Building SAM segmentation workflow for frame {}", frame);

    // SAM uses single input - extract InputA from the map
    std::string inputPath;
    auto it = inputPaths.find("InputA");
    if (it != inputPaths.end()) {
        inputPath = it->second;
    } else if (!inputPaths.empty()) {
        // Fallback to first available input
        inputPath = inputPaths.begin()->second;
    } else {
        throw std::runtime_error("No input path provided for SAM workflow");
    }

    if (_logger) _logger->info("Using input path: {}", inputPath);

    // Try to load workflow from file first
    std::string workflowPath;
    _workflowFilePath->getValue(workflowPath);

    if (!workflowPath.empty()) {
        if (_logger) _logger->info("Attempting to load workflow from file: {}", workflowPath);

        try {
            // Resolve the path (handles bundle resources and absolute paths)
            std::string resolvedPath = resolveWorkflowPath(workflowPath);

            if (!resolvedPath.empty()) {
                // Load workflow from file
                json baseWorkflow = loadWorkflowFromFile(resolvedPath);

                // Customize with parameters and paths
                json customized = customizeWorkflow(baseWorkflow, frame, inputPaths);

                // Add SAM-specific parameter customization
                json final = customizeWorkflowWithParams(customized, frame);

                if (_logger) _logger->info("Successfully loaded and customized workflow from file");
                return final;
            } else {
                if (_logger) _logger->warn("Could not resolve workflow path, falling back to hardcoded workflow");
            }
        } catch (const std::exception& e) {
            if (_logger) _logger->error("Failed to load workflow from file: {}. Falling back to hardcoded workflow.", e.what());
        }
    } else {
        if (_logger) _logger->info("No workflow file specified, using hardcoded workflow");
    }

    // Fallback: use hardcoded workflow
    return buildHardcodedWorkflow(frame, inputPath);
}

json SAMSegmentationPlugin::buildHardcodedWorkflow(int frame, const std::string& inputPath)
{
    if (_logger) _logger->info("Building hardcoded SAM segmentation workflow");

    // Get parameter values
    std::string prompt;
    _segmentPrompt->getValue(prompt);

    double threshold = _threshold->getValue();

    // Calculate resolution based on mode
    int resolutionModeIndex;
    _resolutionMode->getValueAtTime(frame, resolutionModeIndex);
    int resolution;

    if (resolutionModeIndex == 0) {
        // "Source" - use source clip dimensions (shorter side, as SAMPreprocessor scales by short side)
        OfxRectD rod = _srcClip->getRegionOfDefinition(frame);
        int width = static_cast<int>(rod.x2 - rod.x1);
        int height = static_cast<int>(rod.y2 - rod.y1);
        resolution = std::min(width, height);  // Use SHORTER side - SAMPreprocessor maintains aspect ratio from short side
        if (_logger) _logger->info("Resolution mode: SOURCE - calculated {}x{} → resolution={} (short side)", width, height, resolution);
    } else if (resolutionModeIndex == 1) {
        resolution = 720;   // 720p
        if (_logger) _logger->info("Resolution mode: 720p → resolution={}", resolution);
    } else if (resolutionModeIndex == 2) {
        resolution = 1080;  // 1080p
        if (_logger) _logger->info("Resolution mode: 1080p → resolution={}", resolution);
    } else if (resolutionModeIndex == 3) {
        resolution = 1536;  // 2K
        if (_logger) _logger->info("Resolution mode: 2K → resolution={}", resolution);
    } else if (resolutionModeIndex == 4) {
        resolution = 2160;  // 4K
        if (_logger) _logger->info("Resolution mode: 4K → resolution={}", resolution);
    } else {  // Custom
        _customResolution->getValueAtTime(frame, resolution);
        if (_logger) _logger->info("Resolution mode: CUSTOM → resolution={}", resolution);
    }

    // Always use sRGB conversion (parameters are hidden)
    bool linearToSRGB = true;
    bool sRGBToLinear = true;
    // _linearToSRGB->getValue();  // No longer reading from parameter
    // _sRGBToLinear->getValue();  // No longer reading from parameter

    std::string samModelName = getSAMModelName();
    std::string dinoModelName = getDinoModelName();

    if (_logger) {
        _logger->info("SAM Parameters:");
        _logger->info("  Prompt: {}", prompt);
        _logger->info("  Threshold: {}", threshold);
        _logger->info("  Resolution: {}", resolution);
        _logger->info("  SAM Model: {}", samModelName);
        _logger->info("  DINO Model: {}", dinoModelName);
        _logger->info("  Linear to sRGB: {}", linearToSRGB);
        _logger->info("  sRGB to Linear: {}", sRGBToLinear);
    }

    // Get shared storage path components for output
    std::string mountPath, project, workflow_name, version;
    _macMountPath->getValue(mountPath);
    _projectName->getValue(project);
    _workflowName->getValue(workflow_name);
    _outputVersion->getValue(version);

    // Get effective basename (auto-generated or manual)
    std::string basename = getEffectiveBasename();

    // Build output path prefix
    std::ostringstream outputPrefix;
    outputPrefix << mountPath << "/out/" << project << "/" << workflow_name
                 << "/" << version << "/" << basename;

    if (_logger) {
        _logger->info("Local paths:");
        _logger->info("  Input: {}", inputPath);
        _logger->info("  Output prefix: {}", outputPrefix.str());
    }

    // Convert paths for ComfyUI (Windows server)
    std::string comfyInputPath = convertPathForComfyUI(inputPath);
    std::string comfyOutputPrefix = convertPathForComfyUI(outputPrefix.str());

    if (_logger) {
        _logger->info("ComfyUI paths (Windows):");
        _logger->info("  Input: {}", comfyInputPath);
        _logger->info("  Output prefix: {}", comfyOutputPrefix);
    }

    // Build ComfyUI workflow based on reference:
    // https://github.com/Dev-Reepost/flame_comfyui_segmentation
    // NOTE: Return ONLY the nodes object - queuePrompt() will wrap it in {"prompt": ...}
    json workflow = {
        // Node 1: Load input EXR
        {"1", {
            {"inputs", {
                {"filepath", comfyInputPath},
                {"linear_to_sRGB", linearToSRGB ? "true" : "false"},
                {"image_load_cap", 0},
                {"skip_first_images", 0},
                {"select_every_nth", 1}
            }},
            {"class_type", "LoadEXR"}
        }},

        // Node 16: Grounding DINO + SAM segmentation
        {"16", {
            {"inputs", {
                {"prompt", prompt},
                {"threshold", threshold},
                {"sam_model", json::array({"18", 0})},
                {"grounding_dino_model", json::array({"17", 0})},
                {"image", json::array({"1", 0})}
            }},
            {"class_type", "GroundingDinoSAMSegment (segment anything)"}
        }},

        // Node 17: Load Grounding DINO model
        {"17", {
            {"inputs", {
                {"model_name", dinoModelName}
            }},
            {"class_type", "GroundingDinoModelLoader (segment anything)"}
        }},

        // Node 18: Load SAM model
        {"18", {
            {"inputs", {
                {"model_name", samModelName}
            }},
            {"class_type", "SAMModelLoader (segment anything)"}
        }},

        // Node 24: SAM preprocessor - resizes image to target resolution
        // This resized image is used by Node 16 for segmentation
        // The resolution parameter controls output size for performance/quality tradeoff
        {"24", {
            {"inputs", {
                {"resolution", resolution},
                {"image", json::array({"1", 0})}
            }},
            {"class_type", "SAMPreprocessor"}
        }},

        // Node 25: Join RGB image with alpha mask to create RGBA
        // Both image and alpha are at the same resolution (from Node 24 and Node 16)
        {"25", {
            {"inputs", {
                {"image", json::array({"24", 0})},  // RGB from SAMPreprocessor (resized to resolution)
                {"alpha", json::array({"16", 1})}   // MASK from segmentation (same resolution)
            }},
            {"class_type", "JoinImageWithAlpha"}
        }},

        // Node 27: Save RGBA image as EXR (RGB + embedded alpha matte)
        {"27", {
            {"inputs", {
                {"filename_prefix", comfyOutputPrefix},
                {"sRGB_to_linear", sRGBToLinear ? "true" : "false"},
                {"version", -1},  // -1 = no version suffix in filename (directory version is enough)
                {"start_frame", frame},
                {"frame_pad", 4},
                {"images", json::array({"25", 0})}  // RGBA from JoinImageWithAlpha
            }},
            {"class_type", "SaveEXR"}
        }}
    };

    // Log the complete workflow being sent to ComfyUI for debugging
    if (_logger) {
        _logger->info("=== HARDCODED WORKFLOW DUMP ===");
        _logger->info("Complete workflow JSON: {}", workflow.dump(2));
        _logger->info("Node 24 (SAMPreprocessor): {}", workflow["24"].dump());
        _logger->info("Node 16 (GroundingDinoSAMSegment): {}", workflow["16"].dump());
        _logger->info("Node 27 (SaveEXR): {}", workflow["27"].dump());
        _logger->info("=== END WORKFLOW DUMP ===");
    }

    return workflow;
}

json SAMSegmentationPlugin::customizeWorkflowWithParams(const json& baseWorkflow, int frame)
{
    // Add SAM-specific parameter customization to workflow loaded from file
    // This allows template workflows to be customized with UI parameters

    if (_logger) _logger->debug("Customizing workflow with SAM parameters");

    json workflow = baseWorkflow;  // Make a copy

    // Get SAM-specific parameters
    std::string prompt;
    _segmentPrompt->getValue(prompt);
    double threshold = _threshold->getValue();

    // Calculate resolution
    int resolutionModeIndex;
    _resolutionMode->getValueAtTime(frame, resolutionModeIndex);
    int resolution;

    if (resolutionModeIndex == 0) {
        // "Source" - use source clip dimensions (shorter side, as SAMPreprocessor scales by short side)
        OfxRectD rod = _srcClip->getRegionOfDefinition(frame);
        int width = static_cast<int>(rod.x2 - rod.x1);
        int height = static_cast<int>(rod.y2 - rod.y1);
        resolution = std::min(width, height);  // Use SHORTER side - SAMPreprocessor maintains aspect ratio from short side
        if (_logger) _logger->info("Resolution mode: SOURCE - calculated {}x{} → resolution={} (short side)", width, height, resolution);
    } else if (resolutionModeIndex == 1) {
        resolution = 720;
        if (_logger) _logger->info("Resolution mode: 720p → resolution={}", resolution);
    } else if (resolutionModeIndex == 2) {
        resolution = 1080;
        if (_logger) _logger->info("Resolution mode: 1080p → resolution={}", resolution);
    } else if (resolutionModeIndex == 3) {
        resolution = 1536;
        if (_logger) _logger->info("Resolution mode: 1536p → resolution={}", resolution);
    } else if (resolutionModeIndex == 4) {
        resolution = 2160;
        if (_logger) _logger->info("Resolution mode: 2160p (4K) → resolution={}", resolution);
    } else {
        _customResolution->getValueAtTime(frame, resolution);
        if (_logger) _logger->info("Resolution mode: CUSTOM → resolution={}", resolution);
    }

    // Always use sRGB conversion (parameters are hidden)
    bool linearToSRGB = true;
    bool sRGBToLinear = true;
    // _linearToSRGB->getValue();  // No longer reading from parameter
    // _sRGBToLinear->getValue();  // No longer reading from parameter
    
    std::string samModelName = getSAMModelName();
    std::string dinoModelName = getDinoModelName();

    // Convert workflow to string for placeholder replacement
    std::string workflowStr = workflow.dump();

    // Replace SAM-specific placeholders
    // ${PROMPT} - segmentation prompt
    // ${THRESHOLD} - detection threshold
    // ${RESOLUTION} - preprocessing resolution
    // ${SAM_MODEL} - SAM model name
    // ${DINO_MODEL} - Grounding DINO model name
    // ${LINEAR_TO_SRGB} - "true" or "false"
    // ${SRGB_TO_LINEAR} - "true" or "false"

    size_t pos = 0;
    std::string placeholder;

    // Replace ${PROMPT}
    placeholder = "${PROMPT}";
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), prompt);
    }

    // Replace "${THRESHOLD}" with number (no quotes)
    // Search for the placeholder INCLUDING surrounding quotes to convert string to number
    placeholder = "\"${THRESHOLD}\"";
    std::string thresholdStr = std::to_string(threshold);
    if (_logger) _logger->info("Replacing \"${{THRESHOLD}}\" with numeric: {}", thresholdStr);
    int replaceCount = 0;
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), thresholdStr);
        replaceCount++;
        if (_logger) _logger->debug("  Replaced THRESHOLD at position {}", pos);
    }
    if (_logger) {
        if (replaceCount == 0) {
            _logger->warn("THRESHOLD placeholder not found in workflow! Search pattern: {}", placeholder);
        } else {
            _logger->info("  Replaced THRESHOLD {} time(s)", replaceCount);
        }
    }

    // Replace "${RESOLUTION}" with number (no quotes)
    placeholder = "\"${RESOLUTION}\"";
    std::string resolutionStr = std::to_string(resolution);
    if (_logger) _logger->info("Replacing \"${{RESOLUTION}}\" with numeric: {}", resolutionStr);
    replaceCount = 0;
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), resolutionStr);
        replaceCount++;
        if (_logger) _logger->debug("  Replaced RESOLUTION at position {}", pos);
    }
    if (_logger) {
        if (replaceCount == 0) {
            _logger->warn("RESOLUTION placeholder not found in workflow! Search pattern: {}", placeholder);
        } else {
            _logger->info("  Replaced RESOLUTION {} time(s)", replaceCount);
        }
    }

    // Replace ${SAM_MODEL}
    placeholder = "${SAM_MODEL}";
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), samModelName);
    }

    // Replace ${DINO_MODEL}
    placeholder = "${DINO_MODEL}";
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), dinoModelName);
    }

    // Replace "${LINEAR_TO_SRGB}" with boolean (no quotes)
    // Search for the placeholder INCLUDING surrounding quotes to convert string to boolean
    placeholder = "\"${LINEAR_TO_SRGB}\"";
    std::string linearToSRGBStr = linearToSRGB ? "true" : "false";
    if (_logger) _logger->info("Replacing \"${{LINEAR_TO_SRGB}}\" with boolean: {}", linearToSRGBStr);
    replaceCount = 0;
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), linearToSRGBStr);
        replaceCount++;
        if (_logger) _logger->debug("  Replaced LINEAR_TO_SRGB at position {}", pos);
    }
    if (_logger) {
        if (replaceCount == 0) {
            _logger->warn("LINEAR_TO_SRGB placeholder not found in workflow! Search pattern: {}", placeholder);
        } else {
            _logger->info("  Replaced LINEAR_TO_SRGB {} time(s)", replaceCount);
        }
    }

    // Replace "${SRGB_TO_LINEAR}" with boolean (no quotes)
    // Search for the placeholder INCLUDING surrounding quotes to convert string to boolean
    placeholder = "\"${SRGB_TO_LINEAR}\"";
    std::string sRGBToLinearStr = sRGBToLinear ? "true" : "false";
    if (_logger) _logger->info("Replacing \"${{SRGB_TO_LINEAR}}\" with boolean: {}", sRGBToLinearStr);
    replaceCount = 0;
    while ((pos = workflowStr.find(placeholder)) != std::string::npos) {
        workflowStr.replace(pos, placeholder.length(), sRGBToLinearStr);
        replaceCount++;
        if (_logger) _logger->debug("  Replaced SRGB_TO_LINEAR at position {}", pos);
    }
    if (_logger) {
        if (replaceCount == 0) {
            _logger->warn("SRGB_TO_LINEAR placeholder not found in workflow! Search pattern: {}", placeholder);
        } else {
            _logger->info("  Replaced SRGB_TO_LINEAR {} time(s)", replaceCount);
        }
    }

    if (_logger) {
        _logger->debug("SAM parameter customization complete");
        _logger->debug("  Prompt: {}", prompt);
        _logger->debug("  Threshold: {}", threshold);
        _logger->debug("  Resolution: {}", resolution);
        _logger->debug("  SAM Model: {}", samModelName);
        _logger->debug("  DINO Model: {}", dinoModelName);
    }

    // Parse back to JSON
    try {
        json finalWorkflow = json::parse(workflowStr);

        // Log critical nodes to verify all replacements worked
        if (_logger) {
            if (finalWorkflow.contains("16")) {
                _logger->info("Node 16 (Segmentation) in final workflow: {}", finalWorkflow["16"].dump());
            }
            if (finalWorkflow.contains("24")) {
                _logger->info("Node 24 (SAMPreprocessor) in final workflow: {}", finalWorkflow["24"].dump());
            }
            if (finalWorkflow.contains("27")) {
                _logger->info("Node 27 (SaveEXR) in final workflow: {}", finalWorkflow["27"].dump());
            }
            
            // Log the COMPLETE final workflow that will be sent to ComfyUI
            _logger->info("=== FINAL WORKFLOW TO BE SENT TO COMFYUI ===");
            _logger->info("Complete workflow JSON:\n{}", finalWorkflow.dump(2));
            _logger->info("=== END FINAL WORKFLOW ===");
        }

        return finalWorkflow;
    } catch (const json::exception& e) {
        if (_logger) _logger->error("Failed to parse customized workflow: {}", e.what());
        throw std::runtime_error("Failed to parse SAM customized workflow: " + std::string(e.what()));
    }
}

std::vector<std::string> SAMSegmentationPlugin::getRequiredModels()
{
    return {
        getSAMModelName(),
        getDinoModelName()
    };
}

void SAMSegmentationPlugin::describeInContext(OFX::ImageEffectDescriptor &desc,
                                               OFX::ContextEnum context)
{
    // Single page for Flame - organize with collapsible groups
    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");

    // ==== SEGMENTATION GROUP ====
    OFX::GroupParamDescriptor *samGroup = desc.defineGroupParam("samGroup");
    samGroup->setLabel("Segmentation");
    samGroup->setOpen(true);
    page->addChild(*samGroup);

    // Segment prompt
    OFX::StringParamDescriptor *prompt = desc.defineStringParam("segmentPrompt");
    prompt->setLabel("Prompt");
    prompt->setHint("Text description of what to segment (e.g., 'foreground', 'person', 'car', 'building')");
    prompt->setDefault("foreground");
    prompt->setAnimates(false);
    prompt->setParent(*samGroup);
    page->addChild(*prompt);

    // Threshold
    OFX::DoubleParamDescriptor *threshold = desc.defineDoubleParam("threshold");
    threshold->setLabel("Threshold");
    threshold->setHint("Detection confidence threshold (0.0-1.0). Higher values = stricter detection.");
    threshold->setDefault(0.3);
    threshold->setRange(0.0, 1.0);
    threshold->setDisplayRange(0.0, 1.0);
    threshold->setParent(*samGroup);
    page->addChild(*threshold);

    // SAM Model selection
    OFX::ChoiceParamDescriptor *samModel = desc.defineChoiceParam("samModel");
    samModel->setLabel("SAM Model");
    samModel->setHint("Segment Anything Model version. Larger models = better quality but slower.");
    samModel->appendOption("SAM ViT-H (2.56GB) - Highest quality");
    samModel->appendOption("SAM ViT-L (1.25GB) - Balanced");
    samModel->appendOption("SAM ViT-B (375MB) - Fastest");
    samModel->setDefault(0); // ViT-H
    samModel->setAnimates(false);
    samModel->setParent(*samGroup);
    page->addChild(*samModel);

    // Grounding DINO Model selection
    OFX::ChoiceParamDescriptor *dinoModel = desc.defineChoiceParam("dinoModel");
    dinoModel->setLabel("Grounding DINO Model");
    dinoModel->setHint("Grounding DINO model for text-based detection. Larger models = better accuracy.");
    dinoModel->appendOption("SwinT (694MB) - Faster");
    dinoModel->appendOption("SwinB (938MB) - More accurate");
    dinoModel->setDefault(0); // SwinT
    dinoModel->setAnimates(false);
    dinoModel->setParent(*samGroup);
    page->addChild(*dinoModel);

    // Resolution Mode
    OFX::ChoiceParamDescriptor *resolutionMode = desc.defineChoiceParam("resolutionMode");
    resolutionMode->setLabel("Resolution");
    resolutionMode->setHint("Preprocessing resolution (longer side). Higher = better quality but slower.");
    resolutionMode->appendOption("Source", "Use source clip resolution");
    resolutionMode->appendOption("720p", "1280x720 (HD)");
    resolutionMode->appendOption("1080p", "1920x1080 (Full HD)");
    resolutionMode->appendOption("2K", "2048x1536");
    resolutionMode->appendOption("4K", "3840x2160 (Ultra HD)");
    resolutionMode->appendOption("Custom", "Custom resolution value");
    resolutionMode->setDefault(0);  // Default to "Source"
    resolutionMode->setParent(*samGroup);
    page->addChild(*resolutionMode);

    // Custom Resolution (only visible when Custom is selected)
    OFX::IntParamDescriptor *customResolution = desc.defineIntParam("customResolution");
    customResolution->setLabel("Custom Resolution");
    customResolution->setHint("Custom preprocessing resolution value (longer side)");
    customResolution->setDefault(1080);
    customResolution->setRange(512, 4096);
    customResolution->setDisplayRange(720, 2160);
    customResolution->setIsSecret(true);  // Hidden by default
    customResolution->setEnabled(false);   // Disabled by default
    customResolution->setParent(*samGroup);
    page->addChild(*customResolution);

    // Linear to sRGB (input) - hidden, always enabled
    OFX::BooleanParamDescriptor *linearToSRGB = desc.defineBooleanParam("linearToSRGB");
    linearToSRGB->setLabel("Input: Linear to sRGB");
    linearToSRGB->setHint("Convert input from linear to sRGB color space");
    linearToSRGB->setDefault(true);
    linearToSRGB->setIsSecret(true);  // Hidden from UI
    linearToSRGB->setParent(*samGroup);
    // page->addChild(*linearToSRGB);  // Removed from UI

    // sRGB to Linear (output) - hidden, always enabled
    OFX::BooleanParamDescriptor *sRGBToLinear = desc.defineBooleanParam("sRGBToLinear");
    sRGBToLinear->setLabel("Output: sRGB to Linear");
    sRGBToLinear->setHint("Convert output from sRGB to linear color space");
    sRGBToLinear->setDefault(true);
    sRGBToLinear->setIsSecret(true);  // Hidden from UI
    sRGBToLinear->setParent(*samGroup);
    // page->addChild(*sRGBToLinear);  // Removed from UI

    // Add common parameters (all to same page with groups)
    BasePlugin::describeCommonParameters(desc, context, page, page, page);

    // Override workflowName default for this specialized plugin
    // SAMSegmentation uses "segmentation" as the effect subdirectory
    OFX::StringParamDescriptor *workflow = desc.defineStringParam("workflowName");
    workflow->setDefault("segmentation");
}

// ============================================================================
// SAMSegmentationPluginFactory Implementation
// ============================================================================

SAMSegmentationPluginFactory::SAMSegmentationPluginFactory()
    : OFX::PluginFactoryHelper<SAMSegmentationPluginFactory>(
        "com.comfyui.SAMSegmentation",  // Plugin identifier
        1,                               // Version major
        0                                // Version minor
    )
{
}

void SAMSegmentationPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // Basic plugin properties
    desc.setLabel("ComfyUI SAM Segmentation");
    desc.setLabels("ComfyUI SAM", "SAM", "ComfyUI SAM Segmentation");
    desc.setPluginGrouping("ComfyUI");

    desc.setPluginDescription(
        "Semantic segmentation using Grounding DINO and Segment Anything Model (SAM) via ComfyUI.\n\n"
        "Uses text prompts to automatically segment objects in images. "
        "Generates both segmented result and alpha matte.\n\n"
        "Requires:\n"
        "- ComfyUI server with Segment Anything extension\n"
        "- Grounding DINO and SAM models installed\n"
        "- Shared network storage for image exchange\n\n"
        "Based on: https://github.com/Dev-Reepost/flame_comfyui_segmentation"
    );

    // Supported contexts
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedContext(OFX::eContextGeneral);

    // Supported pixel depths
    desc.addSupportedBitDepth(OFX::eBitDepthUByte);
    desc.addSupportedBitDepth(OFX::eBitDepthUShort);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);

    // Plugin capabilities
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    desc.setSupportsTiles(false); // Full-frame processing required
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
    desc.setSupportsMultipleClipDepths(true);
    desc.setRenderThreadSafety(OFX::eRenderInstanceSafe);
}

void SAMSegmentationPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                                      OFX::ContextEnum context)
{
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

    // Define parameters
    SAMSegmentationPlugin::describeInContext(desc, context);
}

OFX::ImageEffect* SAMSegmentationPluginFactory::createInstance(OfxImageEffectHandle handle,
                                                                OFX::ContextEnum /*context*/)
{
    return new SAMSegmentationPlugin(handle);
}

} // namespace ComfyUI

// ============================================================================
// Plugin Registration
// ============================================================================

namespace OFX {
namespace Plugin {

void getPluginIDs(OFX::PluginFactoryArray &ids)
{
    static ComfyUI::SAMSegmentationPluginFactory factory;
    ids.push_back(&factory);
}

} // namespace Plugin
} // namespace OFX
