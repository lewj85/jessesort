#!/usr/bin/env bash
set -eu

# Builds and runs the tests on the current worktree and the last commit or a different
# commit, when given one. Run from this directory.
# Usage:
#   ./compare [commit]

COMMIT="${1:-HEAD}"
TMP_DIR="$(mktemp -d)"

cleanup() {
    echo
    echo "==> Cleaning up the detached head in $TMP_DIR ..."
    git worktree remove --force "$TMP_DIR" >/dev/null 2>&1 || true
    rm -rf "$TMP_DIR"
    echo "Done."
}
trap cleanup EXIT

echo "==> Running on current working tree ..."
(
    make -B
    ./jesseSortTest
)

echo
echo
echo "==> Running on commit: $COMMIT in $TMP_DIR ..."

git worktree add --detach "$TMP_DIR" "$COMMIT" >/dev/null

(
    cd "${TMP_DIR}/src/jessesort"
    make -B
    ./jesseSortTest
)

