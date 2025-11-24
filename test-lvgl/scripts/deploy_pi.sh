#!/bin/bash

# Raspberry Pi deployment automation script
# This script automates building and deploying the project to a Raspberry Pi

set -e # Exit on any error

# Configuration
PI_HOST="raspberrypi.local"
PI_USER="pi"
PROJECT_DIR="/home/pi/sparkle-duck"
BUILD_DIR="build-pi"
SSH_KEY="~/.ssh/id_rsa"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Starting Raspberry Pi deployment automation...${NC}"

# Function to print colored output
print_status() {
    echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

# Function to print success
print_success() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

# Function to print error
print_error() {
    echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')] ERROR: $1${NC}"
}

# Check if we're on a supported platform
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    print_error "This script is designed for Linux systems"
    exit 1
fi

# Check if SSH key exists
if [[ ! -f "$SSH_KEY" ]]; then
    print_error "SSH key not found at $SSH_KEY"
    exit 1
fi

# Check if rsync is installed
if ! command -v rsync &> /dev/null; then
    print_error "rsync is not installed"
    exit 1
fi

# Check if ssh is installed
if ! command -v ssh &> /dev/null; then
    print_error "ssh is not installed"
    exit 1
fi

# Create build directory
print_status "Creating build directory..."
mkdir -p "$BUILD_DIR"

# Check if we can connect to the Raspberry Pi
print_status "Testing connection to Raspberry Pi..."
if ! ssh -i "$SSH_KEY" -o ConnectTimeout=10 "$PI_USER@$PI_HOST" "exit" 2>/dev/null; then
    print_error "Cannot connect to Raspberry Pi at $PI_HOST"
    exit 1
fi

# Create project directory on Raspberry Pi
print_status "Creating project directory on Raspberry Pi..."
ssh -i "$SSH_KEY" "$PI_USER@$PI_HOST" "mkdir -p $PROJECT_DIR"

# Sync source code to Raspberry Pi
print_status "Syncing source code to Raspberry Pi..."
rsync -avz --delete \
    --exclude='build*' \
    --exclude='.git' \
    --exclude='*.swp' \
    --exclude='*.swo' \
    --exclude='*.o' \
    --exclude='*.a' \
    --exclude='*.so' \
    --exclude='*.out' \
    --exclude='*.log' \
    --exclude='*.tmp' \
    --exclude='*.cache' \
    --exclude='*.build' \
    --exclude='*.cmake' \
    --exclude='CMakeCache.txt' \
    --exclude='CMakeFiles' \
    --exclude='*.d' \
    --exclude='*.pdb' \
    --exclude='*.ilk' \
    --exclude='*.exp' \
    --exclude='*.lib' \
    --exclude='*.dll' \
    --exclude='*.exe' \
    --exclude='*.bin' \
    --exclude='*.dat' \
    --exclude='*.db' \
    --exclude='*.lock' \
    --exclude='*.pid' \
    --exclude='*.core' \
    --exclude='*.dump' \
    --exclude='*.crash' \
    --exclude='*.back' \
    --exclude='*.bak' \
    --exclude='*.old' \
    --exclude='*.new' \
    --exclude='*.save' \
    --exclude='*.orig' \
    --exclude='*.rej' \
    --exclude='*.patch' \
    --exclude='*.diff' \
    --exclude='*.changes' \
    --exclude='*.todo' \
    --exclude='*.todo.*' \
    --exclude='*.todo.*.*' \
    --exclude='*.todo.*.*.*' \
    --exclude='*.todo.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*' \
    --exclude='*.todo.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*