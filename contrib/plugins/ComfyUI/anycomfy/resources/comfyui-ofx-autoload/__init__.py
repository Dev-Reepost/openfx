# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, the AIFX contributors.
#
# comfyui-ofx-autoload — a web-only ComfyUI custom node.
#
# It ships two browser extensions used by the AIFX "AnyComfy" OpenFX plugin:
#   - ofx_autoloader.js : when the plugin opens ComfyUI with a
#       ?load_local_json=<path> URL, load that workflow into the graph.
#   - ofx_autosaver.js  : save edits back to the same file the plugin opened.
#
# Modern ComfyUI (frontend >= ~1.4x) no longer serves legacy drop-in
# ComfyUI/web/extensions/*.js. Web assets must be exposed by a custom node via
# WEB_DIRECTORY; ComfyUI then serves + auto-loads every .js under it at
#   /extensions/comfyui-ofx-autoload/<file>
#
# There are no Python nodes — the mappings are intentionally empty.

WEB_DIRECTORY = "./js"
NODE_CLASS_MAPPINGS = {}
NODE_DISPLAY_NAME_MAPPINGS = {}

__all__ = ["NODE_CLASS_MAPPINGS", "NODE_DISPLAY_NAME_MAPPINGS", "WEB_DIRECTORY"]
