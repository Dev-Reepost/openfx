// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

#include "comfyui_base_plugin.h"

namespace ComfyUI {

BasePlugin::BasePlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _srcClip(nullptr)
    , _dstClip(nullptr)
    , _serverAddress(nullptr)
    , _serverPort(nullptr)
    , _sharedMountPath(nullptr)
    , _projectName(nullptr)
    , _state(Idle)
{
    _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);

    // TODO: Fetch common parameters when they're defined
    // _serverAddress = fetchStringParam("serverAddress");
    // _serverPort = fetchIntParam("serverPort");
    // etc.
}

BasePlugin::~BasePlugin() = default;

void BasePlugin::changedParam(const OFX::InstanceChangedArgs &args,
                              const std::string &paramName)
{
    // TODO: Handle parameter changes
}

void BasePlugin::render(const OFX::RenderArguments &args)
{
    // TODO: Implement workflow execution
    // This will be the main entry point for processing
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
    // TODO: Implement workflow execution logic
}

std::string BasePlugin::writeInputImage(OFX::Image* img, int frame)
{
    // TODO: Implement EXR writing with OpenImageIO
    return "";
}

} // namespace ComfyUI
