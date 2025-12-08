#!/bin/bash
# ComfyUI Plugin Pre-Test Verification Script

echo "=== ComfyUI Plugin Pre-Test Verification ==="
echo

# 1. Check plugin installation
echo "1. Checking plugin installation..."
PLUGIN_PATH=~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
if [ -f "$PLUGIN_PATH" ]; then
    TIMESTAMP=$(stat -f "%Sm" -t "%Y-%m-%d %H:%M:%S" "$PLUGIN_PATH")
    SIZE=$(ls -lh "$PLUGIN_PATH" | awk '{print $5}')
    echo "   ✓ Plugin found: $SIZE, built $TIMESTAMP"
    if [[ "$TIMESTAMP" < "2025-11-07 13:18:00" ]]; then
        echo "   ⚠ WARNING: Plugin is older than expected (should be 2025-11-07 13:18:10)"
    fi
else
    echo "   ✗ Plugin NOT found at $PLUGIN_PATH"
fi
echo

# 2. Check ComfyUI server
echo "2. Checking ComfyUI server..."
if curl -s --connect-timeout 5 http://192.168.1.211:8188/system_stats > /dev/null 2>&1; then
    echo "   ✓ ComfyUI server responding at 192.168.1.211:8188"
else
    echo "   ✗ ComfyUI server NOT responding at 192.168.1.211:8188"
    echo "   → Make sure ComfyUI is running"
fi
echo

# 3. Check network storage mount
echo "3. Checking network storage..."
if [ -d "/Volumes/silo2/002_COMFYUI" ]; then
    echo "   ✓ Network storage mounted at /Volumes/silo2/002_COMFYUI"
else
    echo "   ✗ Network storage NOT mounted at /Volumes/silo2/002_COMFYUI"
    echo "   → Mount the network share first"
fi
echo

# 4. Check/create required directories
echo "4. Checking required directories..."
DIRS=(
    "/Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation"
    "/Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001"
)
for dir in "${DIRS[@]}"; do
    if [ -d "$dir" ]; then
        echo "   ✓ $dir"
    else
        echo "   ⚠ Creating: $dir"
        mkdir -p "$dir" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "   ✓ Created successfully"
        else
            echo "   ✗ Failed to create (check permissions)"
        fi
    fi
done
echo

# 5. Check log files
echo "5. Checking log files..."
LOG_COUNT=$(ls -1 ~/comfyui_plugin_*.log 2>/dev/null | wc -l)
if [ $LOG_COUNT -gt 0 ]; then
    echo "   ✓ Found $LOG_COUNT log file(s)"
    LATEST_LOG=$(ls -t ~/comfyui_plugin_*.log 2>/dev/null | head -1)
    if [ -f "$LATEST_LOG" ]; then
        echo "   Latest: $(basename $LATEST_LOG)"
    fi
else
    echo "   ℹ No log files yet (will be created on first render)"
fi
echo

# 6. Summary
echo "=== Pre-Test Summary ==="
echo "Plugin version: 2025-11-07 13:18:10"
echo "Ready to test: Start Flame and render a frame with SAMSegmentation plugin"
echo
echo "Monitor in real-time with:"
echo "  tail -f ~/comfyui_plugin_*.log"
echo
echo "Quick test checklist:"
echo "  cd ~/Library/OFX/Plugins && cat ~/COMFYUI_PLUGIN_TEST_CHECKLIST.md"
echo
