---
name: project-pvz-adaptive-mo3-music
description: 原版 MO3 动态分轨音乐：libopenmpt interactive channel mute + minimp3/stb_vorbis 宽松许可 overlay，覆盖五类关卡音乐和 burst 状态机
metadata:
  node_type: memory
  type: project
---

# 原版 MO3 动态分轨音乐

2026-07-17 完成首版，`clang-release` 配置与编译通过；主人明确要求不跑 AutoTest，听感由主人手动验收。

## 资源与依赖

- 原版素材：`D:\PVZ\中文年度加强版完整版\Test\sounds\mainmusic.mo3` 与 `mainmusic_hihats.mo3`。
- 运行位置：`build/<preset>/resources/music/`。资源实体不进 Git，清理/新建 build tree 后必须重新复制；缺失时 `AudioSystem` 自动回退现有 OGG。
- `libopenmpt` 本体 BSD-3-Clause；不能直接采用当时 stock vcpkg port，因为静态 triplet 会带入 `mpg123`（LGPL）。仓库用 `cmake/vcpkg-ports/libopenmpt` overlay，编入 OpenMPT 0.8.6 随源码附带的 `minimp3`（CC0）和 `stb_vorbis`（选择 MIT），另链接 zlib；构建生成的 `THIRD-PARTY-LICENSES.txt` 必须同时包含三份声明。

## 架构与原版映射

- `AdaptiveMusicPlayer` 通过 SDL_mixer `Mix_HookMusic` 输出 S16 双声道；每个 layer 是独立 `openmpt::module_ext`，用 `openmpt::ext::interactive::set_channel_mute_status` 只保留对应 tracker channel。音频线程独占 module，游戏线程仅用 atomic 传音量/order 跳转请求。
- 场景 order：DAY=0、NIGHT main=0x30/drums=0x5C、POOL=0x5E、FOG=0x7D、ROOF=0xB8。
- scheme 1 channel：DAY main0..23/drums24..26/hihat27；POOL main0..17/drums25..28/hihat18..24+29；FOG main0..15/drums16..22/hihat23；ROOF main0..17/drums18..20/hihat21。
- NIGHT 主轨和鼓轨均使用全部 channel；鼓进入时按主轨小节奇偶跳 order 76/77。
- burst：敌对、有头、未垂死僵尸数达到10进入；至少维持8秒，低于4后退出。DAY/POOL/FOG/ROOF 用4秒踩镲淡入，3秒后排队到小节边界开鼓，退出踩镲8秒淡出、鼓0.5秒；NIGHT 在小节边界开鼓并把主轨4秒淡出，退出鼓8秒淡出，主轨在11秒退出段末4秒淡回。
- 一大波倒计时强制 burst：NIGHT 在警告后0.5秒触发，其余场景3.5秒触发，与原版 750→700/400 tick 一致。

## 接入点与 foot-gun

- `AudioSystem::PlayMusic` 只把五类关卡 key 路由到 MO3；菜单、选卡、图鉴等继续走 SDL_mixer OGG。Stop/Pause/Resume/音量/Shutdown 两条路径都要同步处理，尤其 Shutdown 必须先卸 `Mix_HookMusic` 再 `Mix_CloseAudio`。
- `Board::PlayBackgroundMusic` 映射：DAY、NIGHT、POOL、FOG、ROOF；NIGHT_ROOF 暂用 ROOF。
- `CountHostileZombiesForMusic` 语义对齐原版 `CountZombiesOnScreen`：有头、未垂死、未魅惑；本项目预览僵尸不注册 EntityManager，因此无需额外 preview 标志。
- 音频回调禁止每块新建大缓冲：`Playback::mixedBuffer` 与各 Layer renderBuffer 要复用 capacity。
- 修改 overlay port 后 Ninja 会触发 vcpkg 全局 buildtree 写入，沙箱下需提升构建权限；不要因权限错误误判为依赖编译失败。
