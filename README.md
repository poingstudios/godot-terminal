<div align="center">
  <h1>
    🖥️ Godot Terminal
  </h1>

  [![Godot Engine](https://img.shields.io/badge/Godot-4.x-478CBF?style=for-the-badge&logo=godotengine&logoColor=white)](https://godotengine.org)
  [![License](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](LICENSE)
  [![Platform](https://img.shields.io/badge/Platform-macOS%20|%20Windows%20|%20Linux-lightgrey?style=for-the-badge)](https://github.com/poingstudios/godot-terminal)
  [![Discord](https://img.shields.io/badge/Discord-Poing_Studios-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.com/invite/YEPvYjSSMk)

  **The ultimate built-in terminal shell for Godot 4.**  
  Bring the full power of your native system shell directly into the Godot editor bottom panel.

  ---

  [✨ Features](#-features) • [📦 Installation](#-installation) • [⌨️ Shortcuts](#️-shortcuts) • [🛠️ Building from Source](#️-building-from-source) • [🙏 Support](#-support)
</div>

---

## ✨ Features

- **🚀 Native PTY Execution**: Powered by a high-performance C++ GDExtension with POSIX `openpty` (macOS/Linux) and Windows ConPTY support.
- **🎨 True ANSI Terminal Emulation**: Integrated with `libvterm` supporting standard ANSI colors, 24-bit RGB colors, bold/underline text, alternate screen buffers (`vim`, `htop`, `top`), and interactive TUI apps (e.g. `bubbletea`, `lipgloss`, `fzf`, `agy`).
- **🔗 Smart Clickable Links**: Automatically detects file paths and URLs in output; click to open URLs in your browser or jump straight to scripts and line numbers in the Godot script editor.
- **📑 Multi-Tab & Splits**: Open multiple shells (`zsh`, `bash`, `fish`, `powershell`, `cmd`), split horizontally or vertically, and rename tabs dynamically.
- **⚡ Hotkey Toggle**: Open and collapse the terminal dock instantly from anywhere in the editor with <kbd>Ctrl</kbd> + <kbd>\`</kbd> (or <kbd>⌘</kbd> + <kbd>\`</kbd> on macOS).
- **⚙️ Configurable**: Customize font size, scrollback buffer limit, default shell, and shortcuts via the built-in settings panel.

---

## 📦 Installation

### Manual Installation

1. Download the latest release from the [Releases](https://github.com/poingstudios/godot-terminal/releases) page.
2. Copy the `addons/godot_terminal` folder into your Godot project's `addons/` directory.
3. In Godot, go to **Project -> Project Settings -> Plugins** and enable **Godot Terminal**.
4. The **Terminal** tab will appear in your editor's bottom panel.

---

## ⌨️ Shortcuts

| Action | Shortcut | Description |
| :--- | :--- | :--- |
| **Toggle Terminal** | <kbd>Ctrl</kbd> + <kbd>\`</kbd> / <kbd>⌘</kbd> + <kbd>\`</kbd> | Toggle the terminal bottom dock open/collapsed |
| **Copy Selection** | <kbd>Ctrl</kbd> + <kbd>C</kbd> / <kbd>⌘</kbd> + <kbd>C</kbd> | Copy selected text to clipboard (when text is selected) |
| **Paste** | <kbd>Ctrl</kbd> + <kbd>V</kbd> / <kbd>⌘</kbd> + <kbd>V</kbd> | Paste clipboard text into the terminal |
| **Clear Buffer** | <kbd>Ctrl</kbd> + <kbd>L</kbd> / <kbd>⌘</kbd> + <kbd>L</kbd> | Clear scrollback buffer and screen |
| **Interrupt / SIGINT** | <kbd>Ctrl</kbd> + <kbd>C</kbd> | Send interrupt signal to active process (when no text is selected) |

---

## 🛠️ Building from Source

### Prerequisites

- [Godot 4.x](https://godotengine.org/download)
- C++17 compiler (GCC, Clang, or MSVC)
- [SCons](https://scons.org/) build tool
- Python 3.x

### Build Instructions

1. Clone repository with submodules:
   ```bash
   git clone --recursive https://github.com/poingstudios/godot-terminal.git
   cd godot-terminal
   ```

2. Build GDExtension for your platform using the build script:
   ```bash
   ./scripts/build_local.sh
   ```

   Or build directly with SCons:
   ```bash
   # macOS
   scons -C platforms/gdextension platform=macos target=template_debug arch=x86_64

   # Linux
   scons -C platforms/gdextension platform=linux target=template_debug arch=x86_64

   # Windows
   scons -C platforms/gdextension platform=windows target=template_debug arch=x86_64
   ```

3. Open the project in the Godot editor:
   ```bash
   godot -e
   ```

---

## 🙏 Support

If you find this plugin helpful, consider supporting the project:

- ⭐ **Star this repository** on GitHub!
- 💬 Join our [Discord Community](https://discord.gg/fhbyqgm7ky) for help and discussion.
- 💖 Sponsor us on [Patreon](https://www.patreon.com/poingstudios) or [Ko-fi](https://ko-fi.com/poingstudios).

---

<div align="center">
  <sub>MIT License © 2026 Poing Studios</sub>
</div>
