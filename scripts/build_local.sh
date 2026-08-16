#!/bin/bash
# MIT License
#
# Copyright (c) 2026 Poing Studios

# scripts/build_local.sh

show_help() {
    echo "Usage: ./scripts/build_local.sh [platform] [target] [arch]"
    echo ""
    echo "Arguments:"
    echo "  [platform]  macos, linux, or windows (default: auto-detected)"
    echo "  [target]    template_debug or template_release (default: template_debug)"
    echo "  [arch]      x86_64 or arm64 (default: auto-detected)"
    echo ""
    echo "Examples:"
    echo "  ./scripts/build_local.sh"
    echo "  ./scripts/build_local.sh macos template_debug x86_64"
    echo "  ./scripts/build_local.sh macos template_release arm64"
}

if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    show_help
    exit 0
fi

# Detect platform
if [ -z "$1" ]; then
    case "$(uname -s)" in
        Darwin*) PLATFORM="macos" ;;
        Linux*)  PLATFORM="linux" ;;
        CYGWIN*|MINGW*|MSYS*) PLATFORM="windows" ;;
        *) PLATFORM="macos" ;;
    esac
else
    PLATFORM="$1"
fi

TARGET=${2:-template_debug}

# Detect arch
if [ -z "$3" ]; then
    case "$(uname -m)" in
        x86_64|amd64) ARCH="x86_64" ;;
        arm64|aarch64) ARCH="arm64" ;;
        *) ARCH="x86_64" ;;
    esac
else
    ARCH="$3"
fi

NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ">>> Building Godot Terminal ($PLATFORM, $TARGET, $ARCH) with $NPROC jobs..."
scons -C platforms/gdextension platform="$PLATFORM" target="$TARGET" arch="$ARCH" -j"$NPROC"

if [ $? -eq 0 ]; then
    echo ">>> Build completed successfully! Binaries updated in platforms/godot_editor/addons/godot_terminal/bin"
else
    echo ">>> Build failed!"
    exit 1
fi
