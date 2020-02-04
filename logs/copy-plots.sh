#!/bin/bash

: ${DEST:=$HOME/skola/clanek-2018-steiner-trees/plots}

mkdir -p "$DEST"/{rect,pace,pace-nozel}

cp -v 007-rect/*.{tex,pdf} "$DEST/rect"
cp -v 008-pace/*.{tex,pdf} "$DEST/pace"
cp -v 009-pace-work/*.{tex,pdf} "$DEST/pace"
cp -v 010-nozel/*.{tex,pdf} "$DEST/pace-nozel"
cp -v 011-nozel-work/*.{tex,pdf} "$DEST/pace-nozel"

