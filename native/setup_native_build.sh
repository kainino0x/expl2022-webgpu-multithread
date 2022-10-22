#!/bin/bash

set -e
THIRD_PARTY="$(dirname "$0")"/third_party/

mkdir -p "$THIRD_PARTY"
git clone https://dawn.googlesource.com/dawn "$THIRD_PARTY/dawn"

cd "$THIRD_PARTY/dawn"
git checkout --detach 4b015154185d419854bd53dda35ec32e0066321c --

cp scripts/standalone.gclient .gclient
gclient sync

cd ..

# glfw 3.3 stable

git clone https://github.com/glfw/glfw.git "glfw3"
cd "glfw3"
git checkout 3.3-stable
