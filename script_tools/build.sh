#!/bin/bash
# build.sh - Build TUI Metadata MP3 Editor

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

log()  { echo -e "${GREEN}[OK]${NC} $1"; }
err()  { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

cd "$(dirname "$0")/.."

# Check FTXUI is installed
if [ ! -d "/tmp/ftxui-install/usr/local/include/ftxui" ]; then
    err "FTXUI not found. Run: ./script_tools/install_deps.sh"
fi

echo "Building TUI Metadata MP3 Editor..."
make clean && make

if [ $? -eq 0 ]; then
    log "Build successful: bin/MetadataMP3"
    echo ""
    echo "Run: ./bin/MetadataMP3 [directory]"
else
    err "Build failed"
fi
