# 经典跳跳僵尸

## 当前契约（2026-08-15）

- `ZOMBIE_POGO` 由独立 `PogoZombie` 实现，500 本体生命；生存权重 1000，第 10 波、第 10 轮起可选。冒险模式在 4-7（内部 34）首次教学，并于 4-8（内部 35）继续复习。它没有水路状态机，`CanZombieTypeSpawnInPool` 必须返回 false，使泳池正式选行只落在四条陆路。
- C# 参考的四阶段为普通弹跳、原地高跳、前跳越障和弃杆步行。弹跳周期基准 80 tick；普通/高跳/前跳高度分别 40/50/90 px，最低视觉偏移 9 px。
- 上升保留 `BounceSlowMiddle` 重量感；持杆时 `GetSlowAnimFactor()` 返回 1，整段空中计时使用 `DeltaTime::GetDeltaTime()` 而非寒冰后的 `scaledTime`，避免弹跳动画、上升与浮空被减速。过顶点后以 2.0 倍推进，约 0.33 秒落地。前跳水平位移必须消费同一时间基准和下落倍率，否则寒冰时会落点缩水。弃杆时重算动画速度，恢复普通 0.6 倍寒冰动画倍率。
- 持杆时允许寒冰减速但不允许冻结或魅惑，也不触发土豆地雷。大嘴花因高度咬不到而不能吞下，`AdjustRejectedChomperBiteDamage()` 按当前持杆状态把拒吞伤害调整为 0；弃杆后恢复基类即时吞食与普通僵尸交互。高坚果按前跳约 10% 进度击落跳杆并开始啃食。
- 主人确认的动画全局帧：啃食 86/107，死亡 154，直接注册且不减一。
- 选卡与图鉴详情的预览实例不会进入基类的 `ZombieUpdate/ZombieMove`；`PogoZombie::Update()` 因此在调用 `Zombie::Update()` 后单独推进无位移、无碰撞、无落地声的原地弹跳。图鉴网格缩略图带 `mIsUI`，继续遵守场景的显式暂停。

## 表现与资源

- 手臂受损时换四张断臂贴图并发射 `PogoArmOff`；掉头同时折杆、隐藏头部/眼镜/头发，并用 `ZombiePogoHeadOff` 多 emitter 发射头与眼镜。
- 折杆粒子 `ZombiePogo` 使用 0.75 缩放。其 XML 局部偏移已经按僵尸逻辑原点配置，因此代码从 `GetPosition() - altitudeY` 发射；不得改回 `GetVisualPosition()`，否则会把 `mVisualOffset.y=-85` 重复叠加，杆子从头顶出现。
- 资源闭环包含 Pogo reanim、运行时换图、三种粒子 XML/贴图与 `pogo_zombie.ogg`；检查时须同时验证 loader 注册键和运行时 `HasReanimation`/纹理键。

## 存档与验证

- 存档保存阶段、剩余计时、高度、是否持杆、接触/前跳目标 ID、阻拦与落地事件标志、前跳总/已应用位移及动画播放态；读档后钳位并重建视觉终态。
- `smoke_pogo_zombie.json` 覆盖资源、陆路/水路兼容性、弹跳/越障/高坚果、啃食、断肢掉头、存档和地面植物交互。`smoke_fog_spawnlists_4_7_to_4_9.json` 锁定三关有序池，并以 `animatedObjectsByTag.Zombie.*.pogoAltitudeOn1000` 验证普通/精英选卡预览在动；`smoke_elite_pogo_almanac.json` 分别点开普通与精英详情并锁定弹跳状态。三份脚本于 2026-08-02 在主人桌面可见运行、exit 0，截图与 `run.log` 均正常。
