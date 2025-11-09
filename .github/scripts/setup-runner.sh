#!/bin/bash
#
# Self-hosted GitHub Actions runner setup script.
# This script installs and configures a GitHub Actions runner for the sparkle-duck repository.
#
# Usage:
#   1. Copy .env.example to .env and fill in your values:
#      cp .github/.env.example .github/.env
#   2. Run the setup script:
#      ./.github/scripts/setup-runner.sh
#
# Alternatively, pass token as argument (legacy):
#   ./.github/scripts/setup-runner.sh <github-token>
#
# The GitHub token should have 'repo' scope and can be generated at:
#   https://github.com/settings/tokens
#

set -e

# Color output.
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Determine script directory and repo root.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GITHUB_DIR="$REPO_ROOT/.github"

# Load .env file if it exists.
if [ -f "$GITHUB_DIR/.env" ]; then
    log_info "Loading configuration from .env file..."
    # Export variables from .env, ignoring comments and empty lines.
    set -a
    source "$GITHUB_DIR/.env"
    set +a
elif [ $# -lt 1 ]; then
    log_error "No .env file found and no token provided."
    echo ""
    echo "Please either:"
    echo "  1. Create .env file:"
    echo "     cp .github/.env.example .github/.env"
    echo "     # Edit .github/.env and add your GitHub token"
    echo "     ./.github/scripts/setup-runner.sh"
    echo ""
    echo "  2. Pass token as argument:"
    echo "     ./.github/scripts/setup-runner.sh ghp_xxxxxxxxxxxxx"
    echo ""
    echo "Get a token at: https://github.com/settings/tokens"
    exit 1
fi

# Allow command-line arguments to override .env values.
if [ $# -ge 1 ]; then
    GITHUB_TOKEN="$1"
fi
if [ $# -ge 2 ]; then
    REPO_OWNER="$2"
fi
if [ $# -ge 3 ]; then
    REPO_NAME="$3"
fi

# Validate GitHub token.
if [ -z "$GITHUB_TOKEN" ]; then
    log_error "GITHUB_TOKEN is not set."
    echo "Add it to .env or pass as first argument."
    exit 1
fi

# Configuration.
RUNNER_VERSION="2.321.0"
RUNNER_NAME="${RUNNER_NAME:-sparkle-duck-runner-$(hostname)}"
RUNNER_WORK_DIR="${RUNNER_WORK_DIR:-_work}"
RUNNER_LABELS="${RUNNER_LABELS:-self-hosted,linux,x64}"

# Auto-detect repo owner/name if not provided.
if [ -z "$REPO_OWNER" ] || [ -z "$REPO_NAME" ]; then
    GIT_URL=$(git -C "$REPO_ROOT" config --get remote.origin.url 2>/dev/null || echo "")

    if [ -n "$GIT_URL" ]; then
        # Handle SSH format: git@github.com:owner/repo.git
        if [[ "$GIT_URL" =~ :([^/]+)/([^/]+)\.git$ ]]; then
            REPO_OWNER="${REPO_OWNER:-${BASH_REMATCH[1]}}"
            REPO_NAME="${REPO_NAME:-${BASH_REMATCH[2]}}"
        # Handle HTTPS format: https://github.com/owner/repo.git
        elif [[ "$GIT_URL" =~ /([^/]+)/([^/]+)\.git$ ]]; then
            REPO_OWNER="${REPO_OWNER:-${BASH_REMATCH[1]}}"
            REPO_NAME="${REPO_NAME:-${BASH_REMATCH[2]}}"
        fi
    fi
fi

if [ -z "$REPO_OWNER" ] || [ -z "$REPO_NAME" ]; then
    log_error "Could not determine repository owner/name from git config."
    echo "Please provide them explicitly:"
    echo "  $0 <token> <owner> <repo-name>"
    exit 1
fi

log_info "Setting up GitHub Actions runner for ${REPO_OWNER}/${REPO_NAME}"

# Install dependencies.
log_info "Installing dependencies..."
sudo apt-get update
sudo apt-get install -y \
    curl \
    jq \
    build-essential \
    cmake \
    pkg-config \
    libboost-dev \
    clang-format

# Create runner directory.
RUNNER_DIR="$HOME/actions-runner"
mkdir -p "$RUNNER_DIR"
cd "$RUNNER_DIR"

# Download and extract runner.
log_info "Downloading GitHub Actions runner v${RUNNER_VERSION}..."
curl -o actions-runner-linux-x64.tar.gz -L "https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz"

log_info "Extracting runner..."
tar xzf actions-runner-linux-x64.tar.gz
rm actions-runner-linux-x64.tar.gz

# Get registration token from GitHub API.
log_info "Fetching registration token from GitHub..."
REGISTRATION_TOKEN=$(curl -s -X POST \
    -H "Authorization: token ${GITHUB_TOKEN}" \
    -H "Accept: application/vnd.github.v3+json" \
    "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/actions/runners/registration-token" \
    | jq -r '.token')

if [ -z "$REGISTRATION_TOKEN" ] || [ "$REGISTRATION_TOKEN" = "null" ]; then
    log_error "Failed to get registration token. Check your GitHub token and permissions."
    exit 1
fi

# Configure runner.
log_info "Configuring runner..."
./config.sh \
    --url "https://github.com/${REPO_OWNER}/${REPO_NAME}" \
    --token "${REGISTRATION_TOKEN}" \
    --name "${RUNNER_NAME}" \
    --work "${RUNNER_WORK_DIR}" \
    --labels "${RUNNER_LABELS}" \
    --unattended

# Install and start service.
log_info "Installing runner service..."
sudo ./svc.sh install

log_info "Starting runner service..."
sudo ./svc.sh start

log_info "Runner setup complete!"
log_info "Runner name: ${RUNNER_NAME}"
log_info "Runner labels: ${RUNNER_LABELS}"
log_info ""
log_info "To check status: sudo ./svc.sh status"
log_info "To stop: sudo ./svc.sh stop"
log_info "To uninstall: sudo ./svc.sh uninstall"
