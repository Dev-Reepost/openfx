# comfyui-ofx-autoload

A tiny, web-only ComfyUI custom node that ships two browser extensions used by
the **AIFX AnyComfy** OpenFX plugin:

- **`ofx_autoloader.js`** — when AnyComfy's *New Workflow* / *Open Workflow*
  button opens ComfyUI with a `?load_local_json=<path>` URL, load that workflow
  into the graph.
- **`ofx_autosaver.js`** — save edits back to the same file the plugin opened.

There are **no Python nodes** — the node exists only to expose the JS to the
frontend via `WEB_DIRECTORY`.

## Why this exists

Modern ComfyUI (frontend ≥ ~1.4x) no longer serves legacy drop-in
`ComfyUI/web/extensions/*.js` files. Web assets must be exposed by a **custom
node** through `WEB_DIRECTORY`; ComfyUI then serves and auto-loads every `.js`
under it at `/extensions/comfyui-ofx-autoload/<file>`. Without this node the
AnyComfy "Open Workflow" button opens ComfyUI's default workflow instead of the
selected one.

## Install

Pick one:

- **ComfyUI-Manager** → *Install via Git URL* → this repository, then restart.
- **comfy-cli**: `comfy node registry-install comfyui-ofx-autoload` (once
  published), or `comfy node install <git-url>`.
- **Manual**: copy the `comfyui-ofx-autoload/` folder into
  `ComfyUI/custom_nodes/` and restart ComfyUI.

## Verify

After restarting ComfyUI:

```bash
curl -o /dev/null -w "%{http_code}\n" \
  http://<server>:<port>/extensions/comfyui-ofx-autoload/ofx_autoloader.js
```

Expect **200**. In the browser console you should see
`[OFX AutoLoader] Loading extension...` when the page loads, and
`[OFX AutoLoader] ✓ Workflow loaded successfully` when opened via the plugin.

## Requirements

- ComfyUI launched with `--input-directory` pointing at the same shared folder
  the AnyComfy plugin uses (workflows live under `<input>/workflows/…`), so the
  autoloader's `/view?type=input` fetch resolves.

## License

BSD-3-Clause. See [LICENSE](LICENSE).
