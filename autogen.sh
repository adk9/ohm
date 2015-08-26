#!/bin/sh
[ -d libdwarf/config ] || mkdir -p libdwarf/config
set -e
autoreconf --force --install -I config . || exit 1
rm -rf autom4te.cache
