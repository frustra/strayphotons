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
	"filesystem"
	# add more here if needed
)

foreach(boost_lib ${boost_libs})
	add_custom_target(${boost_lib}_target
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${b2_exe}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND "b2" ${boost_lib}
	)

	add_library(boost_${boost_lib} STATIC IMPORTED GLOBAL)
	add_dependencies(boost_${boost_lib} ${boost_lib}_target)
endforeach()
