# gen_manifest.cmake — 构建期生成资源清单（取代运行时 std::filesystem 目录枚举）
#
# 用法（由 CMakeLists POST_BUILD 调用）：
#   cmake -DRES_DIR=<资源目录> -DRES_PARENT=<资源目录的父> -DOUT=<清单输出> -P gen_manifest.cmake
#
# 遍历 RES_DIR 下所有文件，以"相对 RES_PARENT 且带 ./ 前缀"的形式逐行写入 OUT，
# 形态与运行时 directory_iterator 今日产出的字符串逐字一致
#（如 ./resources/image/reanim/foo.png），保证下游按名加载零差异。
#
# 详见 docs/superpowers/specs/2026-06-25-resource-manifest-enumeration-design.md

if(NOT EXISTS "${RES_DIR}")
    message(WARNING "[gen_manifest] 资源目录不存在，写空清单: ${RES_DIR}")
    file(WRITE "${OUT}" "")
    return()
endif()

# GLOB_RECURSE 默认只返回文件（不含目录）；RELATIVE 到 RES_PARENT 得到 resources/... 形态。
file(GLOB_RECURSE _files RELATIVE "${RES_PARENT}" "${RES_DIR}/*")
list(SORT _files)

set(_content "")
foreach(_f IN LISTS _files)
    # 排除清单自身，避免把 manifest.txt 列进 manifest。
    if(NOT _f STREQUAL "resources/manifest.txt")
        string(APPEND _content "./${_f}\n")
    endif()
endforeach()

file(WRITE "${OUT}" "${_content}")
