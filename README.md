# 植物大战僵尸 - 同人游戏

一个使用 C++ 和 SDL2、OpenGL 从零开始开发的《植物大战僵尸》同人游戏项目。使用自己封装的Graphics进行绘制。

## 功能特性

### 已实现的核心系统

- **动画系统** - 基于矩阵变换的动画系统
- **粒子特效系统** - 可配置的粒子系统，用于实现各种视觉特效
- **UI 控件系统** - 包含 Button、Slider、GameMessageBox 等较为完整的 UI 组件
- **碰撞检测系统** - 高效的碰撞检测和处理机制
- **对象池系统** - 对频繁创建的对象（子弹）进行优化内存分配和对象管理
- **音频系统** - 完整的音效和背景音乐支持
- **场景管理系统** - 灵活的场景切换和管理
- **存档系统** - 游戏进度保存和加载（还存在瑕疵）

### 游戏内容

- 植物系统
- 僵尸系统
- 子弹系统
- 阳光收集系统
- 卡片选择界面
- 关卡进度系统

## 项目结构

```
PlantVsZombies/
├── Game/                    # 游戏核心逻辑
│   ├── Plant/              # 植物相关类
│   ├── Zombie/             # 僵尸相关类
│   ├── Bullet/             # 子弹系统
│   ├── ObjectPool/         # 对象池
│   ├── Board.cpp/h         # 游戏主场景
│   ├── Card.cpp/h          # 卡片系统
│   ├── Coin.cpp/h          # 阳光/金币系统
│   └── ...
├── UI/                      # UI 系统
│   ├── Button.cpp/h        # 按钮控件
│   ├── Slider.cpp/h        # 滑块控件
│   ├── GameMessageBox.cpp/h # 消息框
│   └── ...
├── ParticleSystem/          # 粒子特效系统
├── Reanimation/             # 动画系统
│   ├── Reanimation.cpp/h   # 动画核心
│   ├── Animator.h          # 动画控制器
│   └── ...
├── Shader/                  # 着色器
├── Graphics.cpp/h           # 自研图形渲染引擎
├── ResourceManager.cpp/h    # 资源管理器
├── GameApp.cpp/h            # 游戏应用主类
└── main.cpp                 # 程序入口
```

## 构建说明

### 环境要求

- Windows 10 或更高版本
- Visual Studio 2022 或更高版本
- SDL2 库

### 构建步骤

1. 克隆仓库
```bash
git clone https://github.com/rgthddei67/PlantVsZombies.git
cd PlantsVsZombies
```

2. 打开 Visual Studio 解决方案
```
双击打开 PlantsVsZombies.sln
```

3. 配置 SDL2
   - 确保 SDL2 库已正确配置在项目中
   - 检查项目属性中的包含目录和库目录设置

4. 编译项目
   - 选择 Release 或 Debug 配置
   - 按 F7 或点击"生成解决方案"

5. 运行游戏
   - 按 F5 运行，或直接运行生成的 exe 文件

## 使用说明

### 启动游戏

- **普通模式**: 直接运行 `PlantsVsZombies.exe`
- **调试模式**: 使用命令行参数 `-Debug` 启动，将显示碰撞体等调试信息(Release模式基本没有信息)

### 系统架构

- **ECS 风格设计**: 使用 Component 组件化设计
- **对象池**: 优化频繁创建销毁的对象（如子弹、粒子）
- **资源管理**: 统一的资源加载和管理系统
- **场景管理**: 场景切换机制

## 开发计划

- [ ] 完善更多植物类型
- [ ] 添加更多僵尸类型
- [ ] 实现完整的关卡系统
- [ ] 优化性能和内存使用
- [ ] 添加更多游戏模式

## 致谢

- 感谢 PopCap Games 创造了经典的《植物大战僵尸》
- 感谢 SDL2 团队提供优秀的跨平台图形库
- 本项目仅供学习交流使用，不用于商业目的

## 联系方式

如有问题或建议，欢迎提交 Issue 或 Pull Request。

---
