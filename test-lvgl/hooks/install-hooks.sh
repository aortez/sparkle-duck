#!/bin/bash
#
# Install git hooks for sparkle-duck development.
#
# Usage: ./hooks/install-hooks.sh
#

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)

if [ -z "$REPO_ROOT" ]; then
    echo "Error: Not in a git repository"
    exit 1
fi

HOOKS_DIR="$REPO_ROOT/.git/hooks"
TEMPLATE_DIR="$REPO_ROOT/test-lvgl/hooks"

echo "Installing git hooks..."

# Install pre-commit hook.
if [ -f "$HOOKS_DIR/pre-commit" ] && [ ! -L "$HOOKS_DIR/pre-commit" ]; then
    echo "Warning: Existing pre-commit hook found (not a symlink)"
    echo "Backing up to pre-commit.backup"
    mv "$HOOKS_DIR/pre-commit" "$HOOKS_DIR/pre-commit.backup"
fi

ln -sf ../../test-lvgl/hooks/pre-commit "$HOOKS_DIR/pre-commit"
echo "✅ pre-commit hook installed"

echo ""
echo "Git hooks installed successfully!"
echo ""
echo "The pre-commit hook will:"
echo "  1. Format code with clang-format"
echo "  2. Run all tests"
echo ""
echo "To skip tests on a commit: SKIP_TESTS=1 git commit"
