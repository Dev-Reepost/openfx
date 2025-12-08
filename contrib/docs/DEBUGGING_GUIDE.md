# ComfyUI Plugin Debugging Guide

## "Plugin rendering failed" Error in Flame

When you see repeated "Plugin rendering failed" messages in the shell, the OFX plugin is throwing an exception during render().

### Step 1: Check Required Parameters

**ALL of these parameters MUST be set before rendering:**

#### Server Configuration
- ✅ Server Address (default: "localhost")
- ✅ Server Port (default: 8188)

#### Storage Configuration (CRITICAL - these have NO defaults!)
- ⚠️ **Shared Mount Path** - MUST be set (e.g., "/tmp/comfyui_shared")
- ⚠️ **Project Name** - MUST be set (e.g., "test_project")
- ⚠️ **Workflow Name** - MUST be set (e.g., "segmentation")
- ⚠️ **Basename** - MUST be set (e.g., "shot01")
- ⚠️ **Layer Name** - MUST be set (e.g., "beauty")
- ⚠️ **Output Version** - MUST be set (e.g., "v001")

#### Segmentation (have defaults, usually OK)
- ✅ Prompt (default: "foreground")
- ✅ Threshold (default: 0.3)
- ✅ SAM Model (default: ViT-H)
- ✅ Grounding DINO Model (default: SwinT)
- ✅ Resolution (default: 1080)

### Step 2: Check Flame Logs

**macOS Flame logs:**
```bash
# Check system console for Flame messages
log show --predicate 'process == "flame"' --last 5m | grep -i "ofx\|plugin\|error"

# Check crash reports
ls -lt ~/Library/Logs/DiagnosticReports/ | grep -i flame | head -5

# Check for OFX-specific logs
find /opt/Autodesk -name "*.log" 2>/dev/null | xargs grep -l "SAMSegmentation" 2>/dev/null
```

### Step 3: Verify Shared Storage Setup

The plugin will fail immediately if it cannot write to the shared storage directory:

```bash
# Create the directory structure
mkdir -p /tmp/comfyui_shared/in/test_project/segmentation
mkdir -p /tmp/comfyui_shared/out/test_project/segmentation

# Set permissions
chmod -R 777 /tmp/comfyui_shared

# Verify writeable
touch /tmp/comfyui_shared/in/test_project/segmentation/test.txt && \
  echo "✓ Directory is writable" && \
  rm /tmp/comfyui_shared/in/test_project/segmentation/test.txt
```

### Step 4: Common Failure Points

**The render() method fails at these points (in order):**

1. **Line 100**: Failed to get Server Address parameter
   - *Fix*: Ensure parameter exists and has valid value

2. **Line 111**: Failed to fetch source image
   - *Fix*: Ensure plugin is connected to a valid source clip

3. **Line 112**: Failed to write input image to shared storage
   - *Cause*: Cannot write to Shared Mount Path
   - *Fix*: Check directory exists and is writable

4. **Line 116**: Failed to build workflow JSON
   - *Cause*: Missing required parameters (Project Name, Workflow Name, etc.)
   - *Fix*: Set all Storage Configuration parameters

5. **Line 120**: Failed to connect to ComfyUI server
   - *Cause*: Server not running or wrong address/port
   - *Fix*: Start ComfyUI and verify with `curl http://localhost:8188/system_stats`

6. **Line 127-178**: ComfyUI execution failed
   - *Cause*: Model not found, invalid workflow, server error
   - *Check*: ComfyUI server console for error messages

7. **Line 185**: Failed to find output file
   - *Cause*: ComfyUI didn't generate expected output file
   - *Check*: Look for files in `/tmp/comfyui_shared/out/...`

8. **Line 198**: Failed to read EXR output
   - *Cause*: File doesn't exist or is corrupted
   - *Fix*: Check ComfyUI actually wrote the file

### Step 5: Enable Detailed Error Messages (Development Build)

Since Flame swallows the exception messages, we need to add logging. Create a test build with error logging:

**Add to comfyui_base_plugin.cpp render() method:**
```cpp
void BasePlugin::render(const OFX::RenderArguments &args)
{
    try {
        executeWorkflow(args);
    } catch (const std::exception& e) {
        // Write error to a file since Flame swallows it
        std::ofstream errorLog("/tmp/sam_plugin_error.log", std::ios::app);
        errorLog << "Render failed: " << e.what() << std::endl;
        errorLog.close();
        throw; // Re-throw for Flame
    }
}
```

Then check the log:
```bash
tail -f /tmp/sam_plugin_error.log
```

### Step 6: Test Minimal Configuration

**In Flame, set ONLY these parameters:**

```
Server Configuration:
  Server Address: localhost
  Server Port: 8188

Storage Configuration:
  Shared Mount Path: /tmp/comfyui_shared
  Project Name: test
  Workflow Name: seg
  Basename: shot
  Layer Name: main
  Output Version: v1

Segmentation:
  Prompt: person
  (leave others at defaults)
```

### Step 7: Verify ComfyUI Server

Before testing in Flame:

```bash
# 1. Start ComfyUI
cd ~/ComfyUI
python main.py --listen 0.0.0.0 --port 8188

# 2. In another terminal, verify it's running
curl http://localhost:8188/system_stats

# 3. Check models are available
curl http://localhost:8188/object_info | grep -i "sam\|grounding"
```

### Step 8: Manual Workflow Test

Test if the workflow JSON is valid by posting it directly to ComfyUI:

```bash
# Build a test workflow (you can extract this from the code)
cat > /tmp/test_workflow.json << 'EOF'
{
  "prompt": {
    "1": {
      "inputs": {
        "filepath": "/tmp/comfyui_shared/in/test/seg/shot_main_0001_v1_.exr",
        "linear_to_sRGB": "true",
        "image_load_cap": 0,
        "skip_first_images": 0,
        "select_every_nth": 1
      },
      "class_type": "LoadEXR"
    }
  },
  "client_id": "test-client"
}
EOF

# Post to ComfyUI
curl -X POST http://localhost:8188/prompt \
  -H "Content-Type: application/json" \
  -d @/tmp/test_workflow.json
```

### Step 9: Check Extension Installation

Verify the ComfyUI Segment Anything extension is properly installed:

```bash
# Check extension directory
ls -la ~/ComfyUI/custom_nodes/comfyui_segment_anything/

# Check for required nodes
cat ~/ComfyUI/custom_nodes/comfyui_segment_anything/__init.py | grep -i "NODE_CLASS_MAPPINGS"

# Test extension loads without errors
cd ~/ComfyUI
python -c "import custom_nodes.comfyui_segment_anything"
```

### Step 10: Known Issues

**Issue: "Plugin rendering failed" immediately**
- *Cause*: Missing Storage Configuration parameters
- *Solution*: Set ALL six storage parameters

**Issue: Flame hangs after applying plugin**
- *Cause*: WebSocket connection timeout
- *Solution*: Check ComfyUI server is reachable

**Issue: No error message, just fails silently**
- *Cause*: OFX catches exceptions but Flame doesn't show them
- *Solution*: Add file logging (see Step 5)

**Issue: Models not found**
- *Cause*: SAM or Grounding DINO models not installed
- *Solution*: Download models to `~/ComfyUI/models/` directory

### Step 11: Quick Verification Script

```bash
#!/bin/bash
# Save as: check_sam_plugin_setup.sh

echo "=== ComfyUI SAM Plugin Setup Check ==="
echo

# Check plugin installation
if [ -d ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle ]; then
    echo "✓ Plugin installed"
else
    echo "✗ Plugin NOT found in ~/Library/OFX/Plugins/"
    exit 1
fi

# Check shared storage
if [ -w /tmp/comfyui_shared/in ]; then
    echo "✓ Shared storage writable"
else
    echo "✗ Shared storage not writable"
    echo "  Run: mkdir -p /tmp/comfyui_shared/in && chmod 777 /tmp/comfyui_shared"
fi

# Check ComfyUI server
if curl -s http://localhost:8188/system_stats > /dev/null; then
    echo "✓ ComfyUI server running"
else
    echo "✗ ComfyUI server NOT running"
    echo "  Start with: cd ~/ComfyUI && python main.py"
fi

# Check for SAM extension
if [ -d ~/ComfyUI/custom_nodes/comfyui_segment_anything ]; then
    echo "✓ Segment Anything extension installed"
else
    echo "✗ Segment Anything extension NOT installed"
fi

# Check for models
SAM_COUNT=$(find ~/ComfyUI/models -name "sam_vit*.pth" 2>/dev/null | wc -l)
DINO_COUNT=$(find ~/ComfyUI/models -name "GroundingDINO*.pth" 2>/dev/null | wc -l)

if [ $SAM_COUNT -gt 0 ]; then
    echo "✓ SAM models found ($SAM_COUNT)"
else
    echo "✗ No SAM models found"
fi

if [ $DINO_COUNT -gt 0 ]; then
    echo "✓ Grounding DINO models found ($DINO_COUNT)"
else
    echo "✗ No Grounding DINO models found"
fi

echo
echo "=== Setup Check Complete ==="
```

Run it:
```bash
chmod +x check_sam_plugin_setup.sh
./check_sam_plugin_setup.sh
```

### Most Likely Solution

Based on "Plugin rendering failed" immediately in Flame, the most likely cause is:

**Missing Storage Configuration Parameters**

In Flame's plugin UI, make sure you fill in ALL of these:
- Shared Mount Path
- Project Name
- Workflow Name
- Basename
- Layer Name
- Output Version

If ANY of these are empty strings, the plugin will fail when trying to build file paths at line 243-247 in comfyui_base_plugin.cpp.
