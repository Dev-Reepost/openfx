// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef COMFYUI_SAM_SEGMENTATION_PLUGIN_H
#define COMFYUI_SAM_SEGMENTATION_PLUGIN_H

#include "comfyui_base_plugin.h"
#include <string>
#include <vector>

namespace ComfyUI {

/**
 * @brief OFX plugin for SAM (Segment Anything Model) segmentation via ComfyUI
 *
 * This plugin uses Grounding DINO + SAM to perform semantic segmentation
 * based on text prompts. It generates both the segmented result image and
 * an alpha matte for compositing.
 *
 * Workflow:
 * 1. Load input image
 * 2. Load Grounding DINO and SAM models
 * 3. Segment based on text prompt
 * 4. Generate mask and preprocessed image
 * 5. Save both outputs as EXR files
 *
 * Based on: https://github.com/Dev-Reepost/flame_comfyui_segmentation
 */
class SAMSegmentationPlugin : public BasePlugin {
public:
    SAMSegmentationPlugin(OfxImageEffectHandle handle);
    virtual ~SAMSegmentationPlugin();

    // Implement abstract methods from BasePlugin
    virtual json buildWorkflow(int frame, const std::map<std::string, std::string>& inputPaths) override;
    virtual std::vector<std::string> getRequiredModels() override;

    // OFX lifecycle
    virtual void changedParam(const OFX::InstanceChangedArgs &args,
                             const std::string &paramName) override;

    // Static parameter definition (called by factory)
    static void describeInContext(OFX::ImageEffectDescriptor &desc,
                                  OFX::ContextEnum context);

private:
    // Segmentation-specific parameters
    OFX::StringParam  *_segmentPrompt;      // Text prompt (e.g., "foreground", "person", "car")
    OFX::DoubleParam  *_threshold;          // Detection threshold (0.0-1.0)
    OFX::ChoiceParam  *_samModel;           // SAM model selection
    OFX::ChoiceParam  *_dinoModel;          // Grounding DINO model selection
    OFX::ChoiceParam  *_resolutionMode;     // Resolution mode (Source, 720p, 1080p, etc.)
    OFX::IntParam     *_customResolution;   // Custom resolution value
    OFX::BooleanParam *_linearToSRGB;       // Color space conversion (input)
    OFX::BooleanParam *_sRGBToLinear;       // Color space conversion (output)

    // Helper methods
    std::string getSAMModelName() const;
    std::string getDinoModelName() const;
    json buildHardcodedWorkflow(int frame, const std::string& inputPath);  // Fallback hardcoded workflow
    json customizeWorkflowWithParams(const json& baseWorkflow, int frame);  // Add SAM-specific parameters
};

/**
 * @brief Plugin factory for SAM Segmentation
 */
class SAMSegmentationPluginFactory : public OFX::PluginFactoryHelper<SAMSegmentationPluginFactory> {
public:
    SAMSegmentationPluginFactory();
    virtual void describe(OFX::ImageEffectDescriptor &desc) override;
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc,
                                   OFX::ContextEnum context) override;
    virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle,
                                            OFX::ContextEnum context) override;
};

} // namespace ComfyUI

#endif // COMFYUI_SAM_SEGMENTATION_PLUGIN_H
