# Godot Terminal — AGENTS.md

AI assistant context for the godot-terminal repository. Read this before making changes.

---

## Project Overview

**Godot Terminal** is a built-in terminal shell plugin for the Godot 4 Editor, developed by Poing Studios. It embeds a native terminal dock in the Godot bottom panel, allowing developers to run shell commands, interact with CLI tools, and click output links to navigate directly to scripts in the editor.

- **Engine:** Godot 4.x
- **Primary Languages:** C++17 (GDExtension backend) + GDScript (Editor UI & Session Management)
- **C/C++ Dependencies:** `godot-cpp` (submodule), `libvterm` (ANSI/VT100 screen buffer emulation)
- **Supported OS Platforms:** macOS (POSIX `openpty`), Linux (POSIX `openpty`), Windows (ConPTY)

---

## Architecture & Directory Layout

The repository follows a multi-platform / polyglot target structure under `platforms/`:

```
godot-terminal/
├── platforms/
│   ├── gdextension/         # C++ GDExtension source & build system
│   │   ├── godot-cpp/       # Godot C++ bindings submodule
│   │   ├── src/             # C++ source files (PTY drivers, emulator, bindings)
│   │   ├── thirdparty/      # libvterm C terminal emulation library
│   │   └── SConstruct       # SCons build configuration
│   └── godot_editor/        # Godot Editor test/sample project & addon folder
│       └── addons/
│           └── godot_terminal/
│               ├── bin/     # Compiled native shared libraries (.dylib, .so, .dll)
│               ├── icons/   # UI icons
│               ├── internal/# Internal GDScript implementation (no class_name)
│               │   ├── core/    # Config & session manager
│               │   ├── parser/  # ANSI link and path parsing
│               │   └── ui/      # UI controls, dock view, tabs, dialogs
│               ├── godot_terminal.gdextension # GDExtension manifest
│               ├── plugin.cfg                 # Editor plugin metadata
│               └── plugin.gd                  # Editor plugin entry point
└── scripts/
    └── build_local.sh       # Local build automation script
```

---

## Key Files & Responsibilities

| File / Folder | Purpose |
| :--- | :--- |
| `platforms/gdextension/src/terminal_pty.cpp` | Native PTY process lifecycle, I/O channels, resize, and signal handlers |
| `platforms/gdextension/src/terminal_emulator.cpp` | Bridges `libvterm` screen buffer, color palettes, cell grid state to Godot |
| `platforms/gdextension/src/pty/` | OS-specific PTY drivers (`pty_driver_posix.cpp` & `pty_driver_windows.cpp`) |
| `platforms/godot_editor/addons/godot_terminal/plugin.gd` | Main `EditorPlugin` registering the bottom panel dock and global shortcuts |
| `platforms/godot_editor/addons/godot_terminal/internal/` | Plugin logic and UI components (must use `preload()`, no `class_name`) |
| `platforms/gdextension/SConstruct` | SCons build recipe compiling binaries to `addons/godot_terminal/bin/` |
| `scripts/build_local.sh` | SCons wrapper script detecting host OS/arch and building the extension |

---

## Build & Test Commands

### Prerequisites

- **Godot 4.x** (editor executable)
- **C++17 Compiler** (Clang on macOS, GCC/Clang on Linux, MSVC on Windows)
- **SCons** (`pip install scons`)
- **Python 3.x**

### Compile GDExtension

Using the build script:
```bash
./scripts/build_local.sh [platform] [target] [arch]
```
*Examples:*
```bash
./scripts/build_local.sh                             # Auto-detect host OS and architecture
./scripts/build_local.sh macos template_debug arm64 # Explicit macOS arm64 debug build
./scripts/build_local.sh windows template_release x86_64
```

Using SCons directly:
```bash
scons -C platforms/gdextension platform=macos target=template_debug arch=arm64
scons -C platforms/gdextension platform=linux target=template_debug arch=x86_64
scons -C platforms/gdextension platform=windows target=template_debug arch=x86_64
```

### Running the Godot Editor

```bash
godot --path platforms/godot_editor -e
```

---

## Coding Rules & Guidelines

### GDScript Rules

1. **Type Inference**: Always use `:=` instead of `=` for variable assignments (e.g. `var panel := BottomPanel.new()`).
2. **`internal/` Encapsulation**: The `internal/` directory inside `addons/godot_terminal/` **must not** contain any script with `class_name`. All internal scripts must be loaded explicitly using `preload("res://addons/godot_terminal/internal/...")`.
3. **Tabs Indentation**: Use tabs for indentation, not spaces.
4. **UI Construction**: Keep UI components modular with clean separation between session state, ANSI parser, and Godot controls.

### Comments & Documentation

- Only add comments if the code logic is complex or specifically requested.
- Do not add boilerplate, redundant, or self-explanatory comments.

### GitHub & Git Workflow

- Always use the `gh` CLI for GitHub interactions (pull requests, issues, releases, browsing).
- Do not make git commits directly unless explicitly instructed.
- All code changes and responses must be in English.

---

## GDScript & C++ Naming Conventions

| Type | Convention | Example |
| :--- | :--- | :--- |
| Folders / Files | `snake_case` | `terminal_view.gd`, `terminal_pty.cpp` |
| Classes / Structs | `PascalCase` | `TerminalPty`, `TerminalEmulator` |
| Variables / Functions | `snake_case` | `active_session`, `create_session()` |
| Private Members | `_snake_case` | `_on_text_submitted()`, `_vterm` |
| Constants / Enums | `SCREAMING_SNAKE_CASE` | `DEFAULT_FONT_SIZE`, `KEY_QUOTELEFT` |
| Signals | `snake_case` | `data_received`, `session_closed` |
