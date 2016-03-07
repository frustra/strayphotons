#!/bin/bash

for INFILE in src/shaders/*.glsl.*; do
	OUTFILE="${INFILE#src/}"
	OUTFILE="assets/${OUTFILE/.glsl./.}.spv"
	mkdir -p `dirname "$OUTFILE"`
	glslangValidator -V "$INFILE" -o "$OUTFILE"
done
