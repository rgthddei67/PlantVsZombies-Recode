---
name: project-pvz-adaptive-mo3-music
description: 原版 MO3 动态分轨音乐：libopenmpt interactive channel mute + minimp3/stb_vorbis 宽松许可 overlay，覆盖五类关卡音乐和 burst 状态机
metadata:
  node_type: memory
  type: project
---

# 原版 MO3 动态分轨音乐

2026-07-17 完成首版；2026-08-23 修复选卡回移结束后 `StartGame()` 在主线程同步解析 MO3 的停顿。

## 资源与依赖

- 原版素材：`D:\PVZ\中文年度加强版完整版\Test\sounds\mainmusic.mo3` 与 `mainmusic_hihats.mo3`。
- 运行位置：`build/<preset>/resources/music/`。资源实体不进 Git，清理/新建 build tree 后必须重新复制；缺失时 `AudioSystem` 自动回退现有 OGG。
- `libopenmpt` 本体 BSD-3-Clause；不能直接采用当时 stock vcpkg port，因为静态 triplet 会带入 `mpg123`（LGPL）。仓库用 `cmake/vcpkg-ports/libopenmpt` overlay，编入 OpenMPT 0.8.6 随源码附带的 `minimp3`（CC0）和 `stb_vorbis`（选择 MIT），另链接 zlib；构建生成的 `THIRD-PARTY-LICENSES.txt` 必须同时包含三份声明。

## 架构与原版映射

- `AdaptiveMusicPlayer` 通过 SDL_mixer `Mix_HookMusic` 输出 S16 双声道；每个 layer 是独立 `openmpt::module_ext`，用 `openmpt::ext::interactive::set_channel_mute_status` 只保留对应 tracker channel。音频线程独占 module，游戏线程仅用 atomic 传音量/order 跳转请求。
- `GameScene` 在 Board 地形与读档结果确定后调用 `Board::PrepareBackgroundMusic()`；单一后台 worker 串行读取 MO3 并构建完整 Playback。请求用 generation 只发布最新地形，过期结果在 worker 线程销毁，主线程不等待解析。
- `AdaptiveMusicPlayer::Play()` 只接管已经完成的 Playback；极快提交或读档直入尚未准备好时立即返回，让 `AudioSystem` 先播放对应 OGG，随后由 `Board::Update` 的动态音乐更新入口在主线程安全切换到 MO3。菜单/选卡音乐会取消延迟接管，但不阻塞 worker；`AudioSystem::Shutdown` 停止 callback 后 join worker。
- 场景 order：DAY=0、NIGHT main=0x30/drums=0x5C、POOL=0x5E、FOG=0x7D、ROOF=0xB8。
- scheme 1 channel：DAY main0..23/drums24..26/hihat27；POOL main0..17/drums25..28/hihat18..24+29；FOG main0..15/drums16..22/hihat23；ROOF main0..17/drums18..20/hihat21。
- NIGHT 主轨和鼓轨均使用全部 channel；鼓进入时按主轨小节奇偶跳 order 76/77。
- burst：敌对、有头、未垂死僵尸数达到10进入；至少维持8秒，低于4后退出。DAY/POOL/FOG/ROOF 用4秒踩镲淡入，3秒后排队到小节边界开鼓，退出踩镲8秒淡出、鼓0.5秒；NIGHT 在小节边界开鼓并把主轨4秒淡出，退出鼓8秒淡出，主轨在11秒退出段末4秒淡回。
- 一大波倒计时强制 burst：NIGHT 在警告后0.5秒触发，其余场景3.5秒触发，与原版 750→700/400 tick 一致。

## 接入点与 foot-gun

- `AudioSystem::PlayMusic` 只把五类关卡 key 路由到 MO3；菜单、选卡、图鉴等继续走 SDL_mixer OGG。Stop/Pause/Resume/音量/Shutdown 两条路径都要同步处理，尤其 Shutdown 必须先卸 `Mix_HookMusic` 再 `Mix_CloseAudio`。
- 关卡音乐映射必须由 `Board.cpp` 的唯一 `BackgroundMusicKey()` 同时服务 Prepare/Play，避免预构建 tune 与正式播放 key 漂移；当前 `NIGHT_ROOF` 沿用 `MUSIC_NIGHT`。
- `Board::PlayBackgroundMusic` 映射：DAY、NIGHT、POOL、FOG、ROOF；NIGHT_ROOF 暂用 NIGHT。
- 音乐敌对数语义对齐原版 `CountZombiesOnScreen`：有头、未垂死、未魅惑；由 `Board::UpdateZombieMetrics` 每 0.5 游戏秒与血量汇总同遍历刷新缓存，避免动态音乐每帧额外 O(n) 扫描。本项目预览僵尸不注册 EntityRegistry，因此无需额外 preview 标志。
- 音频回调禁止每块新建大缓冲：`Playback::mixedBuffer` 与各 Layer renderBuffer 要复用 capacity。
- 修改 overlay port 后 Ninja 会触发 vcpkg 全局 buildtree 写入，沙箱下需提升构建权限；不要因权限错误误判为依赖编译失败。
- AutoTest 根状态 `adaptiveMusic` 导出 playing/currentTune/preparedTune、最近一次是否当帧启动、后台准备毫秒和主线程接管微秒；`smoke_adaptive_music_preload.json` 覆盖 DAY→POOL 换关。2026-08-23 最终 `clang-release` 可见专项中泳池完整准备 335ms、`StartGame` 接管 3us，两地图均当帧启动；跨地图、重置与存档重载的 `smoke_autotest_harness` 同样通过。墙钟夹具会受渲染帧节奏影响，不用它替代内部同链路耗时。
