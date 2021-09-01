
# No need to build the CXXOPTS tests
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "Disable CXXOPTS Test Suite" FORCE)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "Disable CXXOPTS Examples" FORCE)
add_subdirectory(cxxopts)
