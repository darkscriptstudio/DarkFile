#!/bin/bash

# DARK FILE TOOL INSTALLER
# Works on: Linux, Android (Termux), macOS, Windows (WSL/Git Bash)

SOURCE_FILE="src/darkfile.c"
OUTPUT_BIN="darkfile"
INSTALL_DIR=""
SUDO_CMD=""

# --- 1. Detect Environment ---
echo "[-] Detecting Operating System..."

if [ -n "$TERMUX_VERSION" ]; then
    echo "    > Detected: Android (Termux)"
    INSTALL_DIR="$PREFIX/bin"
    SUDO_CMD="" # Termux is single-user, no sudo needed
elif [ "$(uname)" == "Darwin" ]; then
    echo "    > Detected: macOS"
    INSTALL_DIR="/usr/local/bin"
    SUDO_CMD="sudo"
elif [[ "$(uname)" == MINGW* ]] || [[ "$(uname)" == CYGWIN* ]]; then
    echo "    > Detected: Windows (Git Bash/MinGW)"
    INSTALL_DIR="/usr/bin" 
    OUTPUT_BIN="darkfile.exe"
    SUDO_CMD=""
else
    echo "    > Detected: Linux"
    INSTALL_DIR="/usr/local/bin"
    SUDO_CMD="sudo"
fi

# --- 2. Check Dependencies ---
echo "[-] Checking dependencies..."
if ! command -v gcc &> /dev/null; then
    echo "[!] Error: GCC compiler not found."
    if [ -n "$TERMUX_VERSION" ]; then
        echo "    Run: pkg install clang"
    else
        echo "    Please install gcc (e.g., sudo apt install gcc)"
    fi
    exit 1
fi

# --- 3. Compile ---
echo "[-] Compiling $SOURCE_FILE..."
if [ -f "$SOURCE_FILE" ]; then
    # Compile with optimization (-O3) and threading support (-pthread)
    gcc "$SOURCE_FILE" -o "$OUTPUT_BIN" -O3 -pthread
    
    if [ $? -eq 0 ]; then
        echo "    > Compilation Successful!"
    else
        echo "[!] Compilation Failed."
        exit 1
    fi
else
    echo "[!] Error: Source file $SOURCE_FILE not found!"
    exit 1
fi

# --- 4. Install ---
echo "[-] Installing to $INSTALL_DIR..."

# Move binary to path (using sudo if required)
if [ -n "$SUDO_CMD" ]; then
    $SUDO_CMD mv "$OUTPUT_BIN" "$INSTALL_DIR/$OUTPUT_BIN"
else
    mv "$OUTPUT_BIN" "$INSTALL_DIR/$OUTPUT_BIN"
fi

# Set permissions
if [ -n "$SUDO_CMD" ]; then
    $SUDO_CMD chmod +x "$INSTALL_DIR/$OUTPUT_BIN"
else
    chmod +x "$INSTALL_DIR/$OUTPUT_BIN"
fi

echo "------------------------------------------------"
echo " ✅ INSTALLATION COMPLETE"
echo "------------------------------------------------"
echo " You can now use the tool by typing:"
echo " darkfile -h"
echo "------------------------------------------------"
