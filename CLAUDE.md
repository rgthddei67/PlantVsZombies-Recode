我正在进行一个使用 SDL2 和自制的 Graphics 类开发的《植物大战僵尸1》同人游戏。目前项目已完成了基础框架，并实现了以下系统：

动画系统（使用矩阵来转换）

粒子特效系统

控件系统（Button、Slider、GameMessageBox等 UI 元素）

碰撞系统

我的代码风格偏好：

不喜欢使用现代 C++ 的高级特性，例如模板（template）、C++20 及之后的复杂语法。但可以接受智能指针（std::unique_ptr, std::shared_ptr std::weak_ptr）和 std::vector 、std::unordered_map等这类实用且易懂的容器。

追求代码的高效、简单易懂、无 bug，逻辑清晰，避免过度设计。

类型转换：大部分情况下使用 static_cast，只有在少数场景才会考虑C风格的转换。

请你给我修改代码的时候遵循上述代码风格且编辑、读取文件使用UTF8编码，谢谢！