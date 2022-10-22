#!/bin/bash

set -e
THIRD_PARTY="$(dirname "$0")"/third_party/

# TODO: move to setup_native_build.sh

# glfw 3.3 stable

git clone https://github.com/glfw/glfw.git "$THIRD_PARTY/glfw3"
cd "$THIRD_PARTY/glfw3"
git checkout 3.3-stable
