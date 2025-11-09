#!/bin/bash
# Merge specified branch into current branch with proper error handling.

set -euo pipefail

# Accept merge branch as parameter, default to main.
MERGE_BRANCH="${1:-main}"

echo "=== Merging ${MERGE_BRANCH} into current branch ==="

# Configure git for merge commits.
git config user.email "actions@github.com"
git config user.name "GitHub Actions"

# Fetch the latest merge branch.
echo "Fetching latest ${MERGE_BRANCH} branch..."
git fetch origin "${MERGE_BRANCH}"

# Show current state for debugging.
echo "Current branch: $(git rev-parse --abbrev-ref HEAD)"
echo "Current commit: $(git rev-parse --short HEAD)"
echo "${MERGE_BRANCH} commit: $(git rev-parse --short origin/${MERGE_BRANCH})"

# Check if we actually need to merge.
if git merge-base --is-ancestor "origin/${MERGE_BRANCH}" HEAD; then
    echo "✓ Current branch is already up to date with ${MERGE_BRANCH}"
    exit 0
fi

# Attempt the merge.
echo "Merging ${MERGE_BRANCH} into current branch..."
if git merge "origin/${MERGE_BRANCH}" --no-edit --no-ff; then
    echo "✓ Successfully merged ${MERGE_BRANCH} into current branch"

    # Show merge summary.
    echo "Merge summary:"
    git log --oneline --graph -5
else
    # Merge failed - check if it's due to conflicts.
    echo "✗ Merge failed"

    # Show conflict details if any.
    if git diff --name-only --diff-filter=U | grep -q .; then
        echo ""
        echo "Merge conflicts detected in the following files:"
        git diff --name-only --diff-filter=U
        echo ""
        echo "The PR author needs to:"
        echo "1. Pull the latest ${MERGE_BRANCH} branch"
        echo "2. Merge or rebase their branch"
        echo "3. Resolve conflicts"
        echo "4. Push the updated branch"
    fi

    # Abort the merge to clean up.
    git merge --abort 2>/dev/null || true
    exit 1
fi
