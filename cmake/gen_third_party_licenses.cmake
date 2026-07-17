# gen_third_party_licenses.cmake — 汇总所有第三方库的许可证声明
#
# 用法：
#   cmake -DSHARE_DIR=<vcpkg_installed/<triplet>/share> -DVMA_HEADER=<vk_mem_alloc.h> -DOUT=<输出文件> -P gen_third_party_licenses.cmake
#
# 遍历 SHARE_DIR 下每个 <包名>/copyright，逐字节拼成一份清单写入 OUT。
# vcpkg 为每个库都备好了原样的 copyright 文件——本脚本只做归集，不改写正文，
# 保证法律文本与上游逐字一致（含 IJG / FreeType 等"必须出现的一句话"）。
#
# VMA 是从 Vulkan SDK 头文件直接编译进程序，不在 vcpkg share 中；脚本额外提取其文件头的
# 原始 MIT 声明。合规义务是随二进制分发物附上所选宽松许可证声明。

if(NOT EXISTS "${SHARE_DIR}")
    message(FATAL_ERROR "[licenses] vcpkg share 目录不存在: ${SHARE_DIR} —— 先构建一次任意 preset 让 vcpkg 物化依赖")
endif()

if(NOT EXISTS "${VMA_HEADER}")
    message(FATAL_ERROR "[licenses] VMA 头文件不存在: ${VMA_HEADER}")
endif()

# 每个库的声明文件惯例叫 copyright；GLOB 出全部，按名排序保证输出稳定（可复现 diff）。
file(GLOB _copyrights "${SHARE_DIR}/*/copyright")
list(SORT _copyrights)

if(NOT _copyrights)
    message(FATAL_ERROR "[licenses] 在 ${SHARE_DIR} 下没找到任何 */copyright")
endif()

# —— 文件头：说明用途，不点名具体库（保持 data-driven，依赖变动无需改本脚本）——
set(_out "THIRD-PARTY SOFTWARE LICENSES\n")
string(APPEND _out "================================================================================\n")
string(APPEND _out "This product includes the following third-party libraries.\n")
string(APPEND _out "Each is distributed under its own permissive (zlib / BSD / MIT-style) license.\n")
string(APPEND _out "The complete, verbatim license text of every library follows below.\n")
string(APPEND _out "Generated from upstream copyright files and headers; do not edit by hand.\n")
string(APPEND _out "================================================================================\n")

set(_names "")
foreach(_cp IN LISTS _copyrights)
    get_filename_component(_dir "${_cp}" DIRECTORY)
    get_filename_component(_pkg "${_dir}" NAME)
    list(APPEND _names "${_pkg}")
    file(READ "${_cp}" _body)
    string(APPEND _out "\n\n")
    string(APPEND _out "================================================================================\n")
    string(APPEND _out "${_pkg}\n")
    string(APPEND _out "================================================================================\n\n")
    string(APPEND _out "${_body}")
endforeach()

# VMA 的 MIT 正文位于 vk_mem_alloc.h 起始连续 // 注释块；逐行去掉注释前缀，
# 保留上游原文。遇到第一个非 // 行即停止，避免把实现或文档正文写入许可证。
file(STRINGS "${VMA_HEADER}" _vma_lines ENCODING UTF-8)
set(_vma_body "")
set(_vma_started FALSE)
foreach(_line IN LISTS _vma_lines)
    if(_line MATCHES "^//")
        set(_vma_started TRUE)
        string(REGEX REPLACE "^// ?" "" _license_line "${_line}")
        string(APPEND _vma_body "${_license_line}\n")
    elseif(_vma_started)
        break()
    endif()
endforeach()

if(NOT _vma_body MATCHES "Permission is hereby granted")
    message(FATAL_ERROR "[licenses] 无法从 ${VMA_HEADER} 提取完整 MIT 声明")
endif()

# 去掉注释块外围的空 // 行，最终仅保留一个换行，保证根快照 diff 稳定。
string(REGEX REPLACE "^\n+" "" _vma_body "${_vma_body}")
string(REGEX REPLACE "\n+$" "\n" _vma_body "${_vma_body}")

list(APPEND _names "vma")
string(APPEND _out "\n\n")
string(APPEND _out "================================================================================\n")
string(APPEND _out "vma\n")
string(APPEND _out "================================================================================\n\n")
string(APPEND _out "${_vma_body}")

file(WRITE "${OUT}" "${_out}")

list(LENGTH _names _count)
string(REPLACE ";" ", " _names_str "${_names}")
message(STATUS "[licenses] 已写入 ${OUT}（${_count} 个库: ${_names_str}）")
