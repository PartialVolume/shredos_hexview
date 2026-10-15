#!/bin/sh
#
# autogen.sh - Bootstrap build configuration for shredos_hexviewer
#

echo "Running autoreconf..."
autoreconf --install --force --verbose
echo "Now run ./configure and make"
