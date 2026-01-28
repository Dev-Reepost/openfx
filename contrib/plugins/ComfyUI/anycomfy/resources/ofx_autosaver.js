// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * OFX Auto-Save Extension for ComfyUI
 *
 * Automatically saves workflows back to their original subdirectory when modified,
 * so the OFX plugin can find them for processing.
 *
 * Works with the OFX AutoLoader to maintain the workflow in its subdirectory:
 *   <input_dir>/workflows/<workflow_name>/<workflow_name>.json
 *   <input_dir>/workflows/<workflow_name>/<workflow_name>_api.json
 */

import { app } from "../../scripts/app.js";

app.registerExtension({
    name: "OFX.AutoSave",

    async setup() {
        console.log("[OFX AutoSave] Setting up auto-save extension");

        // Track if this workflow was auto-loaded from OFX plugin
        let originalWorkflowName = null;
        let lastSaveTimestamp = 0;
        let saveInProgress = false;

        // Check if we loaded via ?load_local_json parameter
        const params = new URLSearchParams(window.location.search);
        const loadedFileName = params.get('load_local_json');

        // Parse path into subfolder and filename
        // e.g., "workflows/my_workflow/my_workflow.json" -> subfolder="workflows/my_workflow", filename="my_workflow.json"
        let subfolder = "";
        let actualFileName = "";

        if (loadedFileName) {
            originalWorkflowName = loadedFileName;

            const lastSlash = loadedFileName.lastIndexOf('/');
            if (lastSlash !== -1) {
                subfolder = loadedFileName.substring(0, lastSlash);
                actualFileName = loadedFileName.substring(lastSlash + 1);
            } else {
                actualFileName = loadedFileName;
            }

            console.log("[OFX AutoSave] Tracking workflow:", originalWorkflowName);
            console.log("[OFX AutoSave] Subfolder:", subfolder || "(root)");
            console.log("[OFX AutoSave] Filename:", actualFileName);
        }

        // Function to perform the actual save
        async function performAutoSave(source) {
            if (!originalWorkflowName || saveInProgress) {
                return;
            }

            // Debounce: Don't save more than once per 2 seconds
            const now = Date.now();
            if (now - lastSaveTimestamp < 2000) {
                console.log("[OFX AutoSave] Skipping save (too soon since last save)");
                return;
            }

            saveInProgress = true;
            lastSaveTimestamp = now;

            try {
                console.log("[OFX AutoSave] Auto-save triggered by:", source);
                console.log("[OFX AutoSave] Saving back to input directory:", originalWorkflowName);

                // Get the workflow data in API format for execution
                const promptResult = await app.graphToPrompt();

                // Debug: check structure of graphToPrompt result
                console.log("[OFX AutoSave] graphToPrompt() result keys:", Object.keys(promptResult));
                console.log("[OFX AutoSave] graphToPrompt() full result:", JSON.stringify(promptResult).substring(0, 500));

                // Also get UI format for later editing
                const workflowUI = app.graph.serialize();

                // Save API format (for OFX plugin execution)
                // graphToPrompt() typically returns { workflow: {...}, output: {...} }
                // where 'output' is the API format prompt and 'workflow' is UI format
                const apiFileName = actualFileName.replace('.json', '_api.json');

                // Try to get the API format from various possible properties
                let apiFormat = null;
                if (promptResult.output) {
                    apiFormat = promptResult.output;
                    console.log("[OFX AutoSave] Using API format from: output");
                } else if (promptResult.prompt) {
                    apiFormat = promptResult.prompt;
                    console.log("[OFX AutoSave] Using API format from: prompt");
                } else if (promptResult.workflow && typeof promptResult.workflow === 'object' && !Array.isArray(promptResult.workflow.nodes)) {
                    // If workflow is an object without 'nodes' array, it might be API format
                    apiFormat = promptResult.workflow;
                    console.log("[OFX AutoSave] Using API format from: workflow (appears to be API format)");
                } else {
                    console.error("[OFX AutoSave] ✗ Could not find API format in graphToPrompt result!");
                    console.error("[OFX AutoSave] Available keys:", Object.keys(promptResult));
                    // Fallback: try to use the whole result if it looks like API format
                    if (!promptResult.nodes) {
                        apiFormat = promptResult;
                        console.log("[OFX AutoSave] Using entire result as API format (fallback)");
                    } else {
                        throw new Error("Cannot determine API format from graphToPrompt result");
                    }
                }

                const apiBlob = new Blob([JSON.stringify(apiFormat, null, 2)], { type: 'application/json' });

                // Save UI format (for later editing)
                const uiBlob = new Blob([JSON.stringify(workflowUI, null, 2)], { type: 'application/json' });

                // Upload both formats to input directory with overwrite enabled
                // Use the subfolder to save back to the original location
                const formDataAPI = new FormData();
                formDataAPI.append('image', apiBlob, apiFileName);
                formDataAPI.append('subfolder', subfolder);
                formDataAPI.append('type', 'input');
                formDataAPI.append('overwrite', 'true');  // Force overwrite

                const formDataUI = new FormData();
                formDataUI.append('image', uiBlob, actualFileName);
                formDataUI.append('subfolder', subfolder);
                formDataUI.append('type', 'input');
                formDataUI.append('overwrite', 'true');  // Force overwrite

                console.log("[OFX AutoSave] Saving to subfolder:", subfolder || "(root)");

                // Upload API format
                const responseAPI = await fetch('/upload/image', {
                    method: 'POST',
                    body: formDataAPI
                });

                if (responseAPI.ok) {
                    const resultAPI = await responseAPI.json();
                    console.log("[OFX AutoSave] ✓ Saved API format:", resultAPI.name || apiFileName);
                } else {
                    console.error("[OFX AutoSave] ✗ Failed to save API format. Status:", responseAPI.status);
                }

                // Upload UI format
                const responseUI = await fetch('/upload/image', {
                    method: 'POST',
                    body: formDataUI
                });

                if (responseUI.ok) {
                    const resultUI = await responseUI.json();
                    console.log("[OFX AutoSave] ✓ Saved UI format:", resultUI.name || originalWorkflowName);
                } else {
                    console.error("[OFX AutoSave] ✗ Failed to save UI format. Status:", responseUI.status);
                }

            } catch (error) {
                console.error("[OFX AutoSave] ✗ Failed to auto-save:", error);
            } finally {
                saveInProgress = false;
            }
        }

        // Method 1: Hook into app.saveWorkflow (if it exists)
        console.log("[OFX AutoSave] Checking app.saveWorkflow:", typeof app.saveWorkflow);
        if (typeof app.saveWorkflow === 'function') {
            const originalSave = app.saveWorkflow;
            app.saveWorkflow = async function(...args) {
                console.log("[OFX AutoSave] app.saveWorkflow() called");
                const result = await originalSave.apply(this, args);
                await performAutoSave('app.saveWorkflow hook');
                return result;
            };
            console.log("[OFX AutoSave] ✓ Hooked into app.saveWorkflow");
        } else {
            console.warn("[OFX AutoSave] ⚠ app.saveWorkflow not found, will use alternative methods");
        }

        // Method 2: Detect Ctrl+S / Cmd+S keyboard shortcut
        window.addEventListener('keydown', async (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 's') {
                console.log("[OFX AutoSave] Save keyboard shortcut detected");
                e.preventDefault(); // Prevent browser save dialog
                await performAutoSave('keyboard shortcut');
            }
        });
        console.log("[OFX AutoSave] ✓ Keyboard shortcut listener added");

        // Method 3: Periodic auto-save every 30 seconds (if workflow has changed)
        let lastGraphState = null;
        setInterval(async () => {
            if (!originalWorkflowName) return;

            try {
                const currentState = JSON.stringify(app.graph.serialize());
                if (lastGraphState && lastGraphState !== currentState) {
                    console.log("[OFX AutoSave] Graph changed, triggering periodic save");
                    await performAutoSave('periodic check');
                }
                lastGraphState = currentState;
            } catch (error) {
                console.error("[OFX AutoSave] Error in periodic check:", error);
            }
        }, 30000);
        console.log("[OFX AutoSave] ✓ Periodic auto-save enabled (30s interval)");

        console.log("[OFX AutoSave] Extension initialized with multiple save detection methods");
    }
});
