# gen_third_party_licenses.cmake — 汇总所有第三方库的许可证声明
#
# 用法：
#   cmake -DSHARE_DIR=<vcpkg_installed/<triplet>/share> -DOUT=<输出文件> -P gen_third_party_licenses.cmake
#
# 遍历 SHARE_DIR 下每个 <包名>/copyright，逐字节拼成一份清单写入 OUT。
# vcpkg 为每个库都备好了原样的 copyright 文件——本脚本只做归集，不改写正文，
# 保证法律文本与上游逐字一致（含 IJG / FreeType 等"必须出现的一句话"）。
#
# 设计裁决（见会话记录）：全树 16 个静态链接库均为 zlib/BSD/MIT 宽松证，零 copyleft；
# 合规义务仅"随二进制分发物附上版权与许可证声明"，本文件即满足该义务。

if(NOT EXISTS "${SHARE_DIR}")
    message(FATAL_ERROR "[licenses] vcpkg share 目录不存在: ${SHARE_DIR} —— 先构建一次任意 preset 让 vcpkg 物化依赖")
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
string(APPEND _out "This product statically links the following third-party libraries.\n")
string(APPEND _out "Each is distributed under its own permissive (zlib / BSD / MIT-style) license.\n")
string(APPEND _out "The complete, verbatim license text of every library follows below.\n")
string(APPEND _out "Generated from vcpkg copyright files; do not edit by hand.\n")
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

file(WRITE "${OUT}" "${_out}")

list(LENGTH _names _count)
string(REPLACE ";" ", " _names_str "${_names}")
message(STATUS "[licenses] 已写入 ${OUT}（${_count} 个库: ${_names_str}）")
