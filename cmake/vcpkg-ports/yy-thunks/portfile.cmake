if(NOT VCPKG_TARGET_IS_WINDOWS OR NOT VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
    message(FATAL_ERROR "This project packages only the Windows x64 YY-Thunks libraries.")
endif()

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)
set(VCPKG_POLICY_ONLY_RELEASE_CRT enabled)
set(VCPKG_POLICY_MISMATCHED_NUMBER_OF_BINARIES enabled)

vcpkg_download_distfile(YY_THUNKS_ARCHIVE
    URLS "https://github.com/Chuyu-Team/YY-Thunks/releases/download/v1.2.2/YY-Thunks-Lib.zip"
    FILENAME "YY-Thunks-Lib-v1.2.2.zip"
    SHA512 930f3e319f41225fb10195531a9b994e3e22157d5869aee509ff77e8694e6d8855b103d029f85f3572b27d04d9df9830c1ac2bbcb6f73edfdc53d49267386a58
)
vcpkg_download_distfile(YY_THUNKS_OBJS_ARCHIVE
    URLS "https://github.com/Chuyu-Team/YY-Thunks/releases/download/v1.2.2/YY-Thunks-Objs.zip"
    FILENAME "YY-Thunks-Objs-v1.2.2.zip"
    SHA512 4b1ffc333b6cf6bd9fdf318801fc5d554da494c11dfc812523e790177f624eac010e8f7b6b35c92134fc31df39196c46b73659175f8d37efd6ee7f50126525a4
)

vcpkg_extract_source_archive(
    YY_THUNKS_LIB_SOURCE
    ARCHIVE "${YY_THUNKS_ARCHIVE}"
    NO_REMOVE_ONE_LEVEL
)
vcpkg_extract_source_archive(
    YY_THUNKS_OBJS_SOURCE
    ARCHIVE "${YY_THUNKS_OBJS_ARCHIVE}"
    NO_REMOVE_ONE_LEVEL
)

set(YY_THUNKS_WIN7_X64_SOURCE "${YY_THUNKS_LIB_SOURCE}/Lib/6.1.7600.0/x64")
set(YY_THUNKS_WIN7_X64_DESTINATION "lib/yy-thunks/6.1.7600.0/x64")

# LLD 使用官方 Lib 包中的整套替代 import libraries；MSVC 使用 Objs 包的 Win7 聚合对象。
file(INSTALL "${YY_THUNKS_WIN7_X64_SOURCE}/"
    DESTINATION "${CURRENT_PACKAGES_DIR}/${YY_THUNKS_WIN7_X64_DESTINATION}"
    FILES_MATCHING
        PATTERN "*.lib"
        PATTERN "*.Lib"
)
file(INSTALL "${YY_THUNKS_OBJS_SOURCE}/objs/x64/YY_Thunks_for_Win7.obj"
    DESTINATION "${CURRENT_PACKAGES_DIR}/${YY_THUNKS_WIN7_X64_DESTINATION}"
)

file(INSTALL "${YY_THUNKS_LIB_SOURCE}/Config/x64/6.1.7600.txt"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
    RENAME "win7-x64-exports.txt"
)
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/yy-thunks-config.cmake"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)
vcpkg_install_copyright(FILE_LIST "${YY_THUNKS_LIB_SOURCE}/LICENSE")
