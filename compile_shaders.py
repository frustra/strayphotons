#!/usr/bin/env python

import os
from glob import glob
from subprocess import call

extensions = ['vert', 'tesc', 'tese', 'geom', 'frag', 'comp']
files = []

for ext in extensions:
	files += glob('./src/shaders/*.' + ext)

for f in files:
	out = './assets/' + f[6:] + '.spv'
	dir = os.path.dirname(out)

	if not os.path.exists(dir):
		os.makedirs(dir)

	call(['glslangValidator', '-V', f, '-o', out])
