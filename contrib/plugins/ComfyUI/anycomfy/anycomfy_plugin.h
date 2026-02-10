// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_ANYCOMFY_PLUGIN_H
#define COMFYUI_ANYCOMFY_PLUGIN_H

#include "comfyui_base_plugin.h"
#include <string>
#include <vector>

namespace ComfyUI {

/**
 * @brief Generic OFX plugin for executing any ComfyUI workflow
 *
 * This plugin allows users to execute any ComfyUI workflow as long as it
 * implements LoadEXR and SaveEXR nodes. Unlike specific plugins (like SAM),
 * this plugin has no workflow-specific parameters - it's completely generic.
 *
 * Features:
 * - Workflow file selector (browses workflows/ directory on shared server)
 * - "New Workflow" button to create template and open ComfyUI in browser
 * - Automatic unique naming using OFX instance name
 * - Zero assumptions about workflow content (beyond EXR I/O)
 *
 * Workflow Requirements:
 * - Must have a LoadEXR node (will receive ${INPUT_PATH})
 * - Must have a SaveEXR node (will receive ${OUTPUT_PREFIX} and ${FRAME})
 * - All other nodes are workflow-specific and configured in ComfyUI UI
 */
class AnyComfyPlugin : public BasePlugin {
public:
    AnyComfyPlugin(OfxImageEffectHandle handle);
    virtual ~AnyComfyPlugin();

    // Implement abstract methods from BasePlugin
    virtual json buildWorkflow(int frame, const std::map<std::string, std::string>& inputPaths) override;
    virtual std::vector<std::string> getRequiredModels() override;

    // AnyComfy includes workflow name in basename since each workflow represents a different effect
    virtual bool includeWorkflowInBasename() const override { return true; }

    // Override basename generation to avoid redundant instance name
    // (workflow name already contains instance identifier)
    virtual std::string getEffectiveBasename() override;

    // OFX lifecycle
    virtual void changedParam(const OFX::InstanceChangedArgs &args,
                             const std::string &paramName) override;

    // Static parameter definition (called by factory)
    static void describeInContext(OFX::ImageEffectDescriptor &desc,
                                  OFX::ContextEnum context);

private:
    // AnyComfy-specific parameters
    OFX::PushButtonParam *_createNewWorkflow;  // Button to create template workflow and open browser
    OFX::PushButtonParam *_editWorkflow;       // Button to edit existing workflow in ComfyUI browser
    OFX::StringParam     *_comfyUIInputDir;    // ComfyUI input directory path (for auto-loading workflows)
    OFX::StringParam     *_newWorkflowName;    // User-specified name for new workflows (optional)
    OFX::ChoiceParam     *_newWorkflowInputCount;  // Number of inputs for new workflow template (0-3)

    // Helper methods
    std::string getWorkflowsPath() const;           // Get base path to workflows directory
    std::string getWorkflowPath(const std::string& workflowName) const;  // Get full path to specific workflow file
    std::vector<std::string> scanWorkflowFiles();   // List available workflows (by subdirectory)
    void createTemplateWorkflow();                  // Create empty workflow with EXR nodes
    void openExistingWorkflow();                    // Open selected workflow in ComfyUI browser for editing
    void openComfyUIInBrowser(const std::string& workflowPath);  // Open browser with workflow loaded
    std::string generateUniqueWorkflowName();       // Generate unique workflow name from instance
    json injectPathsIntoWorkflow(const json& workflow, int frame,
                                  const std::map<std::string, std::string>& inputPaths,
                                  const std::string& outputPrefix);  // Smart path injection with position-based LoadEXR mapping
    std::string deriveWorkflowNameFromFilename(const std::string& filepath);  // Auto-derive workflow name from filename
    json convertUIFormatToAPI(const json& uiWorkflow);  // Convert UI format (nodes array) to API format (node IDs as keys)
    json convertAPIFormatToUI(const json& apiWorkflow); // Convert API format to UI format (for editing in ComfyUI)
    bool isUIFormat(const json& workflow);  // Check if workflow is in UI format vs API format
};

/**
 * @brief Plugin factory for AnyComfy
 */
class AnyComfyPluginFactory : public OFX::PluginFactoryHelper<AnyComfyPluginFactory> {
public:
    AnyComfyPluginFactory();
    virtual void describe(OFX::ImageEffectDescriptor &desc) override;
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc,
                                   OFX::ContextEnum context) override;
    virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle,
                                            OFX::ContextEnum context) override;
};

} // namespace ComfyUI

#endif // COMFYUI_ANYCOMFY_PLUGIN_H
