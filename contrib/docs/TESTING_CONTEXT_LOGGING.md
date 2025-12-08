# Testing OFX Context Properties - Logging Guide

## What Was Added

The plugin now logs **all available OFX context properties** to help us understand what information is accessible from different host applications (Flame, Nuke, Resolve, etc.).

## Log File Location

```bash
~/comfyui_plugin_YYYYMMDD.log
```

Example: `~/comfyui_plugin_20251120.log`

## What Gets Logged

### 1. During Plugin Construction (First Time Load)

When the plugin is first instantiated in the host:

```
=== OFX Context Properties Discovery ===

Instance kOfxPropName: '<value_or_NOT_AVAILABLE>'
Instance kOfxPropLabel: '<value_or_NOT_AVAILABLE>'
Instance kOfxPropShortLabel: '<value_or_NOT_AVAILABLE>'
Instance kOfxPropLongLabel: '<value_or_NOT_AVAILABLE>'

--- Source Clip Properties ---
Source Clip kOfxPropName: '<value_or_NOT_AVAILABLE>'
Source Clip kOfxPropLabel: '<value_or_NOT_AVAILABLE>'
Source Clip Connected: <YES/NO/UNKNOWN>

--- Output Clip Properties ---
Output Clip kOfxPropName: '<value_or_NOT_AVAILABLE>'
Output Clip kOfxPropLabel: '<value_or_NOT_AVAILABLE>'

=== End OFX Context Properties ===
```

### 2. During Each Render Call

When the plugin renders a frame:

```
========================================
RENDER STARTED - Frame: <frame_number>
Render window: (x1,y1) to (x2,y2)
Render scale: <scale_factor>
Interactive: <0/1>, Draft: <0/1>, Sequential: <0/1>

--- Runtime Source Clip Info ---
Source Clip Connected: <YES/NO>
Source Clip Name: '<clip_name>'
Source Clip Label: '<clip_label>'
Source Clip Components: <RGBA/RGB/etc>

========================================
```

### 3. Auto-Generated Basename Logging

When `getEffectiveBasename()` is called:

```
Auto-generated basename: <project>_<instance_name> (from project='<value>', instance='<sanitized_name>')
```

OR

```
WARNING: Auto-generate enabled but no instance name available, using manual basename: <fallback>
```

## How to Test

1. **Launch Flame** (or other OFX host)
2. **Load a clip** into the timeline/batch
3. **Apply SAMSegmentation plugin** to the clip
4. **Check the log immediately** - construction logging happens on plugin load
5. **Enable processing** and render a frame
6. **Check the log again** - runtime logging happens during render

## What to Look For

### Key Questions:

1. **What does `kOfxPropName` return?**
   - Generic ID? (e.g., "effect_1", UUID)
   - Meaningful name? (e.g., "SAM_node", "segmentation_1")
   - Node name? (e.g., user-renamed node in Flame)

2. **What does `kOfxPropLabel` return?**
   - Same as `kOfxPropName`?
   - User-visible label?
   - Plugin type name?

3. **What is the Source Clip name?**
   - Is it the connected clip/layer name?
   - Generic "Source"?
   - Actual media filename?

4. **Does it differ between hosts?**
   - Flame vs Nuke vs Resolve behavior
   - Interactive vs background render differences

## Expected Outcomes

### Best Case Scenario:
```
Instance kOfxPropName: 'sam_segmentation_node_1'
Source Clip Name: 'my_media_clip'
```
→ Auto-basename would be: `TEST_SAM_sam_segmentation_node_1`

### Realistic Scenario:
```
Instance kOfxPropName: 'uk.co.example.SAMSegmentation_001'
Source Clip Name: 'Source'
```
→ Auto-basename would be: `TEST_SAM_uk_co_example_SAMSegmentation_001`

### Worst Case Scenario:
```
Instance kOfxPropName: NOT AVAILABLE
Instance kOfxPropLabel: 'SAM Segmentation'
Source Clip Name: 'Source'
```
→ Auto-basename would fall back to: `TEST_SAM_SAM_Segmentation`

## Quick Log Check Commands

```bash
# View entire log
cat ~/comfyui_plugin_$(date +%Y%m%d).log

# Filter for context properties only
grep "OFX Context\|Instance kOfx\|Clip kOfx" ~/comfyui_plugin_$(date +%Y%m%d).log

# Filter for basename generation
grep "Auto-generated basename" ~/comfyui_plugin_$(date +%Y%m%d).log

# Check for errors
grep "NOT AVAILABLE\|WARN\|ERROR" ~/comfyui_plugin_$(date +%Y%m%d).log
```

## Next Steps After Testing

Based on the log results:

1. **If instance names are meaningful**: Keep auto-generate enabled by default ✅
2. **If instance names are UUIDs/generic**: Consider changing default to manual basename ⚠️
3. **If clip names are useful**: Consider adding clip name to basename pattern 💡
4. **If nothing useful**: Document that manual basename is required 📝

## Comparison with Pybox

Pybox has direct access to Flame's Python API:
```python
flame.project.name  # e.g., "TEST_SAM"
flame.batch.node.name  # e.g., "my_node"
```

OFX only has:
```cpp
getPropertySet().propGetString(kOfxPropName)  # ??? (host-dependent)
```

This test will reveal if OFX can provide equivalent information across different hosts.
