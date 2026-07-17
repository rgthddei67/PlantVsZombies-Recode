---
name: feedback-vcpkg-cache-deletion
description: "删 vcpkg-master 缓存(downloads/buildtrees/packages)的代价与边界——\"可再生\"≠\"删了免费\""
metadata:
  node_type: memory
  type: feedback
  originSessionId: 418dc96a-b203-4115-8603-0e22d58813fe
---

`D:\PVZ\vcpkg-master` 是本项目 vcpkg 工具本体(`CMakePresets.json` 的 `toolchainFile` 指向它的 `scripts/buildsystems/vcpkg.cmake`),**整个目录不能删**——删了 CMake configure 直接报错。可删的只有其下可再生缓存:`buildtrees`(~7.3G,最大头)/`packages`(~2.6G)/`downloads`(~1.3G),以及迁移到 manifest 前残留的经典 `installed`(~154M)。构建实际链接的库在 `build/<preset>/vcpkg_installed/`(CMakeCache 已证实),与经典 `installed` 是两份,删经典 `installed` 不影响构建。

**Why:** 2026-06-13 我建议主人删这几个缓存腾空间(回收 ~11GB),并断言"configure 会是秒过 no-op、删 downloads 无影响"。这是误判:删完后一次 configure 触发了 vcpkg **重装**,因缓存全没了只能**全量联网重下**,把 `vcpkg_installed` 推成"只剩 glm、缺 SDL2"的半成品(中断后 build 会失败)。"缓存可再生"被我等同于了"删了没代价",忽略了再生 = 一次昂贵的全量重下。

**How to apply:** 清 vcpkg 缓存前先确认"近期不会重新 configure、不会改 vcpkg.json 依赖";否则保留 `downloads`(避免重下)或干脆别清。若已清且 configure 触发了重装,唯一干净收尾是让 `cmake --preset <preset>` 完整跑完一次重新填满每个 preset 的 `vcpkg_installed`(三 preset 各自独立,需各跑一次)。诊断半成品状态:查 `vcpkg_installed/.../lib` 是否只剩个别 .lib + `glm.lib` 写入时间是否是刚刚 + 有无 vcpkg/cl 进程在跑。相关:[project_pvz_cmake_migration](project_pvz_cmake_migration.md)
