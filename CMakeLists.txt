# CMake 是本项目唯一构建系统。
# 日常构建见 CMakePresets.json：cmake --preset clang-playtest && cmake --build --preset clang-playtest
cmake_minimum_required(VERSION 3.29)

project(PlantsVsZombies LANGUAGES CXX)

option(PVZ_ENABLE_LTO "为发布构建启用 Clang LTO" OFF)
set(PVZ_SHARED_RUNTIME_ROOT "" CACHE PATH
    "共享 resources/font 的权威运行目录；留空表示当前构建持有实体目录")

# 非发布预设通过 NTFS 目录联接复用发布版的 resources/font。联接只在首次配置时创建，
# 不复制资源；Shader、存档与 AutoTest 输出仍保留在各自 build/<preset>/ 中。
if(PVZ_SHARED_RUNTIME_ROOT)
    execute_process(
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ensure_runtime_asset_links.ps1
                -CanonicalRoot ${PVZ_SHARED_RUNTIME_ROOT}
                -BuildRoot ${CMAKE_BINARY_DIR}
        RESULT_VARIABLE _asset_link_result
        OUTPUT_VARIABLE _asset_link_output
        ERROR_VARIABLE _asset_link_error
    )
    if(NOT _asset_link_result EQUAL 0)
        message(FATAL_ERROR
            "共享运行资源联接失败:\n${_asset_link_output}${_asset_link_error}")
    endif()
    string(STRIP "${_asset_link_output}" _asset_link_output)
    if(_asset_link_output)
        message(STATUS "${_asset_link_output}")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 与 vcxproj 一致：静态运行时 /MT（Release）、/MTd（Debug），匹配 x64-windows-static triplet
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- 依赖（vcpkg manifest + 系统 Vulkan SDK）----
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(libopenmpt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

set(VMA_HEADER "${Vulkan_INCLUDE_DIR}/vma/vk_mem_alloc.h")
if(NOT EXISTS "${VMA_HEADER}")
    message(FATAL_ERROR "Vulkan SDK 缺少 VMA 头文件: ${VMA_HEADER}")
endif()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/PlantVsZombies)

# 自动收集源文件：CONFIGURE_DEPENDS 让 Ninja 在每次构建前检查 glob 结果，
# 新增/删除 .cpp 自动触发重新 configure，无需手动维护清单。
# 代价：vcxproj 之外多出的"野" .cpp 也会被编进来，须在下方排除名单里登记。
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
# 排除名单：在磁盘上但不参与编译的文件（与 vcxproj 保持一致）
list(REMOVE_ITEM SOURCES
    ${SRC_DIR}/Reanimation/AttachmentSystem.cpp
)

add_executable(PlantsVsZombies ${SOURCES} "PlantVsZombies/Game/Zombie/DoorZombie.cpp" "PlantVsZombies/Game/Plant/HypnoShroom.h" "PlantVsZombies/Game/Zombie/FootballZombie.h" "PlantVsZombies/Game/Zombie/FootballZombie.cpp")

target_include_directories(PlantsVsZombies PRIVATE ${SRC_DIR})

target_compile_definitions(PlantsVsZombies PRIVATE
    _CONSOLE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:
        _HAS_ITERATOR_DEBUGGING=0 _SCL_SECURE_NO_WARNINGS>
)

# /utf-8 必须：源文件里有中文 UI 字符串（cl 与 clang-cl 都接受）
# 注：不需要 /MP——那是 MSBuild 单进程多文件的旗标，Ninja 本身按翻译单元并行调度
target_compile_options(PlantsVsZombies PRIVATE /utf-8 /W3 /sdl /EHsc)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Release 与 Playtest 使用相同的运行期优化；只有正式发布预设额外启用 LTO。
    target_compile_options(PlantsVsZombies PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:
            /O2                     # MSVC 风格，clang-cl 原生识别，不加 /clang:
            /arch:AVX2              # 同上
            /fp:fast                # 同上，等价于 -ffast-math
            /clang:-fvectorize      # 每个 clang 专属选项单独传递
            /clang:-fomit-frame-pointer
        >
    )
    if(PVZ_ENABLE_LTO)
        target_compile_options(PlantsVsZombies PRIVATE $<$<CONFIG:Release>:-flto>)
    endif()
    target_link_options(PlantsVsZombies PRIVATE
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:/OPT:REF /OPT:ICF>
    )
else()
    # 对应旧 Release|x64：MaxSpeed + AVX2 + fast-math + LTCG
    target_compile_options(PlantsVsZombies PRIVATE
        $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /Oy /arch:AVX2 /fp:fast /GL>
    )
    target_link_options(PlantsVsZombies PRIVATE $<$<CONFIG:Release>:/LTCG>)
endif()

target_link_options(PlantsVsZombies PRIVATE /STACK:4194304,65536)

target_link_libraries(PlantsVsZombies PRIVATE
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    libopenmpt::libopenmpt
    glm::glm
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    Vulkan::Vulkan
    version setupapi imm32 winmm shell32 ole32 legacy_stdio_definitions
)

# 存档迁移是纯文件系统逻辑，单独目标可在不启动游戏、不接触真实玩家目录的情况下验证。
include(CTest)
if(BUILD_TESTING)
    add_executable(SaveMigrationTests
        tests/SaveMigrationTests.cpp
        PlantVsZombies/SaveMigration.cpp
    )
    target_include_directories(SaveMigrationTests PRIVATE ${SRC_DIR})
    target_compile_options(SaveMigrationTests PRIVATE /utf-8 /W3 /sdl /EHsc)
    add_test(NAME save-migration COMMAND SaveMigrationTests)
endif()

# ---- GLSL → SPIR-V（复刻 vcxproj 的 CompileShaders Target，增量编译）----
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin REQUIRED)

set(SHADER_SOURCES
    batch.vert.glsl:vert
    batch.frag.glsl:frag
    pool.vert.glsl:vert
    pool.frag.glsl:frag
    reanim_inst.vert.glsl:vert
    reanim_inst.frag.glsl:frag
)

set(SPV_OUTPUTS "")
foreach(entry IN LISTS SHADER_SOURCES)
    string(REPLACE ":" ";" parts ${entry})
    list(GET parts 0 shader)
    list(GET parts 1 stage)
    # 只剥 .glsl（NAME_WE 会把 batch.vert.glsl 剥成 batch，与 batch.frag 撞名）
    string(REPLACE ".glsl" "" name ${shader})
    # 与旧工程一致：spv 落在源码树 Shader/spv/，三个 preset 共享同一份产物
    set(spv ${SRC_DIR}/Shader/spv/${name}.spv)
    add_custom_command(
        OUTPUT ${spv}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SRC_DIR}/Shader/spv
        COMMAND ${GLSLC} -fshader-stage=${stage} --target-env=vulkan1.2 -O
                ${SRC_DIR}/Shader/${shader} -o ${spv}
        DEPENDS ${SRC_DIR}/Shader/${shader}
        COMMENT "[glslc] Shader/${shader} -> Shader/spv/${name}.spv"
    )
    list(APPEND SPV_OUTPUTS ${spv})
endforeach()

add_custom_target(CompileShaders DEPENDS ${SPV_OUTPUTS})
add_dependencies(PlantsVsZombies CompileShaders)

# 只拷 Shader 到输出目录；resources/font 由 clang-release 持有，其他预设通过目录联接共享。
# 注意：copy_directory 拷的是"目录内容"，目标必须写全 Shader/spv（游戏按 Shader/spv/*.spv 加载）
add_custom_command(TARGET PlantsVsZombies POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${SRC_DIR}/Shader/spv $<TARGET_FILE_DIR:PlantsVsZombies>/Shader/spv
    COMMENT "Copying Shader spv to output directory"
)

# ---- 生成资源清单 resources/manifest.txt（取代运行时 std::filesystem 目录枚举）----
# 直接 glob 紧挨 exe 的真实资源目录（非发布预设会经目录联接看到同一份实体资源），
# 每次构建重生成 = 零过期窗口。运行时 FileManager::ListResourceFiles 经 SDL_RWops 读它，
# 使资源目录列举在 Android(APK) 可用。详见
# docs/superpowers/specs/2026-06-25-resource-manifest-enumeration-design.md
add_custom_command(TARGET PlantsVsZombies POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DRES_DIR=$<TARGET_FILE_DIR:PlantsVsZombies>/resources
            -DRES_PARENT=$<TARGET_FILE_DIR:PlantsVsZombies>
            -DOUT=$<TARGET_FILE_DIR:PlantsVsZombies>/resources/manifest.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_manifest.cmake
    COMMENT "Generating resources/manifest.txt"
)

# ---- 生成第三方许可证汇总 THIRD-PARTY-LICENSES.txt（紧挨 exe，随发布包分发）----
# 全树静态链接库均为 zlib/BSD/MIT 宽松证（零 copyleft），合规义务仅"随二进制分发物
# 附上版权与许可证声明"——本文件即满足。直接归集 vcpkg 的 <triplet>/share/*/copyright，
# 并从 Vulkan SDK 的 VMA 头文件提取原始 MIT 声明，每次构建重生成 = 零过期窗口。
# 仓库根另有一份提交快照供 GitHub 浏览；
# 依赖变动时本产物自动反映，根快照需重跑生成器刷新（见 cmake/gen_third_party_licenses.cmake）。
add_custom_command(TARGET PlantsVsZombies POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DSHARE_DIR=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share
            -DVMA_HEADER=${VMA_HEADER}
            -DOUT=$<TARGET_FILE_DIR:PlantsVsZombies>/THIRD-PARTY-LICENSES.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_third_party_licenses.cmake
    COMMENT "Generating THIRD-PARTY-LICENSES.txt"
)

# ---- 头文件守卫：自动装 pre-commit 钩子 + configure 期 lint ----
# 迁离 .sln 后 VS 模板不再自动塞 #pragma once，这里在提交关口兜底并即时反馈。
# BOM-aware：只查 token 是否存在（不锚 ^），故 UTF-8 BOM 前缀不会误判。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    find_program(GIT_EXECUTABLE git)
    if(GIT_EXECUTABLE)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} config core.hooksPath .githooks
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE _hookcfg OUTPUT_QUIET ERROR_QUIET
        )
        if(_hookcfg EQUAL 0)
            message(STATUS "git core.hooksPath -> .githooks（头文件守卫 pre-commit 已启用）")
        endif()
    endif()
endif()

file(GLOB_RECURSE _ALL_HEADERS CONFIGURE_DEPENDS ${SRC_DIR}/*.h)
set(_UNGUARDED "")
foreach(_h IN LISTS _ALL_HEADERS)
    file(READ "${_h}" _head LIMIT 512)
    if(NOT _head MATCHES "#pragma once" AND NOT _head MATCHES "#ifndef")
        list(APPEND _UNGUARDED "${_h}")
    endif()
endforeach()
if(_UNGUARDED)
    string(REPLACE ";" "\n    " _UNGUARDED_MSG "${_UNGUARDED}")
    message(WARNING "以下头文件缺少守卫(#pragma once / #ifndef)，请补：\n    ${_UNGUARDED_MSG}")
endif()
