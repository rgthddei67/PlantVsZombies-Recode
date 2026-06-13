# CMake 构建（与 PlantsVsZombies.sln 并行共存，旧 VS 工程零改动）
# 用法见 CMakePresets.json：cmake --preset msvc-release && cmake --build --preset msvc-release
cmake_minimum_required(VERSION 3.25)

project(PlantsVsZombies LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 与 vcxproj 一致：静态运行时 /MT（Release）、/MTd（Debug），匹配 x64-windows-static triplet
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- 依赖（vcpkg manifest + 系统 Vulkan SDK）----
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/PlantVsZombies)

set(SOURCES
    CrashHandler.cpp
    CursorManager.cpp
    FileManager.cpp
    GameApp.cpp
    GameInfoSaver.cpp
    GameMonitor.cpp
    Logger.cpp
    Graphics.cpp
    main.cpp
    ResourceManager.cpp
    Game/AutoTest/TestDriver.cpp
    Game/AlmanacScene.cpp
    Game/AnimatedObject.cpp
    Game/AudioSystem.cpp
    Game/Board.cpp
    Game/CursorObjectManager.cpp
    Game/GameObjectManager.cpp
    Game/LawnMower.cpp
    Game/PlantAlmanacScene.cpp
    Game/Shovel.cpp
    Game/ShovelBank.cpp
    Game/Card.cpp
    Game/CardComponent.cpp
    Game/CardDisplayComponent.cpp
    Game/CardSlotManager.cpp
    Game/ChooseCardUI.cpp
    Game/ClickableComponent.cpp
    Game/Coin.cpp
    Game/ColliderComponent.cpp
    Game/Component.cpp
    Game/EntityManager.cpp
    Game/FlagMeter.cpp
    Game/GameObject.cpp
    Game/GameProgress.cpp
    Game/GameScene.cpp
    Game/MainMenuScene.cpp
    Game/Scene.cpp
    Game/SceneManager.cpp
    Game/ShadowComponent.cpp
    Game/Trophy.cpp
    Game/ZombieAlmanacScene.cpp
    Game/Bullet/Bullet.cpp
    Game/ObjectPool/BulletPool.cpp
    Game/Plant/CherryBomb.cpp
    Game/Plant/Chomper.cpp
    Game/Plant/GameDataManager.cpp
    Game/Plant/Plant.cpp
    Game/Plant/PotatoMine.cpp
    Game/Plant/PuffShroom.cpp
    Game/Plant/Shooter.cpp
    Game/Plant/Shroom.cpp
    Game/Plant/WallNut.cpp
    Game/Zombie/BucketZombie.cpp
    Game/Zombie/ConeZombie.cpp
    Game/Zombie/FastBucketZombie.cpp
    Game/Zombie/FastPaperZombie.cpp
    Game/Zombie/PaperZombie.cpp
    Game/Zombie/Polevaulter.cpp
    Game/Zombie/Zombie.cpp
    ParticleSystem/Particle.cpp
    ParticleSystem/ParticleConfig.cpp
    ParticleSystem/ParticleEffect.cpp
    ParticleSystem/ParticleEmitter.cpp
    ParticleSystem/ParticleSystem.cpp
    ParticleSystem/ParticleXMLConfig.cpp
    ParticleSystem/ParticleXMLLoader.cpp
    Reanimation/Animator.cpp
    Reanimation/Reanimation.cpp
    Renderer/VmaImpl.cpp
    Renderer/VulkanBuffer.cpp
    Renderer/VulkanContext.cpp
    Renderer/VulkanPipeline.cpp
    Renderer/VulkanRenderer.cpp
    Renderer/VulkanTexturePool.cpp
    UI/Button.cpp
    UI/ButtonManager.cpp
    UI/GameMessageBox.cpp
    UI/InputHandler.cpp
    UI/Slider.cpp
    UI/SliderManager.cpp
)
list(TRANSFORM SOURCES PREPEND ${SRC_DIR}/)

add_executable(PlantsVsZombies ${SOURCES})

target_include_directories(PlantsVsZombies PRIVATE ${SRC_DIR})

target_compile_definitions(PlantsVsZombies PRIVATE
    _CONSOLE
    $<$<CONFIG:Release>:_HAS_ITERATOR_DEBUGGING=0 _SCL_SECURE_NO_WARNINGS>
)

# /utf-8 必须：源文件里有中文 UI 字符串（cl 与 clang-cl 都接受）
target_compile_options(PlantsVsZombies PRIVATE /utf-8 /W3 /sdl /MP)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # 对应旧 ClangRelease|x64：-O3 -march=native -flto（LTO 走 lld-link，见 preset）
    target_compile_options(PlantsVsZombies PRIVATE
        $<$<CONFIG:Release>:/clang:-O3 -march=native -flto>
    )
    target_link_options(PlantsVsZombies PRIVATE $<$<CONFIG:Release>:-flto>)
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
    glm::glm
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    Vulkan::Vulkan
    version setupapi imm32 winmm legacy_stdio_definitions
)

# ---- GLSL → SPIR-V（复刻 vcxproj 的 CompileShaders Target，增量编译）----
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin REQUIRED)

set(SHADER_SOURCES
    batch.vert.glsl:vert
    batch.frag.glsl:frag
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

# 只拷 Shader 到输出目录（resources/font 不拷——运行时工作目录用 x64/Release）
add_custom_command(TARGET PlantsVsZombies POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SRC_DIR}/Shader/spv $<TARGET_FILE_DIR:PlantsVsZombies>/Shader
    COMMENT "Copying Shader spv to output directory"
)
