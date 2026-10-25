#!/bin/sh
# Cuts a release of the version in VERSION:
#
#   tools/release/release.sh [--emu BASE-IMG] [--push] [--remote NAME]
#
#   1. checks: clean working tree, CHANGELOG.md has a section for the
#      version, the tag vX.Y.Z does not exist yet
#   2. make test, make release (build/release/: ZIP, floppy image, SHA256SUMS)
#   3. with --emu: installs, reboots and removes in QEMU from the floppy image
#      (tools/emu/test_setup.py; BASE-IMG from tools/emu/mkwin31.py)
#   4. creates the annotated tag vX.Y.Z; with --push pushes it, and the
#      release job of .github/workflows/build.yml builds the same
#      files on GitHub and publishes the release
#
# To prepare a release: set VERSION, add a "## X.Y.Z" section to
# CHANGELOG.md (it becomes the release notes) and commit both.
#
# Where tags cannot be pushed: push a commit whose message starts with
# "Release vX.Y.Z" to the branch (or start the release workflow by hand on
# it); GitHub then creates the tag with the release.
set -e
cd "$(dirname "$0")/../.."

emu= push= remote=origin
while [ $# -gt 0 ]; do
    case "$1" in
    --emu) emu=$2; shift ;;
    --push) push=1 ;;
    --remote) remote=$2; shift ;;
    *) echo "usage: $0 [--emu BASE-IMG] [--push] [--remote NAME]" >&2; exit 2 ;;
    esac
    shift
done

die() { echo "release: $*" >&2; exit 1; }

v=$(cat VERSION)
echo "$v" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || die "VERSION '$v' is not X.Y.Z"
git diff --quiet && git diff --cached --quiet || die "the working tree has uncommitted changes"
grep -Eq "^## \[?$v\]?( |\$)" CHANGELOG.md || die "CHANGELOG.md has no section '## $v'"
! git rev-parse -q --verify "refs/tags/v$v" >/dev/null || die "tag v$v exists already"

make test
make release
if [ -n "$emu" ]; then
    python3 tools/emu/test_setup.py --base "$emu" --floppy "build/release/retheme16-$v-disk.img"
fi

git tag -a "v$v" -m "Retheme16 $v"
echo "tagged v$v"
if [ -n "$push" ]; then
    git push "$remote" "v$v"
    echo "pushed v$v: the release workflow publishes it on GitHub"
else
    echo "publish with: git push $remote v$v"
fi
