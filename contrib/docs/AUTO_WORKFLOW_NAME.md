# Auto-Workflow Name Derivation

## Feature Overview

**Version**: 1.1.0
**Date**: December 30, 2025

The AnyComfy plugin now automatically derives the "Workflow Name" parameter from the selected workflow filename. This reduces manual parameter entry and prevents errors from typos or inconsistent naming.

## How It Works

When you select a workflow file, the plugin automatically:

1. **Extracts the filename** from the full path
2. **Removes the extension** (.json)
3. **Splits by separators**: underscore (`_`), hyphen (`-`), dot (`.`), space (` `)
4. **Filters out common keywords**:
   - `workflow`, `wf`
   - `api`
   - `comfyui`, `comfy`
   - `json`
5. **Joins remaining words** with underscores
6. **Updates the "Workflow Name" parameter**

## Examples

| Workflow Filename | Derived Name |
|-------------------|--------------|
| `comfyui_normal_map_deepbump_workflow_api.json` | `normal_map_deepbump` |
| `segmentation_workflow.json` | `segmentation` |
| `my-awesome-effect_wf.json` | `my_awesome_effect` |
| `SAM Segmentation API.json` | `sam_segmentation` |
| `upscale-4x.workflow.json` | `upscale_4x` |
| `DeepBump-NormalMap.json` | `deepbump_normalmap` |

## User Control

### Auto-Update Behavior

The plugin auto-updates the workflow name **only when**:
- The current value is **empty**, OR
- The current value is the **default** (`"segmentation"`)

### Manual Override Preserved

If you've manually changed the "Workflow Name" parameter, the plugin will **NOT** overwrite it.

**Example**:
```
1. User selects: "upscale_workflow.json"
   → Auto-derived: "upscale"

2. User manually changes to: "my_upscale"

3. User selects different file: "denoise_api.json"
   → Workflow name stays: "my_upscale" (manual override preserved)
```

To reset auto-derivation, simply clear the "Workflow Name" field or reset it to default.

## Why This Matters

### Before (v1.0.x)

**User workflow**:
1. Select workflow: `comfyui_normal_map_deepbump_workflow_api.json`
2. Manually type workflow name: `normal_map_deepbump`
3. Risk: typo → `normal_map_deepump` → wrong directory → render fails

### After (v1.1.0+)

**User workflow**:
1. Select workflow: `comfyui_normal_map_deepbump_workflow_api.json`
2. ✓ Workflow name auto-populated: `normal_map_deepbump`
3. No manual typing, no errors

## Output Directory Structure

The derived workflow name is used to create output directories:

**Example**:
```
Workflow file: comfyui_segmentation_segment_anything_workflow_api.json
↓
Derived name:  segmentation_segment_anything
↓
Output path:   /out/PROJECT/segmentation_segment_anything/v001/
```

This ensures consistent naming between workflow files and output directories.

## Logging

The plugin logs auto-derivation activity:

**Info level** (when auto-update occurs):
```
[info] Auto-derived workflow name from filename:
       '/workflows/normal_map_deepbump_workflow.json' -> 'normal_map_deepbump'
```

**Debug level** (when skipped due to manual override):
```
[debug] Workflow name not auto-updated (user has custom value: 'my_custom_name')
```

**Debug level** (detailed parsing):
```
[debug] Derived workflow name: 'comfyui_upscale_wf.json' -> 'upscale'
```

## Edge Cases

### All Words Filtered

If filtering removes **all** words (rare), the plugin uses the original filename without extension:

```
Filename: workflow-api.json
All filtered: workflow, api
Fallback: workflow-api
```

**Warning logged**:
```
[warn] All keywords filtered from filename 'workflow-api', using original
```

### Empty Filename

If the workflow path is empty or invalid, auto-derivation is skipped.

### Special Characters

Special characters are preserved in individual words:
```
my-effect_v2.1.json  →  my_effect_v2_1
SAM2.0-api.json      →  sam2_0
```

## Technical Details

### Implementation

**File**: [anycomfy_plugin.cpp](anycomfy_plugin.cpp)

**Method**: `AnyComfyPlugin::deriveWorkflowNameFromFilename()`
**Lines**: 449-511

**Trigger**: `AnyComfyPlugin::changedParam()` on `workflowFilePath` change
**Lines**: 60-88

### Processing Algorithm

```cpp
std::string deriveWorkflowNameFromFilename(const std::string& filepath)
{
    // 1. Extract filename from path
    std::string filename = extractFilename(filepath);

    // 2. Remove .json extension
    filename = removeExtension(filename, ".json");

    // 3. Split by separators: _ - . (space)
    std::vector<std::string> words = splitBySeparators(filename);

    // 4. Convert to lowercase and filter keywords
    words = filterKeywords(words, {"workflow", "wf", "api", "comfyui", "json", "comfy"});

    // 5. Join with underscores
    return joinWithUnderscore(words);
}
```

### Performance

- **Time complexity**: O(n) where n = filename length
- **Memory**: Minimal (small string operations)
- **Trigger**: Only on parameter change (not during render)

## Compatibility

### Plugin Versions

- **v1.0.0-1.0.4**: Manual workflow name entry only
- **v1.1.0+**: Auto-derivation from filename

### Workflow Files

Works with **any** workflow filename format:
- Template workflows (with `${variables}`)
- Non-templated workflows (raw ComfyUI exports)
- Custom naming conventions
- Files from any directory

### Host Applications

Tested with:
- ✓ Flame
- ✓ Nuke (expected to work)
- ✓ Resolve (expected to work)

## Best Practices

### Naming Workflow Files

For best auto-derivation results:

**Good** (descriptive, uses separators):
```
✓ normal_map_generator.json
✓ upscale-4x-esrgan.json
✓ denoise_temporal.json
✓ SAM_segmentation.json
```

**Less ideal** (too generic, requires manual override):
```
△ workflow.json           → derived: "workflow" (too generic)
△ effect1.json            → derived: "effect1" (not descriptive)
△ test.json               → derived: "test" (not descriptive)
```

### When to Override

Manually set the workflow name when:
- Derived name is too long
- You want a simpler name
- You have multiple workflows with similar names
- You're using a generic filename

**Example**:
```
File: comfyui_super_resolution_esrgan_4x_upscale_workflow_api.json
Auto: super_resolution_esrgan_4x_upscale  ← too long!
Manual override: upscale_4x               ← better!
```

## Troubleshooting

### Workflow Name Didn't Auto-Update

**Possible causes**:
1. You previously set a custom workflow name → Reset field to default
2. Workflow path is empty → Select a workflow file
3. Filename parsing failed → Check logs for warnings

**Solution**: Clear the "Workflow Name" field and select workflow again

### Derived Name Is Wrong

The auto-derivation follows strict rules. If the result isn't what you want:
1. Rename the workflow file to be more descriptive
2. OR manually override the "Workflow Name" parameter

### Output Directory Has Wrong Name

Check:
1. **Workflow Name parameter** - Is it what you expect?
2. **Project Name parameter** - Might be set incorrectly
3. **Logs** - Look for "Auto-derived workflow name" messages

## Future Enhancements

### Planned Improvements

1. **Configurable keywords** - Allow users to define custom filter words
2. **Name validation** - Check for invalid directory characters
3. **Name suggestions** - Show multiple options for user to choose
4. **Pattern preservation** - Maintain CamelCase or specific formatting

### User Feedback

If you have suggestions for improving the auto-derivation algorithm, please:
- Check existing issues/discussions
- Provide examples of problematic filenames
- Suggest improved parsing rules

## Summary

**Problem**: Users had to manually type workflow names, leading to typos and inconsistencies

**Solution**: Auto-derive workflow name from filename using intelligent parsing

**Result**:
- Less manual parameter entry
- Consistent naming between files and directories
- Fewer errors from typos
- User can still override when needed

---

**Added**: December 30, 2025
**Version**: 1.1.0
**Feature Type**: UX Enhancement
**Impact**: All users, automatic, non-breaking
