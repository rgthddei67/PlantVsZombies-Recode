get_filename_component(_YY_THUNKS_PACKAGE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(YY_THUNKS_WIN7_X64_LIBRARY_DIR
    "${_YY_THUNKS_PACKAGE_ROOT}/lib/yy-thunks/6.1.7600.0/x64")
set(YY_THUNKS_WIN7_X64_OBJECT
    "${YY_THUNKS_WIN7_X64_LIBRARY_DIR}/YY_Thunks_for_Win7.obj")
set(YY_THUNKS_WIN7_X64_EXPORT_MAP
    "${CMAKE_CURRENT_LIST_DIR}/win7-x64-exports.txt")

if(NOT EXISTS "${YY_THUNKS_WIN7_X64_LIBRARY_DIR}/kernel32.Lib")
    message(FATAL_ERROR "YY-Thunks Win7 x64 import libraries are incomplete")
endif()
if(NOT EXISTS "${YY_THUNKS_WIN7_X64_OBJECT}")
    message(FATAL_ERROR "YY-Thunks Win7 x64 aggregate object is missing")
endif()
if(NOT EXISTS "${YY_THUNKS_WIN7_X64_EXPORT_MAP}")
    message(FATAL_ERROR "YY-Thunks Win7 x64 export map is missing")
endif()

unset(_YY_THUNKS_PACKAGE_ROOT)
