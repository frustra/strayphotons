cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if (UNIX)
	set(b2_exe "b2")
	set(bjam_exe "bjam")
	set(bootstrap_cmd "bootstrap.sh")
else() # Windows
	set(b2_exe "b2.exe")
	set(bjam_exe "bjam.exe")
	set(bootstrap_cmd "bootstrap.bat")
endif()

# build boost's own build system (b2)
add_custom_command(
	OUTPUT
		"${CMAKE_CURRENT_SOURCE_DIR}/${b2_exe}"
		"${CMAKE_CURRENT_SOURCE_DIR}/${bjam_exe}"
		"${CMAKE_CURRENT_SOURCE_DIR}/bootstrap.log"
		"${CMAKE_CURRENT_SOURCE_DIR}/project-config.jam"
	COMMAND ${bootstrap_cmd}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set(boost_libs
	"system"
	"filesystem"
	# add more here if needed
)

foreach(boost_lib ${boost_libs})
	if (UNIX)
		set(lib_file "boost_${boost_lib}-??????")
		set(lib_build_dir
			"${CMAKE_CURRENT_SOURCE_DIR}/??????")
	else () # Windows
		set(lib_file "libboost_${boost_lib}-vc140-mt-gd-1_61.lib")
		set(lib_build_dir
		"${CMAKE_CURRENT_SOURCE_DIR}/bin.v2/libs/${boost_lib}/build/msvc-14.0/debug/link-static/threading-multi")
	endif()

	set(out_file
		"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/${lib_file}")
	# set(out_implib
	# "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/boost_${boost_lib}.lib")
	set(build_file "${lib_build_dir}/${lib_file}")
	set(build_implib "${lib_build_dir}/${imp_lib}")

	add_custom_command(
		OUTPUT ${out_file}
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${b2_exe}"
		COMMAND
			"b2" "link=static" ${boost_lib}
		COMMAND
			${CMAKE_COMMAND} -E copy_if_different
			${build_file}
			${out_file}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Generating boost_${boost_lib} lib as ${out_file}"
		VERBATIM
	)

	add_custom_target(boost_${boost_lib}_target
		DEPENDS "${out_file}"
	)

	add_library(boost_${boost_lib} IMPORTED GLOBAL STATIC)
	set_property(
		TARGET boost_${boost_lib}
		PROPERTY IMPORTED_LOCATION ${out_file}
	)

	add_dependencies(boost_${boost_lib} boost_${boost_lib}_target)

	if (NOT ${boost_lib} STREQUAL "system")
		add_dependencies(boost_${boost_lib}_target boost_system_target)
	endif()
endforeach()
