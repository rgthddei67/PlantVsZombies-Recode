cmake_minimum_required(VERSION 3.15)
project(libopenmpt LANGUAGES C CXX)

find_package(ZLIB REQUIRED)

file(GLOB HEADERS libopenmpt/*.h libopenmpt/*.hpp src/openmpt/all/*.hpp)
list(FILTER HEADERS EXCLUDE REGEX "_impl\\.hpp$|_internal\\.h$")

file(GLOB_RECURSE SOURCES
    common/*.cpp
    soundbase/*.cpp
    sounddsp/*.cpp
    soundlib/*.cpp
    libopenmpt/libopenmpt_c.cpp
    libopenmpt/libopenmpt_cxx.cpp
    libopenmpt/libopenmpt_ext_impl.cpp
    libopenmpt/libopenmpt_impl.cpp
	include/minimp3/minimp3.c
	include/stb_vorbis/stb_vorbis.c
)

add_library(libopenmpt ${SOURCES})
set_target_properties(libopenmpt PROPERTIES OUTPUT_NAME openmpt)
target_compile_features(libopenmpt PUBLIC cxx_std_17)

target_include_directories(libopenmpt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/build/svn_version
        ${CMAKE_CURRENT_SOURCE_DIR}/libopenmpt
        ${CMAKE_CURRENT_SOURCE_DIR}/common
		${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/soundbase
        ${CMAKE_CURRENT_SOURCE_DIR}/sounddsp
        ${CMAKE_CURRENT_SOURCE_DIR}/soundlib
)

target_compile_definitions(libopenmpt PRIVATE
    LIBOPENMPT_BUILD
    MPT_BUILD_VCPKG
    MPT_WITH_MINIMP3
    MPT_WITH_STBVORBIS
    MPT_WITH_ZLIB
	STB_VORBIS_NO_PULLDATA_API
	STB_VORBIS_NO_STDIO
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(libopenmpt PRIVATE LIBOPENMPT_BUILD_DLL)
endif()

if(MSVC)
    target_compile_options(libopenmpt PRIVATE
        /W3 /wd4005 /wd4018 /wd4100 /wd4244 /wd4267 /wd4996
    )
    target_compile_definitions(libopenmpt PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        _CRT_NONSTDC_NO_WARNINGS
    )
endif()

target_link_libraries(libopenmpt PRIVATE ZLIB::ZLIB)

install(TARGETS libopenmpt EXPORT libopenmpt-targets)
install(EXPORT libopenmpt-targets DESTINATION share/libopenmpt NAMESPACE libopenmpt::)
install(FILES ${HEADERS} DESTINATION include/libopenmpt)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/libopenmpt-config.cmake" [[
include(CMakeFindDependencyMacro)
find_dependency(ZLIB)
include("${CMAKE_CURRENT_LIST_DIR}/libopenmpt-targets.cmake")
]])
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libopenmpt-config.cmake" DESTINATION share/libopenmpt)
