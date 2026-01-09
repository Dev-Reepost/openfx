# OFX Multiple Inputs: Comprehensive Implementation Guide

**Date:** 2025-12-16
**Status:** Technical Reference
**Audience:** OFX Plugin Developers

---

## Table of Contents

1. [Overview](#overview)
2. [Understanding OFX Contexts](#understanding-ofx-contexts)
3. [When to Use Multiple Inputs](#when-to-use-multiple-inputs)
4. [Implementation Approaches](#implementation-approaches)
5. [Step-by-Step Implementation](#step-by-step-implementation)
6. [Host Compatibility](#host-compatibility)
7. [Best Practices](#best-practices)
8. [Advanced Patterns](#advanced-patterns)
9. [Troubleshooting](#troubleshooting)
10. [Real-World Examples](#real-world-examples)

---

## Overview

### What Are Multiple Inputs?

In OFX, **multiple inputs** allow a plugin to receive and process several image sources simultaneously. This is essential for:

- **Compositing operations** (blend foreground over background)
- **Masking effects** (use matte to control effect intensity)
- **Multi-source effects** (combine multiple layers)
- **Advanced image processing** (texture mapping, displacement, etc.)

### Key Capabilities

| Feature | Single Input (Filter) | Multiple Inputs (General) |
|---------|----------------------|---------------------------|
| **Input Clips** | 1 mandated ("Source") | Unlimited custom-named |
| **Output Clips** | 1 mandated ("Output") | 1 mandated ("Output") |
| **Input Names** | Fixed: "Source" | Custom: Any valid name |
| **Optional Inputs** | Not applicable | Yes, via `kOfxImageClipPropOptional` |
| **Typical Hosts** | Timeline editors (Resolve, Premiere) | Node-graph compositors (Nuke, Flame, Fusion) |

### Important Limitation

**OFX DOES NOT support multiple outputs.** All plugins, regardless of context, have exactly one output clip named "Output". See the "Multiple Outputs Workaround" section below for solutions.

---

## Understanding OFX Contexts

### Context Comparison Matrix

| Context | Mandated Inputs | Mandated Output | Optional Inputs | Use Case |
|---------|----------------|-----------------|-----------------|----------|
| **Filter** | 1: "Source" | 1: "Output" | No | Single-source effects (blur, color correct) |
| **General** | 0 | 1: "Output" | Yes, unlimited | Multi-source compositing, complex effects |
| **Transition** | 2: "SourceFrom", "SourceTo" | 1: "Output" | No | Dissolves between two clips |
| **Generator** | 0 (optional: "Source") | 1: "Output" | No | Create images from scratch (noise, gradients) |
| **Retimer** | 1: "Source" | 1: "Output" | No | Time-based effects (slow motion, reverse) |

### The General Context in Detail

The **General Context** is the key to multiple inputs:

```cpp
#define kOfxImageEffectContextGeneral "OfxImageEffectContextGeneral"
```

**Characteristics:**
- **Only mandated clip:** "Output"
- **No constraints** on number or names of input clips
- **Full flexibility** in pixel preferences
- **Designed for** node-graph compositing workflows
- **Preferred by** Nuke, Flame, Fusion, Natron

**From OFX specification:**
> "The general context is to some extent a catch all context, but is generally how a 'tree' effect should be instantiated. It has no constraints on its input clips, nor on the pixel preferences actions."

---

## When to Use Multiple Inputs

### Use Cases Requiring Multiple Inputs

#### 1. Compositing Operations

**Over/Under compositing:**
- **Foreground** input: Top layer (often with alpha)
- **Background** input: Bottom layer
- **Matte** input: Optional mask to control blending

**Example:** Keyer plugin that composites keyed foreground over background with edge matte.

#### 2. Masking and Control

**Effect with spatial control:**
- **Source** input: Image to process
- **Mask** input: Grayscale mask controls effect intensity
- **InvMask** input: Optional inverted mask

**Example:** Selective blur where mask defines blur regions.

#### 3. Multi-Source Effects

**Texture/displacement mapping:**
- **Source** input: Base image
- **DisplacementMap** input: Controls pixel displacement
- **TextureMap** input: Provides texture data

**Example:** Lens distortion effect using displacement maps.

#### 4. Advanced Color Grading

**Look development:**
- **Image** input: Source footage
- **Reference** input: Color reference image
- **LUT** input: Optional LUT as image data

**Example:** Match color from reference image.

#### 5. Multi-Layer Effects

**Particle/element compositing:**
- **Beauty** input: Main render pass
- **Reflection** input: Reflection pass
- **Shadow** input: Shadow pass
- **Matte** input: Holdout matte

**Example:** Multi-pass render compositor.

### When NOT to Use Multiple Inputs

❌ **Simple pixel operations** (invert, brightness, contrast)
❌ **Single-source filters** (blur, sharpen, denoise)
❌ **Generators** (unless truly needed)
❌ **Time-based effects** without spatial compositing

**Use Filter context for these instead** - it's simpler and more widely supported.

---

## Implementation Approaches

### Approach 1: Dual-Context Strategy (Recommended)

**Support BOTH Filter and General contexts** for maximum host compatibility:

```cpp
// In describe()
gPropertySuite->propSetString(effectProps,
                              kOfxImageEffectPropSupportedContexts,
                              0,
                              kOfxImageEffectContextFilter);
gPropertySuite->propSetString(effectProps,
                              kOfxImageEffectPropSupportedContexts,
                              1,
                              kOfxImageEffectContextGeneral);
```

**Benefits:**
- ✅ Works in timeline-based hosts (Resolve, Premiere) via Filter context
- ✅ Works in node-graph hosts (Nuke, Flame) via General context
- ✅ Maximum market reach
- ✅ Graceful degradation (single input in Filter, multiple in General)

**Trade-offs:**
- ⚠️ Must implement effect logic for both modes
- ⚠️ More complex code paths
- ⚠️ Testing in both contexts required

### Approach 2: General Context Only

**Support ONLY General context** for specialized workflows:

```cpp
// In describe()
gPropertySuite->propSetString(effectProps,
                              kOfxImageEffectPropSupportedContexts,
                              0,
                              kOfxImageEffectContextGeneral);
```

**When to use:**
- Plugin absolutely requires multiple inputs
- Target audience uses node-graph compositors exclusively
- Effect doesn't make sense with single input

**Trade-offs:**
- ❌ Won't appear in Filter-only hosts
- ❌ Smaller potential user base
- ✅ Simpler implementation (one code path)
- ✅ Can assume multiple inputs always available

### Approach 3: Filter Context with Optional Inputs

**Use Filter context but add optional clips:**

⚠️ **NOT RECOMMENDED** - OFX spec states optional inputs are constrained in Filter context.

From the specification:
> "In all contexts, except for the general context, mandated input clips cannot have their component types remapped, nor can the output. Optional input clips can always have their component types remapped."

While technically possible, this creates confusion and compatibility issues.

---

## Step-by-Step Implementation

### Phase 1: Plugin Descriptor Setup

#### Using Raw C API

```cpp
// describe() - Declare supported contexts
static OfxStatus describe(OfxImageEffectHandle descriptor) {
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(descriptor, &effectProps);

    // Plugin identification
    gPropertySuite->propSetString(effectProps, kOfxPropLabel, 0,
                                  "Multi-Input Compositor");
    gPropertySuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0,
                                  "Compositing");

    // Supported contexts (Filter + General)
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedContexts,
                                  0, kOfxImageEffectContextFilter);
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedContexts,
                                  1, kOfxImageEffectContextGeneral);

    // Pixel depths
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  0, kOfxBitDepthFloat);
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  1, kOfxBitDepthShort);
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  2, kOfxBitDepthByte);

    // Thread safety
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginRenderThreadSafety,
                                  0, kOfxImageEffectRenderFullySafe);

    // Host frame threading
    gPropertySuite->propSetInt(effectProps,
                               kOfxImageEffectPluginPropHostFrameThreading,
                               0, 1);

    return kOfxStatOK;
}
```

#### Using Support Library (C++)

```cpp
class MultiInputPlugin : public OFX::ImageEffect {
public:
    MultiInputPlugin(OfxImageEffectHandle handle) : ImageEffect(handle) {
        // Constructor - clips will be fetched automatically
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);

        // Additional clips for General context
        if (getContext() == OFX::eContextGeneral) {
            bgClip_ = fetchClip("Background");
            matteClip_ = fetchClip("Matte");
        }
    }

private:
    OFX::Clip *srcClip_;
    OFX::Clip *bgClip_;
    OFX::Clip *matteClip_;
    OFX::Clip *dstClip_;
};

class MultiInputPluginFactory : public OFX::PluginFactoryHelper<MultiInputPluginFactory> {
public:
    MultiInputPluginFactory() : OFX::PluginFactoryHelper<MultiInputPluginFactory>(
        "com.example.MultiInputPlugin", 1, 0) {}

    virtual void describe(OFX::ImageEffectDescriptor &desc) {
        desc.setLabel("Multi-Input Compositor");
        desc.setPluginGrouping("Compositing");

        // Contexts
        desc.addSupportedContext(OFX::eContextFilter);
        desc.addSupportedContext(OFX::eContextGeneral);

        // Pixel depths
        desc.addSupportedBitDepth(OFX::eBitDepthFloat);
        desc.addSupportedBitDepth(OFX::eBitDepthUShort);
        desc.addSupportedBitDepth(OFX::eBitDepthUByte);

        // Threading
        desc.setRenderThreadSafety(OFX::eRenderFullySafe);
        desc.setHostFrameThreading(true);
    }

    virtual void describeInContext(OFX::ImageEffectDescriptor &desc,
                                   OFX::ContextEnum context);
};
```

### Phase 2: Clip Definitions

#### Context-Aware Clip Definition (C API)

```cpp
static OfxStatus describeInContext(OfxImageEffectHandle descriptor,
                                   OfxPropertySetHandle inArgs) {
    // Get the context we're being described for
    char *context = nullptr;
    gPropertySuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);

    OfxPropertySetHandle clipProps;

    // ===== OUTPUT CLIP (mandatory in ALL contexts) =====
    gImageEffectSuite->clipDefine(descriptor, kOfxImageEffectOutputClipName, &clipProps);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                  0, kOfxImageComponentRGBA);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                  1, kOfxImageComponentRGB);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                  2, kOfxImageComponentAlpha);

    // ===== SOURCE CLIP (mandatory in Filter, optional in General) =====
    gImageEffectSuite->clipDefine(descriptor, kOfxImageEffectSimpleSourceClipName, &clipProps);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                  0, kOfxImageComponentRGBA);
    gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                  1, kOfxImageComponentRGB);
    gPropertySuite->propSetInt(clipProps, kOfxImageClipPropIsMask, 0, 0);

    // Make Source optional in General context (though usually kept mandatory)
    if (strcmp(context, kOfxImageEffectContextGeneral) == 0) {
        gPropertySuite->propSetInt(clipProps, kOfxImageClipPropOptional, 0, 0); // Still mandatory
    }

    // ===== ADDITIONAL CLIPS (General context only) =====
    if (strcmp(context, kOfxImageEffectContextGeneral) == 0) {

        // --- Background Clip ---
        gImageEffectSuite->clipDefine(descriptor, "Background", &clipProps);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                      0, kOfxImageComponentRGBA);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                      1, kOfxImageComponentRGB);
        gPropertySuite->propSetInt(clipProps, kOfxImageClipPropOptional, 0, 1); // Optional
        gPropertySuite->propSetInt(clipProps, kOfxImageClipPropIsMask, 0, 0);

        // --- Matte Clip ---
        gImageEffectSuite->clipDefine(descriptor, "Matte", &clipProps);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                      0, kOfxImageComponentAlpha);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                      1, kOfxImageComponentRGBA); // Allow RGBA, use alpha channel
        gPropertySuite->propSetInt(clipProps, kOfxImageClipPropOptional, 0, 1); // Optional
        gPropertySuite->propSetInt(clipProps, kOfxImageClipPropIsMask, 0, 1); // This is a mask

        // --- Displacement Map Clip ---
        gImageEffectSuite->clipDefine(descriptor, "DisplacementMap", &clipProps);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                      0, kOfxImageComponentRGBA);
        gPropertySuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents,
                                      1, kOfxImageComponentRGB);
        gPropertySuite->propSetInt(clipProps, kOfxImageClipPropOptional, 0, 1); // Optional
        gPropertySuite->propSetInt(clipProps, kOfxImageClipPropIsMask, 0, 0);

        // Add as many additional clips as needed...
    }

    // Define parameters (not shown here)
    // ...

    return kOfxStatOK;
}
```

#### Context-Aware Clip Definition (Support Library)

```cpp
void MultiInputPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc,
                                                OFX::ContextEnum context) {
    // ===== OUTPUT CLIP =====
    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGB);
    dstClip->addSupportedComponent(OFX::ePixelComponentAlpha);
    dstClip->setSupportsTiles(true);

    // ===== SOURCE CLIP =====
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGB);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // Make optional in General (if desired)
    if (context == OFX::eContextGeneral) {
        srcClip->setOptional(false); // Keep mandatory
    }

    // ===== ADDITIONAL CLIPS (General context only) =====
    if (context == OFX::eContextGeneral) {

        // Background clip
        OFX::ClipDescriptor *bgClip = desc.defineClip("Background");
        bgClip->addSupportedComponent(OFX::ePixelComponentRGBA);
        bgClip->addSupportedComponent(OFX::ePixelComponentRGB);
        bgClip->setTemporalClipAccess(false);
        bgClip->setSupportsTiles(true);
        bgClip->setOptional(true); // Optional
        bgClip->setIsMask(false);

        // Matte clip
        OFX::ClipDescriptor *matteClip = desc.defineClip("Matte");
        matteClip->addSupportedComponent(OFX::ePixelComponentAlpha);
        matteClip->addSupportedComponent(OFX::ePixelComponentRGBA);
        matteClip->setTemporalClipAccess(false);
        matteClip->setSupportsTiles(true);
        matteClip->setOptional(true); // Optional
        matteClip->setIsMask(true); // This is a mask

        // Displacement map clip
        OFX::ClipDescriptor *dispClip = desc.defineClip("DisplacementMap");
        dispClip->addSupportedComponent(OFX::ePixelComponentRGBA);
        dispClip->addSupportedComponent(OFX::ePixelComponentRGB);
        dispClip->setTemporalClipAccess(false);
        dispClip->setSupportsTiles(true);
        dispClip->setOptional(true);
        dispClip->setIsMask(false);
    }

    // Define parameters
    // ...
}
```

### Phase 3: Instance Creation

#### Caching Clip Handles (C API)

```cpp
struct MyInstanceData {
    bool isGeneralContext;

    // Standard clips (all contexts)
    OfxImageClipHandle sourceClip;
    OfxImageClipHandle outputClip;

    // Additional clips (General context only)
    OfxImageClipHandle backgroundClip;
    OfxImageClipHandle matteClip;
    OfxImageClipHandle displacementClip;

    // Parameters
    OfxParamHandle mixParam;
    OfxParamHandle useMatteParam;

    MyInstanceData()
        : isGeneralContext(false)
        , sourceClip(nullptr)
        , outputClip(nullptr)
        , backgroundClip(nullptr)
        , matteClip(nullptr)
        , displacementClip(nullptr)
        , mixParam(nullptr)
        , useMatteParam(nullptr)
    {}
};

static OfxStatus createInstance(OfxImageEffectHandle instance) {
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(instance, &effectProps);

    // Allocate instance data
    MyInstanceData *myData = new MyInstanceData;

    // Check context
    char *context = nullptr;
    gPropertySuite->propGetString(effectProps, kOfxImageEffectPropContext, 0, &context);
    myData->isGeneralContext = (context && strcmp(context, kOfxImageEffectContextGeneral) == 0);

    // Get standard clip handles
    gImageEffectSuite->clipGetHandle(instance, kOfxImageEffectSimpleSourceClipName,
                                     &myData->sourceClip, nullptr);
    gImageEffectSuite->clipGetHandle(instance, kOfxImageEffectOutputClipName,
                                     &myData->outputClip, nullptr);

    // Get additional clip handles (General context only)
    if (myData->isGeneralContext) {
        gImageEffectSuite->clipGetHandle(instance, "Background",
                                         &myData->backgroundClip, nullptr);
        gImageEffectSuite->clipGetHandle(instance, "Matte",
                                         &myData->matteClip, nullptr);
        gImageEffectSuite->clipGetHandle(instance, "DisplacementMap",
                                         &myData->displacementClip, nullptr);
    }

    // Get parameter handles
    OfxParamSetHandle paramSet;
    gImageEffectSuite->getParamSet(instance, &paramSet);
    gParameterSuite->paramGetHandle(paramSet, "mix", &myData->mixParam, nullptr);

    if (myData->isGeneralContext) {
        gParameterSuite->paramGetHandle(paramSet, "useMatte", &myData->useMatteParam, nullptr);
    }

    // Store instance data
    gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void*)myData);

    return kOfxStatOK;
}
```

#### Caching Clip Handles (Support Library)

```cpp
class MultiInputPlugin : public OFX::ImageEffect {
public:
    MultiInputPlugin(OfxImageEffectHandle handle)
        : ImageEffect(handle)
        , srcClip_(nullptr)
        , bgClip_(nullptr)
        , matteClip_(nullptr)
        , dispClip_(nullptr)
        , dstClip_(nullptr)
        , mixParam_(nullptr)
        , useMatteParam_(nullptr)
    {
        // Fetch standard clips
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);

        // Fetch additional clips if in General context
        if (getContext() == OFX::eContextGeneral) {
            bgClip_ = fetchClip("Background");
            matteClip_ = fetchClip("Matte");
            dispClip_ = fetchClip("DisplacementMap");

            useMatteParam_ = fetchBooleanParam("useMatte");
        }

        // Fetch parameters
        mixParam_ = fetchDoubleParam("mix");
    }

    virtual void render(const OFX::RenderArguments &args) override;
    virtual void changedParam(const OFX::InstanceChangedArgs &args,
                             const std::string &paramName) override;

private:
    // Clips
    OFX::Clip *srcClip_;
    OFX::Clip *bgClip_;
    OFX::Clip *matteClip_;
    OFX::Clip *dispClip_;
    OFX::Clip *dstClip_;

    // Parameters
    OFX::DoubleParam *mixParam_;
    OFX::BooleanParam *useMatteParam_;
};
```

### Phase 4: Render Implementation

#### Fetching Multiple Input Images (C API)

```cpp
static OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs) {
    // Get instance data
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(instance, &effectProps);
    MyInstanceData *myData = nullptr;
    gPropertySuite->propGetPointer(effectProps, kOfxPropInstanceData, 0, (void**)&myData);

    // Get render time
    double time;
    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    // Get render window
    OfxRectI renderWindow;
    gPropertySuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    // ===== FETCH SOURCE IMAGE (mandatory) =====
    OfxPropertySetHandle sourceImg = nullptr;
    OfxStatus stat = gImageEffectSuite->clipGetImage(myData->sourceClip, time,
                                                      nullptr, &sourceImg);
    if (stat != kOfxStatOK || !sourceImg) {
        return kOfxStatFailed;
    }

    // ===== FETCH BACKGROUND IMAGE (optional, General context only) =====
    OfxPropertySetHandle bgImg = nullptr;
    if (myData->isGeneralContext && myData->backgroundClip) {
        // Try to fetch - may return NULL if not connected
        gImageEffectSuite->clipGetImage(myData->backgroundClip, time, nullptr, &bgImg);
        // bgImg may be NULL - that's OK for optional clips
    }

    // ===== FETCH MATTE IMAGE (optional, General context only) =====
    OfxPropertySetHandle matteImg = nullptr;
    bool useMatte = false;
    if (myData->isGeneralContext && myData->matteClip) {
        gParameterSuite->paramGetValueAtTime(myData->useMatteParam, time, &useMatte);
        if (useMatte) {
            gImageEffectSuite->clipGetImage(myData->matteClip, time, nullptr, &matteImg);
        }
    }

    // ===== FETCH OUTPUT IMAGE =====
    OfxPropertySetHandle outputImg = nullptr;
    stat = gImageEffectSuite->clipGetImage(myData->outputClip, time, nullptr, &outputImg);
    if (stat != kOfxStatOK || !outputImg) {
        if (sourceImg) gImageEffectSuite->clipReleaseImage(sourceImg);
        if (bgImg) gImageEffectSuite->clipReleaseImage(bgImg);
        if (matteImg) gImageEffectSuite->clipReleaseImage(matteImg);
        return kOfxStatFailed;
    }

    // ===== PROCESS PIXELS =====
    // Get image properties and pixel pointers
    void *srcPtr, *bgPtr, *mattePtr, *dstPtr;
    int srcRowBytes, bgRowBytes, matteRowBytes, dstRowBytes;
    OfxRectI srcBounds, bgBounds, matteBounds, dstBounds;

    gPropertySuite->propGetPointer(sourceImg, kOfxImagePropData, 0, &srcPtr);
    gPropertySuite->propGetInt(sourceImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    gPropertySuite->propGetIntN(sourceImg, kOfxImagePropBounds, 4, &srcBounds.x1);

    if (bgImg) {
        gPropertySuite->propGetPointer(bgImg, kOfxImagePropData, 0, &bgPtr);
        gPropertySuite->propGetInt(bgImg, kOfxImagePropRowBytes, 0, &bgRowBytes);
        gPropertySuite->propGetIntN(bgImg, kOfxImagePropBounds, 4, &bgBounds.x1);
    }

    if (matteImg) {
        gPropertySuite->propGetPointer(matteImg, kOfxImagePropData, 0, &mattePtr);
        gPropertySuite->propGetInt(matteImg, kOfxImagePropRowBytes, 0, &matteRowBytes);
        gPropertySuite->propGetIntN(matteImg, kOfxImagePropBounds, 4, &matteBounds.x1);
    }

    gPropertySuite->propGetPointer(outputImg, kOfxImagePropData, 0, &dstPtr);
    gPropertySuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropertySuite->propGetIntN(outputImg, kOfxImagePropBounds, 4, &dstBounds.x1);

    // Process based on available inputs
    if (bgImg && matteImg) {
        // Full compositing: Source over Background with Matte
        processComposite(srcPtr, bgPtr, mattePtr, dstPtr,
                        srcRowBytes, bgRowBytes, matteRowBytes, dstRowBytes,
                        renderWindow, srcBounds, bgBounds, matteBounds, dstBounds);
    }
    else if (bgImg) {
        // Compositing without matte
        processCompositeNoMatte(srcPtr, bgPtr, dstPtr,
                               srcRowBytes, bgRowBytes, dstRowBytes,
                               renderWindow, srcBounds, bgBounds, dstBounds);
    }
    else if (matteImg) {
        // Source with matte (no background)
        processMasked(srcPtr, mattePtr, dstPtr,
                     srcRowBytes, matteRowBytes, dstRowBytes,
                     renderWindow, srcBounds, matteBounds, dstBounds);
    }
    else {
        // Simple pass-through or effect on source only
        processSimple(srcPtr, dstPtr, srcRowBytes, dstRowBytes,
                     renderWindow, srcBounds, dstBounds);
    }

    // ===== RELEASE IMAGES =====
    gImageEffectSuite->clipReleaseImage(sourceImg);
    if (bgImg) gImageEffectSuite->clipReleaseImage(bgImg);
    if (matteImg) gImageEffectSuite->clipReleaseImage(matteImg);
    gImageEffectSuite->clipReleaseImage(outputImg);

    return kOfxStatOK;
}
```

#### Fetching Multiple Input Images (Support Library)

```cpp
void MultiInputPlugin::render(const OFX::RenderArguments &args) {
    // ===== FETCH SOURCE IMAGE (mandatory) =====
    std::auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
    if (!src.get()) {
        throwSuiteStatusException(kOfxStatFailed);
    }

    // Validate source
    if (src->getRenderScale().x != args.renderScale.x ||
        src->getRenderScale().y != args.renderScale.y) {
        throwSuiteStatusException(kOfxStatFailed);
    }

    // ===== FETCH BACKGROUND IMAGE (optional) =====
    std::auto_ptr<OFX::Image> bg;
    if (getContext() == OFX::eContextGeneral && bgClip_ && bgClip_->isConnected()) {
        bg.reset(bgClip_->fetchImage(args.time));
        // bg may be NULL even if connected (fetching can fail)
    }

    // ===== FETCH MATTE IMAGE (optional) =====
    std::auto_ptr<OFX::Image> matte;
    bool useMatte = false;
    if (getContext() == OFX::eContextGeneral && matteClip_) {
        useMatteParam_->getValueAtTime(args.time, useMatte);
        if (useMatte && matteClip_->isConnected()) {
            matte.reset(matteClip_->fetchImage(args.time));
        }
    }

    // ===== FETCH DISPLACEMENT MAP (optional) =====
    std::auto_ptr<OFX::Image> disp;
    if (getContext() == OFX::eContextGeneral && dispClip_ && dispClip_->isConnected()) {
        disp.reset(dispClip_->fetchImage(args.time));
    }

    // ===== FETCH OUTPUT IMAGE =====
    std::auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
    if (!dst.get()) {
        throwSuiteStatusException(kOfxStatFailed);
    }

    // Validate output
    OFX::BitDepthEnum dstBitDepth = dst->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = dst->getPixelComponents();

    // ===== GET PARAMETERS =====
    double mix = 1.0;
    mixParam_->getValueAtTime(args.time, mix);

    // ===== PROCESS PIXELS =====
    OfxRectI renderWindow = args.renderWindow;

    // Choose processing path based on available inputs
    if (bg.get() && matte.get()) {
        // Full compositing with all inputs
        processFullComposite(src.get(), bg.get(), matte.get(), dst.get(),
                           renderWindow, mix);
    }
    else if (bg.get()) {
        // Compositing source over background
        processOverComposite(src.get(), bg.get(), dst.get(),
                           renderWindow, mix);
    }
    else if (matte.get()) {
        // Source with matte applied
        processMattedSource(src.get(), matte.get(), dst.get(),
                           renderWindow, mix);
    }
    else {
        // Simple source processing
        processSourceOnly(src.get(), dst.get(), renderWindow, mix);
    }
}
```

### Phase 5: Processing Multiple Inputs

#### Example: Compositing with Matte

```cpp
template <class PIX, int nComponents, int MAX>
void processFullComposite(const PIX *srcPixels, const PIX *bgPixels,
                         const PIX *mattePixels, PIX *dstPixels,
                         int srcRowBytes, int bgRowBytes,
                         int matteRowBytes, int dstRowBytes,
                         OfxRectI renderWindow, OfxRectI srcBounds,
                         OfxRectI bgBounds, OfxRectI matteBounds,
                         OfxRectI dstBounds, double mix) {

    for (int y = renderWindow.y1; y < renderWindow.y2; y++) {
        // Check for abort every 20 lines
        if (y % 20 == 0 && gImageEffectSuite->abort(instance)) {
            break;
        }

        // Calculate row pointers
        const PIX *srcRow = (const PIX *)((char*)srcPixels +
                                          (y - srcBounds.y1) * srcRowBytes);
        const PIX *bgRow = (const PIX *)((char*)bgPixels +
                                         (y - bgBounds.y1) * bgRowBytes);
        const PIX *matteRow = (const PIX *)((char*)mattePixels +
                                            (y - matteBounds.y1) * matteRowBytes);
        PIX *dstRow = (PIX *)((char*)dstPixels +
                              (y - dstBounds.y1) * dstRowBytes);

        for (int x = renderWindow.x1; x < renderWindow.x2; x++) {
            const PIX *srcPix = srcRow + (x - srcBounds.x1) * nComponents;
            const PIX *bgPix = bgRow + (x - bgBounds.x1) * nComponents;
            const PIX *mattePix = matteRow + (x - matteBounds.x1); // 1 component
            PIX *dstPix = dstRow + (x - dstBounds.x1) * nComponents;

            // Get matte value (0.0 = background, 1.0 = foreground)
            float matteValue = normalizePixel<PIX, MAX>(*mattePix);

            // Composite: dst = bg * (1 - matte) + src * matte
            for (int c = 0; c < nComponents; c++) {
                float srcVal = normalizePixel<PIX, MAX>(srcPix[c]);
                float bgVal = normalizePixel<PIX, MAX>(bgPix[c]);

                float composited = bgVal * (1.0f - matteValue) + srcVal * matteValue;

                // Apply mix
                float final = bgVal * (1.0f - mix) + composited * mix;

                dstPix[c] = denormalizePixel<PIX, MAX>(final);
            }
        }
    }
}

template <class PIX, int MAX>
inline float normalizePixel(PIX value) {
    if (MAX == 1) {
        return value; // Float
    } else {
        return (float)value / (float)MAX;
    }
}

template <class PIX, int MAX>
inline PIX denormalizePixel(float value) {
    if (MAX == 1) {
        return value; // Float
    } else {
        return (PIX)(value * MAX + 0.5f);
    }
}
```

---

## Host Compatibility

### Host Context Preferences

Different host applications prefer different contexts:

| Host Application | Preferred Context | Secondary Context | Notes |
|-----------------|-------------------|-------------------|-------|
| **Nuke** | General | Filter | Node-graph compositor; General is native |
| **Flame** | General | Filter | Action node tree; expects General |
| **Fusion** | General | Filter | Node-based; General preferred |
| **Natron** | General | Filter | Nuke alternative; General native |
| **DaVinci Resolve** | Filter | General | Timeline-based; Filter is native |
| **Adobe Premiere** | Filter | General | Timeline-based; Filter preferred |
| **Adobe After Effects** | Filter | - | Layer-based; Filter only |
| **Final Cut Pro** | Filter | - | Timeline-based; Filter only |
| **Baselight** | General | Filter | Node-graph; General preferred |
| **Smoke** | General | Filter | Autodesk suite; General preferred |

### Context Discovery

Hosts query supported contexts during plugin discovery:

```cpp
// Host queries this property:
kOfxImageEffectPropSupportedContexts
```

**Your plugin should list contexts in order of preference:**
```cpp
// Prefer General, fall back to Filter
gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts,
                              0, kOfxImageEffectContextGeneral);
gPropertySuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts,
                              1, kOfxImageEffectContextFilter);
```

### Testing Across Hosts

**Recommended test matrix:**

1. **Primary Hosts** (must work):
   - Nuke (General context)
   - Resolve (Filter context)
   - One of: Flame/Baselight/Fusion (General context)

2. **Secondary Hosts** (should work):
   - Natron (free, good for testing)
   - After Effects (if Filter context supported)

3. **Test Cases**:
   - [ ] Plugin appears in effects list
   - [ ] All input connectors visible (General)
   - [ ] Optional inputs work when disconnected
   - [ ] Mandatory inputs fail gracefully when missing
   - [ ] Parameters visible and functional
   - [ ] Render produces correct output
   - [ ] Multi-threaded rendering works
   - [ ] Undo/redo works
   - [ ] Saving/loading project preserves settings

---

## Best Practices

### 1. Clip Naming Conventions

**Use industry-standard names when possible:**

| Purpose | Recommended Name | Alternative Names |
|---------|------------------|-------------------|
| Primary input | `Source` | `Input`, `Fg`, `Foreground` |
| Background layer | `Background` | `Bg`, `Back`, `Under` |
| Alpha/matte | `Matte` | `Mask`, `Alpha`, `Key` |
| Displacement | `DisplacementMap` | `Disp`, `DistortMap` |
| Texture | `Texture` | `Tex`, `TextureMap` |
| Reference | `Reference` | `Ref`, `Guide` |
| Detail layer | `Detail` | `Overlay`, `HighFreq` |

**Naming rules:**
- ✅ Use PascalCase or camelCase
- ✅ Be descriptive but concise
- ✅ Avoid special characters
- ✅ Consistent across your plugin suite
- ❌ Don't use spaces in clip names
- ❌ Don't use host-specific terminology

### 2. Optional vs. Mandatory Inputs

**Make inputs optional when:**
- Effect can reasonably work without them
- Provides fallback behavior (e.g., black background)
- User might want minimal setup

**Make inputs mandatory when:**
- Effect is meaningless without them
- No reasonable default exists
- Better to fail than produce garbage output

**Example decision tree:**

```
Keyer plugin:
  - Source: MANDATORY (nothing to key without it)
  - Background: OPTIONAL (can output transparent)
  - Matte: OPTIONAL (can use generated matte)

Multi-view stereo plugin:
  - LeftEye: MANDATORY
  - RightEye: MANDATORY
  - DepthMap: OPTIONAL (can generate from stereo pair)
```

### 3. Checking Optional Clip Connection

**Always check if optional clips are connected before using:**

#### C API
```cpp
// Check if clip is connected
OfxPropertySetHandle clipProps;
gImageEffectSuite->clipGetPropertySet(myData->backgroundClip, &clipProps);
int isConnected = 0;
gPropertySuite->propGetInt(clipProps, kOfxImageClipPropConnected, 0, &isConnected);

if (isConnected) {
    OfxPropertySetHandle bgImg = nullptr;
    OfxStatus stat = gImageEffectSuite->clipGetImage(myData->backgroundClip,
                                                      time, nullptr, &bgImg);
    if (stat == kOfxStatOK && bgImg) {
        // Use background image
    }
}
```

#### Support Library
```cpp
if (bgClip_ && bgClip_->isConnected()) {
    std::auto_ptr<OFX::Image> bg(bgClip_->fetchImage(args.time));
    if (bg.get()) {
        // Use background image
    }
}
```

### 4. Parameter Visibility

**Show/hide parameters based on context:**

```cpp
// In describeInContext()
if (context == OFX::eContextGeneral) {
    // Parameter only relevant when Background clip available
    OFX::BooleanParamDescriptor *useMatteParam =
        desc.defineBooleanParam("useMatte");
    useMatteParam->setLabel("Use Matte");
    useMatteParam->setHint("Apply matte to control compositing");
    useMatteParam->setDefault(false);

    // Could also be controlled via secret flag based on clip connection
}
```

**Dynamic visibility based on clip connection:**

```cpp
void MultiInputPlugin::changedClip(const OFX::InstanceChangedArgs &args,
                                   const std::string &clipName) {
    if (clipName == "Matte") {
        // Show/hide matte-related parameters
        bool matteConnected = matteClip_->isConnected();
        useMatteParam_->setIsSecretAndDisabled(!matteConnected);
        matteInvertParam_->setIsSecretAndDisabled(!matteConnected);
    }
}
```

### 5. Render Region of Interest (RoI)

**When using multiple inputs, correctly handle RoI:**

```cpp
// getRegionsOfInterest action
OfxStatus getRegionsOfInterest(OfxImageEffectHandle instance,
                               OfxPropertySetHandle inArgs,
                               OfxPropertySetHandle outArgs) {
    double time;
    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    OfxRectD roi;
    gPropertySuite->propGetDoubleN(inArgs, kOfxImageEffectPropRegionOfInterest,
                                   4, &roi.x1);

    // Source needs same region
    gPropertySuite->propSetDoubleN(outArgs, "OfxImageClipPropRoI_Source",
                                   4, &roi.x1);

    // Background needs same region
    gPropertySuite->propSetDoubleN(outArgs, "OfxImageClipPropRoI_Background",
                                   4, &roi.x1);

    // Matte might need larger region if using for blur/feather
    OfxRectD matteRoi = roi;
    expandRectangle(&matteRoi, featherRadius);
    gPropertySuite->propSetDoubleN(outArgs, "OfxImageClipPropRoI_Matte",
                                   4, &matteRoi.x1);

    return kOfxStatOK;
}
```

### 6. Component Compatibility

**Handle different component types across inputs:**

```cpp
void MultiInputPlugin::render(const OFX::RenderArguments &args) {
    auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
    auto_ptr<OFX::Image> bg(bgClip_->fetchImage(args.time));

    // Source might be RGBA, background might be RGB
    OFX::PixelComponentEnum srcComps = src->getPixelComponents();
    OFX::PixelComponentEnum bgComps = bg->getPixelComponents();

    if (srcComps == OFX::ePixelComponentRGBA &&
        bgComps == OFX::ePixelComponentRGB) {
        // Composite RGBA source over RGB background
        // Result should be RGBA (preserve source alpha)
        processRGBAoverRGB(src.get(), bg.get(), dst.get(), args.renderWindow);
    }
    else if (srcComps == bgComps) {
        // Same components - straightforward
        processMatching(src.get(), bg.get(), dst.get(), args.renderWindow);
    }
    // ... handle other cases
}
```

### 7. Error Handling for Missing Inputs

**Provide clear error messages:**

```cpp
void MultiInputPlugin::render(const OFX::RenderArguments &args) {
    // Check mandatory inputs
    if (!srcClip_->isConnected()) {
        setPersistentMessage(OFX::Message::eMessageError, "",
                           "Source input must be connected");
        throwSuiteStatusException(kOfxStatFailed);
    }

    // For General context, check if we need specific inputs
    if (getContext() == OFX::eContextGeneral) {
        bool needsBackground = /* check your logic */;
        if (needsBackground && !bgClip_->isConnected()) {
            setPersistentMessage(OFX::Message::eMessageError, "",
                               "Background input required for this operation");
            throwSuiteStatusException(kOfxStatFailed);
        }
    }

    // Clear any previous errors
    clearPersistentMessage();

    // Proceed with render...
}
```

---

## Advanced Patterns

### Pattern 1: Dynamic Input Count

**Problem:** Plugin needs variable number of inputs (e.g., multi-layer compositor with 2-8 inputs)

**Solution:** Define maximum inputs, make extras optional

```cpp
void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) {
    if (context == OFX::eContextGeneral) {
        // Define 8 layer inputs
        for (int i = 0; i < 8; i++) {
            std::stringstream ss;
            ss << "Layer" << (i + 1);

            OFX::ClipDescriptor *layerClip = desc.defineClip(ss.str());
            layerClip->addSupportedComponent(OFX::ePixelComponentRGBA);
            layerClip->setSupportsTiles(true);

            // First layer mandatory, rest optional
            layerClip->setOptional(i > 0);
        }

        // Add integer param to control active layer count
        OFX::IntParamDescriptor *layerCount = desc.defineIntParam("layerCount");
        layerCount->setLabel("Number of Layers");
        layerCount->setRange(1, 8);
        layerCount->setDisplayRange(1, 8);
        layerCount->setDefault(2);
    }
}
```

### Pattern 2: Clip Role Parameters

**Problem:** Want user to choose which input serves which purpose

**Solution:** Use choice parameters to assign roles

```cpp
// Define generic inputs
OFX::ClipDescriptor *input1 = desc.defineClip("Input1");
OFX::ClipDescriptor *input2 = desc.defineClip("Input2");

// Add choice parameter for input roles
OFX::ChoiceParamDescriptor *input1Role = desc.defineChoiceParam("input1Role");
input1Role->setLabel("Input 1 Role");
input1Role->appendOption("Foreground");
input1Role->appendOption("Background");
input1Role->appendOption("Matte");
input1Role->setDefault(0); // Foreground

OFX::ChoiceParamDescriptor *input2Role = desc.defineChoiceParam("input2Role");
input2Role->setLabel("Input 2 Role");
input2Role->appendOption("Foreground");
input2Role->appendOption("Background");
input2Role->appendOption("Matte");
input2Role->setDefault(1); // Background

// In render, use role to determine processing
int input1RoleVal, input2RoleVal;
input1Role_->getValueAtTime(args.time, input1RoleVal);
input2Role_->getValueAtTime(args.time, input2RoleVal);
```

### Pattern 3: Temporal Multi-Input (Time Offset)

**Problem:** Need inputs from different time points (temporal effects)

**Solution:** Use temporal clip access

```cpp
void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) {
    OFX::ClipDescriptor *srcClip = desc.defineClip("Source");
    srcClip->setTemporalClipAccess(true); // Allow accessing different times

    // Add time offset parameter
    OFX::DoubleParamDescriptor *timeOffset = desc.defineDoubleParam("timeOffset");
    timeOffset->setLabel("Time Offset");
    timeOffset->setRange(-100, 100);
    timeOffset->setDefault(0);
}

// In render
void render(const OFX::RenderArguments &args) {
    double timeOffset;
    timeOffsetParam_->getValueAtTime(args.time, timeOffset);

    // Fetch current frame
    auto_ptr<OFX::Image> current(srcClip_->fetchImage(args.time));

    // Fetch offset frame
    auto_ptr<OFX::Image> offset(srcClip_->fetchImage(args.time + timeOffset));

    // Process both frames (motion blur, echo, etc.)
    processTemporalEffect(current.get(), offset.get(), dst.get());
}
```

### Pattern 4: Hierarchical Input Groups

**Problem:** Many inputs become overwhelming in UI

**Solution:** Use clip naming conventions and parameter pages

```cpp
void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) {
    // Group 1: Main inputs
    desc.defineClip("Main_Foreground");
    desc.defineClip("Main_Background");
    desc.defineClip("Main_Matte");

    // Group 2: Detail inputs
    desc.defineClip("Detail_HighFreq");
    desc.defineClip("Detail_LowFreq");

    // Group 3: Control inputs
    desc.defineClip("Control_DisplacementMap");
    desc.defineClip("Control_ColorGrading");

    // Create parameter pages for each group
    OFX::PageParamDescriptor *mainPage = desc.definePageParam("Main");
    OFX::PageParamDescriptor *detailPage = desc.definePageParam("Detail");
    OFX::PageParamDescriptor *controlPage = desc.definePageParam("Control");
}
```

### Pattern 5: Fallback Rendering Chain

**Problem:** Want graceful degradation when optional inputs missing

**Solution:** Chain of fallback render paths

```cpp
void render(const OFX::RenderArguments &args) {
    auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
    auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));

    // Try most complex path first
    if (bgClip_->isConnected() && matteClip_->isConnected() &&
        dispClip_->isConnected()) {
        auto_ptr<OFX::Image> bg(bgClip_->fetchImage(args.time));
        auto_ptr<OFX::Image> matte(matteClip_->fetchImage(args.time));
        auto_ptr<OFX::Image> disp(dispClip_->fetchImage(args.time));
        renderFullComplex(src, bg, matte, disp, dst, args);
    }
    // Fall back to no displacement
    else if (bgClip_->isConnected() && matteClip_->isConnected()) {
        auto_ptr<OFX::Image> bg(bgClip_->fetchImage(args.time));
        auto_ptr<OFX::Image> matte(matteClip_->fetchImage(args.time));
        renderComposite(src, bg, matte, dst, args);
    }
    // Fall back to simple over
    else if (bgClip_->isConnected()) {
        auto_ptr<OFX::Image> bg(bgClip_->fetchImage(args.time));
        renderSimpleOver(src, bg, dst, args);
    }
    // Simplest: source only
    else {
        renderSourceOnly(src, dst, args);
    }
}
```

---

## Troubleshooting

### Common Issues and Solutions

#### Issue 1: Plugin Not Appearing in Host

**Symptoms:**
- Plugin builds successfully
- Not visible in host effects list
- No error messages

**Causes & Solutions:**

1. **Host doesn't support your context**
   ```cpp
   // Solution: Add Filter context as fallback
   desc.addSupportedContext(OFX::eContextFilter);
   desc.addSupportedContext(OFX::eContextGeneral);
   ```

2. **Plugin installed in wrong location**
   ```bash
   # macOS: Should be in one of these:
   ~/Library/OFX/Plugins/MyPlugin.ofx.bundle/
   /Library/OFX/Plugins/MyPlugin.ofx.bundle/

   # Linux:
   ~/.local/share/OFX/Plugins/MyPlugin.ofx.bundle/
   /usr/OFX/Plugins/MyPlugin.ofx.bundle/
   ```

3. **Binary architecture mismatch**
   ```bash
   # Check binary architecture (macOS)
   lipo -info ~/Library/OFX/Plugins/MyPlugin.ofx.bundle/Contents/MacOS/MyPlugin.ofx

   # Should show: x86_64 arm64 (universal) or match host architecture
   ```

#### Issue 2: Clips Not Visible in General Context

**Symptoms:**
- Plugin loads
- Only "Source" and "Output" visible
- Additional clips missing

**Causes & Solutions:**

1. **Clips defined in wrong context**
   ```cpp
   // WRONG: Defining outside context check
   void describeInContext(...) {
       defineClip("Background");  // Always defined
   }

   // CORRECT: Define only in General
   void describeInContext(..., ContextEnum context) {
       if (context == eContextGeneral) {
           defineClip("Background");  // Only in General
       }
   }
   ```

2. **Host not using General context**
   ```cpp
   // Check at runtime which context was chosen
   void createInstance(...) {
       ContextEnum ctx = getContext();
       if (ctx != eContextGeneral) {
           // Host chose Filter instead
           // Additional clips won't exist
       }
   }
   ```

#### Issue 3: Crash When Fetching Optional Clip

**Symptoms:**
- Plugin crashes during render
- Crash when optional clip not connected
- Null pointer access

**Causes & Solutions:**

1. **Not checking if clip is connected**
   ```cpp
   // WRONG: Assumes clip is connected
   auto_ptr<Image> bg(bgClip_->fetchImage(time)); // Crash if not connected
   processWithBg(bg.get());  // NULL pointer access

   // CORRECT: Check connection first
   auto_ptr<Image> bg;
   if (bgClip_ && bgClip_->isConnected()) {
       bg.reset(bgClip_->fetchImage(time));
       if (bg.get()) {
           processWithBg(bg.get());
       }
   }
   ```

2. **Fetching clip that doesn't exist in context**
   ```cpp
   // WRONG: Fetching clip that wasn't defined
   void createInstance(...) {
       bgClip_ = fetchClip("Background");  // Might not exist in Filter
   }

   // CORRECT: Conditional fetching
   void createInstance(...) {
       if (getContext() == eContextGeneral) {
           bgClip_ = fetchClip("Background");
       } else {
           bgClip_ = nullptr;
       }
   }
   ```

#### Issue 4: Wrong Image Returned from Input

**Symptoms:**
- Colors wrong
- Image shifted/scaled incorrectly
- Garbage data

**Causes & Solutions:**

1. **Not respecting clip bounds**
   ```cpp
   // WRONG: Assuming same bounds
   OfxRectI srcBounds = src->getBounds();
   OfxRectI bgBounds = bg->getBounds();
   // Processing without checking if bounds match

   // CORRECT: Transform coordinates
   for (int y = renderWindow.y1; y < renderWindow.y2; y++) {
       for (int x = renderWindow.x1; x < renderWindow.x2; x++) {
           PIX *srcPix = src->getPixelAddress(x, y);  // Uses src bounds
           PIX *bgPix = bg->getPixelAddress(x, y);    // Uses bg bounds
           // Correct - getPixelAddress handles transform
       }
   }
   ```

2. **Component mismatch**
   ```cpp
   // WRONG: Assuming RGBA
   float *srcPix = (float*)src->getPixelAddress(x, y);
   float r = srcPix[0], g = srcPix[1], b = srcPix[2], a = srcPix[3];
   // Crash if source is RGB (only 3 components)

   // CORRECT: Check components
   PixelComponentEnum srcComps = src->getPixelComponents();
   int nComps = (srcComps == ePixelComponentRGBA) ? 4 : 3;
   ```

#### Issue 5: Multi-Input Performance Issues

**Symptoms:**
- Slow rendering with multiple inputs
- Worse performance than single input
- Host becomes unresponsive

**Causes & Solutions:**

1. **Fetching images unnecessarily**
   ```cpp
   // WRONG: Always fetching all clips
   auto_ptr<Image> bg(bgClip_->fetchImage(time));  // Fetched but not used

   // CORRECT: Fetch only when needed
   bool needsBg = /* check if processing needs it */;
   auto_ptr<Image> bg;
   if (needsBg && bgClip_->isConnected()) {
       bg.reset(bgClip_->fetchImage(time));
   }
   ```

2. **Not supporting tiles**
   ```cpp
   // Add in describeInContext
   dstClip->setSupportsTiles(true);
   srcClip->setSupportsTiles(true);
   bgClip->setSupportsTiles(true);
   // Allows host to render small regions at a time
   ```

3. **Not thread-safe**
   ```cpp
   // In describe()
   desc.setRenderThreadSafety(eRenderFullySafe);
   // Allows host to render multiple regions in parallel
   ```

#### Issue 6: Parameters Not Appearing

**Symptoms:**
- Additional parameters missing
- Parameters visible in Filter, missing in General
- UI incomplete

**Causes & Solutions:**

1. **Parameters defined only in one context**
   ```cpp
   // WRONG: Only defining in Filter
   void describeInContext(..., ContextEnum context) {
       if (context == eContextFilter) {
           defineDoubleParam("mix");  // Missing in General!
       }
   }

   // CORRECT: Define in all contexts
   void describeInContext(..., ContextEnum context) {
       // Define common parameters outside context check
       defineDoubleParam("mix");

       // Define context-specific parameters
       if (context == eContextGeneral) {
           defineBooleanParam("useMatte");
       }
   }
   ```

---

## Real-World Examples

### Example 1: Simple Over Composite

**Description:** Composite foreground over background with optional matte

**Clips:**
- `Source` (mandatory): Foreground image
- `Background` (optional): Background image
- `Matte` (optional): Alpha matte

**Implementation:**

```cpp
class OverComposite : public OFX::ImageEffect {
public:
    OverComposite(OfxImageEffectHandle handle) : ImageEffect(handle) {
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);

        if (getContext() == OFX::eContextGeneral) {
            bgClip_ = fetchClip("Background");
            matteClip_ = fetchClip("Matte");
        }

        mixParam_ = fetchDoubleParam("mix");
    }

    virtual void render(const OFX::RenderArguments &args) override {
        auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
        auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));

        if (getContext() == OFX::eContextFilter || !bgClip_->isConnected()) {
            // Simple passthrough or source-only processing
            copyPixels(src.get(), dst.get(), args.renderWindow);
            return;
        }

        auto_ptr<OFX::Image> bg(bgClip_->fetchImage(args.time));
        auto_ptr<OFX::Image> matte;

        if (matteClip_ && matteClip_->isConnected()) {
            matte.reset(matteClip_->fetchImage(args.time));
        }

        double mix;
        mixParam_->getValueAtTime(args.time, mix);

        compositeOver(src.get(), bg.get(), matte.get(), dst.get(),
                     args.renderWindow, mix);
    }

private:
    OFX::Clip *srcClip_, *bgClip_, *matteClip_, *dstClip_;
    OFX::DoubleParam *mixParam_;
};
```

### Example 2: Multi-Layer Blend

**Description:** Blend up to 4 layers with individual blend modes

**Clips:**
- `Layer1` through `Layer4` (Layer1 mandatory, rest optional)

**Parameters:**
- Blend mode per layer
- Opacity per layer

**Implementation:**

```cpp
class MultiLayerBlend : public OFX::ImageEffect {
public:
    MultiLayerBlend(OfxImageEffectHandle handle) : ImageEffect(handle) {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);

        for (int i = 0; i < 4; i++) {
            std::stringstream ss;
            ss << "Layer" << (i + 1);
            layerClips_[i] = fetchClip(ss.str());

            ss.str("");
            ss << "blendMode" << (i + 1);
            blendModeParams_[i] = fetchChoiceParam(ss.str());

            ss.str("");
            ss << "opacity" << (i + 1);
            opacityParams_[i] = fetchDoubleParam(ss.str());
        }
    }

    virtual void render(const OFX::RenderArguments &args) override {
        auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));

        // Fetch all connected layers
        std::vector<OFX::Image*> layers;
        for (int i = 0; i < 4; i++) {
            if (layerClips_[i]->isConnected()) {
                layers.push_back(layerClips_[i]->fetchImage(args.time));
            } else {
                layers.push_back(nullptr);
            }
        }

        // Start with first layer as base
        if (layers[0]) {
            copyPixels(layers[0], dst.get(), args.renderWindow);

            // Blend additional layers on top
            for (int i = 1; i < 4; i++) {
                if (layers[i]) {
                    int blendMode;
                    double opacity;
                    blendModeParams_[i]->getValueAtTime(args.time, blendMode);
                    opacityParams_[i]->getValueAtTime(args.time, opacity);

                    blendLayer(dst.get(), layers[i],
                              (BlendMode)blendMode, opacity,
                              args.renderWindow);
                }
            }
        }

        // Clean up
        for (auto* layer : layers) {
            delete layer;
        }
    }

private:
    OFX::Clip *layerClips_[4];
    OFX::Clip *dstClip_;
    OFX::ChoiceParam *blendModeParams_[4];
    OFX::DoubleParam *opacityParams_[4];
};
```

### Example 3: Texture-Mapped Effect

**Description:** Apply effect using texture map for spatial control

**Clips:**
- `Source` (mandatory): Image to process
- `TextureMap` (optional): RGB texture controlling effect
- `Matte` (optional): Overall effect matte

**Implementation:**

```cpp
class TextureMappedEffect : public OFX::ImageEffect {
public:
    virtual void render(const OFX::RenderArguments &args) override {
        auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
        auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));

        auto_ptr<OFX::Image> texture;
        if (textureClip_ && textureClip_->isConnected()) {
            texture.reset(textureClip_->fetchImage(args.time));
        }

        auto_ptr<OFX::Image> matte;
        if (matteClip_ && matteClip_->isConnected()) {
            matte.reset(matteClip_->fetchImage(args.time));
        }

        double effectStrength;
        strengthParam_->getValueAtTime(args.time, effectStrength);

        // Process each pixel
        for (int y = args.renderWindow.y1; y < args.renderWindow.y2; y++) {
            for (int x = args.renderWindow.x1; x < args.renderWindow.x2; x++) {
                float *srcPix = (float*)src->getPixelAddress(x, y);
                float *dstPix = (float*)dst->getPixelAddress(x, y);

                // Get texture values (RGB used as effect parameters)
                float texR = 1.0f, texG = 1.0f, texB = 1.0f;
                if (texture.get()) {
                    float *texPix = (float*)texture->getPixelAddress(x, y);
                    texR = texPix[0];
                    texG = texPix[1];
                    texB = texPix[2];
                }

                // Get matte value
                float matteValue = 1.0f;
                if (matte.get()) {
                    float *mattePix = (float*)matte->getPixelAddress(x, y);
                    matteValue = mattePix[0];
                }

                // Apply effect using texture and matte
                // texR controls hue shift
                // texG controls saturation
                // texB controls brightness
                // matteValue controls overall intensity

                applyTexturedEffect(srcPix, dstPix,
                                   texR, texG, texB,
                                   matteValue, effectStrength);
            }
        }
    }
};
```

---

## Multiple Outputs Workaround

### The Problem

OFX **does not support multiple output clips**. All plugins have exactly one output named "Output".

### Solution 1: RGBA with Embedded Alpha (Recommended)

**Embed secondary data in alpha channel:**

```cpp
// Example: Keyer with embedded matte
void render(const OFX::RenderArguments &args) {
    // Process keying
    for (each pixel) {
        float r, g, b, matte;
        computeKey(inputPixel, &r, &g, &b, &matte);

        // Output RGBA:
        // RGB = keyed foreground
        // A = matte/alpha
        outputPixel[0] = r;
        outputPixel[1] = g;
        outputPixel[2] = b;
        outputPixel[3] = matte;  // Matte in alpha channel
    }
}
```

**In host application (Flame/Nuke):**
- RGB shows keyed image
- Alpha channel contains matte
- User can "shuffle" or "extract" alpha to separate layer if needed

**This is the industry standard approach.**

### Solution 2: Custom EXR Channels

**Use EXR's arbitrary channel support:**

```cpp
// Write custom channels to EXR
// Beauty.R, Beauty.G, Beauty.B (main image)
// Matte.Y (matte as separate channel)
// Depth.Z (depth pass)
```

**Limitations:**
- Not directly supported by OFX (would need custom file I/O)
- Host may not recognize custom channels
- More complex implementation

### Solution 3: Separate Plugins

**Create two plugins:**
- `MyEffect` - outputs main result
- `MyEffect Matte` - outputs matte

**Limitations:**
- User must run both plugins
- Synchronization complexity
- Double processing time
- Poor user experience

**Verdict:** Use Solution 1 (RGBA with embedded alpha) in almost all cases.

---

## Checklist for Multi-Input Plugin

### Planning Phase
- [ ] Determine required inputs and their purposes
- [ ] Decide which inputs are mandatory vs optional
- [ ] Choose supported contexts (Filter + General recommended)
- [ ] Plan fallback behavior for missing optional inputs
- [ ] Design parameter UI for multi-input controls

### Implementation Phase
- [ ] Declare supported contexts in `describe()`
- [ ] Define clips in `describeInContext()` with context checks
- [ ] Set clip properties (optional, mask, components, etc.)
- [ ] Cache clip handles in `createInstance()`
- [ ] Check context and handle missing clips gracefully
- [ ] Implement connection checking for optional clips
- [ ] Handle different component types across inputs
- [ ] Implement proper RoI handling for multiple inputs
- [ ] Support tiles and threading

### Testing Phase
- [ ] Test in Filter context (single input)
- [ ] Test in General context (multiple inputs)
- [ ] Test with all inputs connected
- [ ] Test with optional inputs disconnected
- [ ] Test with different component types (RGBA, RGB, Alpha)
- [ ] Test with different bit depths
- [ ] Test render correctness
- [ ] Test parameter visibility and functionality
- [ ] Profile performance with multiple inputs
- [ ] Test in at least 2 different host applications

### Documentation Phase
- [ ] Document required vs optional inputs
- [ ] Explain input purposes in user documentation
- [ ] Document supported contexts
- [ ] Provide workflow examples
- [ ] Document fallback behavior

---

## Conclusion

Multiple inputs in OFX enable powerful compositing and multi-source effects. Key takeaways:

1. **Use General Context** for multiple custom-named inputs
2. **Support both Filter and General** for maximum compatibility
3. **Make inputs optional** when fallback behavior exists
4. **Always check connection** before fetching optional clips
5. **Handle different component types** gracefully
6. **Test across multiple hosts** to ensure compatibility
7. **Use RGBA with alpha** for "multiple outputs" (industry standard)

The General context is specifically designed for node-graph compositors like Nuke, Flame, and Fusion. Supporting it alongside Filter context maximizes your plugin's reach across both timeline-based and node-based hosts.

---

**Document Version:** 1.0
**Last Updated:** 2025-12-16
**Maintained By:** OpenFX Community Contributors
