cd bin
MESA_GL_VERSION_OVERRIDE=4.3 \
MESA_GLSL_VERSION_OVERRIDE=430 \
MESA_EXTENSION_OVERRIDE=GL_ARB_compute_shader \

if [ -z "$RUN_DBG" ]; then
	./sp "$@"
else
	$RUN_DBG ./sp
   #	-o "run $@"
fi
