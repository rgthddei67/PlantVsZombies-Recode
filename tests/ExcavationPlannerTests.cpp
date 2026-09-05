#include "Game/Board/MineGrid.h"
#include <iostream>

/** 验证严格净收益、预留排除、绕行每步相邻/不入防守区，以及单格拆除后的路径收敛。 */
int main()
{
	MineGrid grid;
	grid.Initialize();
	std::array<bool,MineGrid::Count> excluded{};
	int wall = -1, stand = -1;
	if (!grid.FindExcavation(17,excluded,wall,stand) || wall != 11 || stand != 12) return 1;
	excluded[11] = true;
	if (grid.FindExcavation(17,excluded,wall,stand)) return 2;
	excluded.fill(false);
	// 施工导航允许沿已连通通道右行，且不会借道左侧两列折返。
	if (grid.NextWork(12,17) != 13 || grid.NextWork(1,17) != -1) return 7;
	// 单墙变动遍历所有起点，确保候选收益包括实际绕行成本。
	for (int removed = -1; removed < MineGrid::Count; ++removed) {
		MineGrid sample = grid;
		if (removed >= 0) sample.rock[removed] = false;
		sample.Rebuild();
		for (int start = 0; start < MineGrid::Count; ++start) {
			if (!sample.FindExcavation(start,excluded,wall,stand)) continue;
			if (start % MineGrid::Columns < 2 || !sample.rock[wall]) return 3;
			MineGrid opened = sample;
			opened.rock[wall] = false;
			opened.Rebuild();
			int cursor = start, steps = 0;
			while (cursor != stand && steps < MineGrid::Count) {
				const int next = sample.NextWork(cursor,stand);
				if (next < 0 || sample.rock[next] || !sample.connected[next]
					|| next % MineGrid::Columns < 2) return 4;
				const int dr = std::abs(next / MineGrid::Columns - cursor / MineGrid::Columns);
				const int dc = std::abs(next % MineGrid::Columns - cursor % MineGrid::Columns);
				if (dr + dc != 1) return 5;
				cursor = next; ++steps;
			}
			if (cursor != stand || steps + opened.distance[stand] >= sample.distance[start]) return 6;
		}
	}
	std::cout << "Excavation benefit, reservation and route boundaries passed\n";
	return 0;
}
