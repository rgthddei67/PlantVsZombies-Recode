# TODO: 编译警告清零（2026-06-13 记，下次会话执行）

构建日志评估结论：无逻辑错误，全是代码卫生问题。按价值排序：

1. **`-Wnonportable-include-path`**：磁盘文件名 `GameAPP.h` 与 include 写法
   `#include "../GameApp.h"` 大小写不一致 → 把文件改名为 `GameApp.h`
   （与类名 GameApp 一致），全仓不用改 include（写法本来就是 GameApp.h）。
   注意 `git mv` 大小写改名在 Windows 需两步（先改临时名再改回）。
2. **MSVC C4477/C4267（CrashHandler.cpp:95/99/102）**：`%p` 收到 DWORD64 是 UB、
   size_t→DWORD 截断——崩溃处理路径最不该有 UB，cast 修正。
3. **`-Wreorder-ctor`（3 处）**：初始化列表按声明顺序重排（纯机械）：
   - `Game/ThreadPool.h:16`
   - `Game/ColliderComponent.h:57`
   - `ParticleSystem/ParticleXMLConfig.cpp:73`
4. **`-Wunused-const-variable`（GameMessageBox.cpp:16/19）**：删 `BUTTON_SPACING`、
   `BOTTOM_MARGIN` 两个死常量。

验证：三 preset 重建，clang-release 与 msvc-release 警告归零；
跑 `demo_peashooter.json` AutoTest（exit 0）确认行为不变。
