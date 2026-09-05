#pragma once

#include <array>
#include <cstdint>

/** 幽晶矿场的纯地形数据；反向最短路只包含左/上/下合法边，不把植物计为障碍。 */
class MineGrid {
public:
	static constexpr int Rows = 5;
	static constexpr int Columns = 9;
	static constexpr int Count = Rows * Columns;
	static constexpr int Unreachable = 1000;
	std::array<bool, Count> rock{};
	std::array<int, Count> distance{};
	std::array<int, Count> exitDistance{};
	std::array<bool, Count> connected{};
	std::array<bool, Rows> entrance{};

	static bool Valid(int row, int col) { return row >= 0 && row < Rows && col >= 0 && col < Columns; }
	static int Index(int row, int col) { return row * Columns + col; }
	bool IsRock(int row, int col) const { return Valid(row, col) && rock[Index(row, col)]; }
	/** 初始化9-1的两条独立天然通路；其他关卡由后续关卡设计提供布局。 */
	void Initialize();
	/** 地形提交后重建房屋连通性与有向距离；固定数组队列，无每帧分配。 */
	void Rebuild();
	bool CanExcavate(int row, int col) const;
	/** 返回严格更近的相邻格下标，或-1；并列左优先、上下按稳定偏好。 */
	int Next(int cell, bool preferDown) const;
	/** 魅惑单位沿四邻接矿道返回任一右侧入口；并列优先向右，距离严格下降。 */
	int NextExit(int cell, bool preferDown) const;
	/** 检查路径图所有可达格的下降性质，供存档与自动验收使用。 */
	bool Validate() const;
};
