#!/bin/bash
# install_deps.sh - Install dependencies for TUI Metadata MP3 Editor
# Supports: Debian/Ubuntu, Raspberry Pi OS, Arch, Fedora

set -e

FTXUI_DIR="${FTXUI_DIR:-/tmp/FTXUI}"
FTXUI_INSTALL="${FTXUI_INSTALL:-/tmp/ftxui-install}"
FTXUI_VERSION="v7.0.0"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()  { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Detect package manager
install_pkg() {
    if command -v apt-get &>/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq "$@"
    elif command -v pacman &>/dev/null; then
        sudo pacman -S --noconfirm "$@"
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y "$@"
    elif command -v brew &>/dev/null; then
        brew install "$@"
    else
        err "No supported package manager found. Install manually: g++ cmake git"
    fi
}

# Check g++
check_compiler() {
    if command -v g++ &>/dev/null; then
        local ver=$(g++ --version | head -1)
        log "Compiler found: $ver"
    else
        warn "g++ not found, installing..."
        if command -v apt-get &>/dev/null; then
            sudo apt-get install -y -qq g++
        elif command -v pacman &>/dev/null; then
            sudo pacman -S --noconfirm gcc
        fi
    fi
}

# Install build tools
install_build_tools() {
    log "Installing build tools..."
    if command -v apt-get &>/dev/null; then
        install_pkg g++ cmake git
    elif command -v pacman &>/dev/null; then
        install_pkg gcc cmake git
    else
        install_pkg g++ cmake git
    fi
}

# Install FTXUI from source
install_ftxui() {
    if [ -d "$FTXUI_INSTALL/usr/local/include/ftxui" ]; then
        log "FTXUI already installed at $FTXUI_INSTALL"
        return
    fi

    log "Installing FTXUI $FTXUI_VERSION..."
    if [ ! -d "$FTXUI_DIR" ]; then
        git clone --depth 1 --branch "$FTXUI_VERSION" https://github.com/ArthurSonzogni/FTXUI.git "$FTXUI_DIR"
    fi

    mkdir -p "$FTXUI_DIR/build"
    cd "$FTXUI_DIR/build"
    cmake .. \
        -DFTXUI_ENABLE_EXAMPLES=OFF \
        -DFTXUI_ENABLE_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX="$FTXUI_INSTALL/usr/local" \
        -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    make install
    cd -
    log "FTXUI installed to $FTXUI_INSTALL"
}

# Install chafa (optional, for album art display)
install_chafa() {
    if command -v chafa &>/dev/null; then
        log "chafa already installed"
        return
    fi

    log "Installing chafa (optional, for album art)..."
    if command -v apt-get &>/dev/null; then
        install_pkg chafa
    elif command -v pacman &>/dev/null; then
        install_pkg chafa
    else
        warn "chafa not available in package manager, skipping"
        warn "Install manually: https://github.com/hpjansson/chafa"
    fi
}

echo "=========================================="
echo "  TUI Metadata MP3 - Dependency Installer"
echo "=========================================="
echo ""

install_build_tools
check_compiler
install_ftxui
install_chafa

echo ""
log "All dependencies installed!"
echo ""
echo "Build the project with:"
echo "  make clean && make"
echo ""
echo "Run with:"
echo "  ./bin/MetadataMP3 [directory]"
echo ""
