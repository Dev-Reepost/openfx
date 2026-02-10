// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * OFX Auto-Loader Extension for ComfyUI
 *
 * This extension enables automatic workflow loading from URL parameters.
 * When AnyComfy OFX plugin creates a new workflow and opens the browser,
 * this extension automatically loads the workflow into ComfyUI.
 *
 * Installation:
 *   Copy this file to: ComfyUI/web/extensions/ofx_autoloader.js
 *
 * Usage:
 *   Open URL: http://localhost:8188/?load_local_json=workflows/my_workflow/my_workflow.json
 *   The workflow file must exist in ComfyUI input directory under the specified path
 *
 * How it works:
 *   1. Reads 'load_local_json' parameter from URL (supports subdirectory paths)
 *   2. Parses the path into subfolder and filename components
 *   3. Fetches the workflow from /view endpoint with subfolder parameter
 *   4. Loads the workflow into ComfyUI using app.loadGraphData()
 *   5. Cleans up the URL parameter to prevent re-loading on refresh
 *
 * Compatibility:
 *   - Uses dynamic import() for broad ComfyUI version compatibility
 *   - Works with ancient and modern ComfyUI versions
 *   - Gracefully falls back to alert() if toast notifications unavailable
 *   - For very old ComfyUI versions, use ofx_autoloader_nomodule.js instead
 */

console.log("[OFX AutoLoader] Loading extension...");

(function() {
    // Import app using dynamic import for compatibility
    import("../../scripts/app.js").then(module => {
        console.log("[OFX AutoLoader] Successfully imported app.js");
        const app = module.app;

        if (!app) {
            console.error("[OFX AutoLoader] ERROR: app object not found in module");
            return;
        }

        app.registerExtension({
            name: "OFX.AutoLoader",

            async setup() {
                console.log("[OFX AutoLoader] Extension setup started");

                // Check URL parameters
                const params = new URLSearchParams(window.location.search);
                const fileName = params.get('load_local_json');

                if (!fileName) {
                    console.log("[OFX AutoLoader] No workflow to auto-load");
                    return;
                }

                console.log("[OFX AutoLoader] Auto-loading workflow:", fileName);

                // CRITICAL: Wait for all extensions and custom nodes to be registered
                // Custom nodes register during extension setup, so we need to wait for
                // all extensions to finish before loading the workflow
                console.log("[OFX AutoLoader] Waiting for custom nodes to be registered...");
                await new Promise(resolve => setTimeout(resolve, 2000));
                console.log("[OFX AutoLoader] Proceeding with workflow load");

                try {
                    // Parse path into subfolder and filename
                    // e.g., "workflows/my_workflow/my_workflow.json" -> subfolder="workflows/my_workflow", filename="my_workflow.json"
                    let subfolder = "";
                    let actualFileName = fileName;

                    const lastSlash = fileName.lastIndexOf('/');
                    if (lastSlash !== -1) {
                        subfolder = fileName.substring(0, lastSlash);
                        actualFileName = fileName.substring(lastSlash + 1);
                    }

                    console.log("[OFX AutoLoader] Subfolder:", subfolder || "(root)");
                    console.log("[OFX AutoLoader] Filename:", actualFileName);

                    // Fetch workflow from input directory with subfolder support
                    // Add cache-busting timestamp to prevent loading stale cached content
                    const cacheBuster = Date.now();
                    let url = `/view?filename=${encodeURIComponent(actualFileName)}&type=input&t=${cacheBuster}`;
                    if (subfolder) {
                        url += `&subfolder=${encodeURIComponent(subfolder)}`;
                    }
                    console.log("[OFX AutoLoader] Fetching from:", url);

                    const response = await fetch(url);

                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }

                    const workflowJson = await response.json();
                    console.log("[OFX AutoLoader] Workflow JSON structure:", Object.keys(workflowJson));

                    // Detect format: UI format has "nodes" array, API format has node IDs as keys
                    const isUIFormat = workflowJson.nodes && Array.isArray(workflowJson.nodes);
                    console.log("[OFX AutoLoader] Detected format:", isUIFormat ? "UI" : "API");

                    // Load workflow into ComfyUI using appropriate method
                    let loadSuccess = false;

                    if (isUIFormat) {
                        // UI format: use loadGraphData
                        if (typeof app.loadGraphData === 'function') {
                            console.log("[OFX AutoLoader] Loading UI format via app.loadGraphData()...");
                            await app.loadGraphData(workflowJson);
                            console.log("[OFX AutoLoader] ✓ Loaded via loadGraphData()");
                            loadSuccess = true;
                        } else {
                            throw new Error("loadGraphData() not available for UI format workflow");
                        }
                    } else {
                        // API format: try loadApiJson first, fall back to loadGraphData
                        if (typeof app.loadApiJson === 'function') {
                            try {
                                console.log("[OFX AutoLoader] Loading API format via app.loadApiJson()...");
                                await app.loadApiJson(workflowJson);
                                console.log("[OFX AutoLoader] ✓ Loaded via loadApiJson()");
                                loadSuccess = true;
                            } catch (apiError) {
                                console.warn("[OFX AutoLoader] loadApiJson() failed:", apiError.message);
                                console.log("[OFX AutoLoader] Falling back to loadGraphData()...");
                            }
                        }

                        // Fallback for ancient ComfyUI with broken loadApiJson
                        if (!loadSuccess && typeof app.loadGraphData === 'function') {
                            console.log("[OFX AutoLoader] Using app.loadGraphData() for API format");
                            await app.loadGraphData(workflowJson);
                            console.log("[OFX AutoLoader] ✓ Loaded via loadGraphData()");
                            loadSuccess = true;
                        }
                    }

                    if (!loadSuccess) {
                        throw new Error("No workflow loading method available");
                    }

                    console.log("[OFX AutoLoader] ✓ Workflow loaded successfully:", fileName);

                    // Publish the path for OFX.AutoSave *before* the URL is cleared.
                    // ComfyUI calls extension setup() sequentially in name order
                    // ("OFX.AutoLoader" < "OFX.AutoSave"), so AutoSave's setup()
                    // has not run yet and the URL param is the only source of truth
                    // at this point.  Write the shared property and fire the event;
                    // AutoSave will pick up whichever it sees first.
                    window.__ofxWorkflowPath = fileName;
                    window.dispatchEvent(new CustomEvent('ofx:workflow-loaded', { detail: { path: fileName } }));
                    console.log("[OFX AutoLoader] Published workflow path for AutoSave:", fileName);

                    // Clean up URL parameter (safe now – path is published above)
                    window.history.replaceState({}, document.title, "/");

                    // Success notification removed - check console logs instead
                    console.log("[OFX AutoLoader] ✓ Success notification suppressed (modal popup disabled)");

                } catch (error) {
                    console.error("[OFX AutoLoader] ✗ Error loading workflow:", error);
                    console.error("[OFX AutoLoader] Failed to auto-load:", fileName);
                    console.error("[OFX AutoLoader] Please load manually (Ctrl+O or drag-and-drop)");

                    // Error notification removed - check console logs instead
                    console.error("[OFX AutoLoader] ✗ Error notification suppressed (modal popup disabled)");

                    // Still clean up URL even on error
                    window.history.replaceState({}, document.title, "/");
                }
            }
        });

        console.log("[OFX AutoLoader] Extension registered successfully");

    }).catch(error => {
        console.error("[OFX AutoLoader] ERROR: Failed to import app.js:", error);
        console.error("[OFX AutoLoader] This extension requires ComfyUI with ES modules support");
        console.error("[OFX AutoLoader] For older ComfyUI versions, use ofx_autoloader_nomodule.js instead");
    });

})();

console.log("[OFX AutoLoader] Extension file loaded");
