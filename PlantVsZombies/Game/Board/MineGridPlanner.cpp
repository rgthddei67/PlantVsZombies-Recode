#include "MineGrid.h"
#include <tuple>

namespace {
	constexpr std::array<std::array<int, 2>, 4> kDirections{{{0,-1},{-1,0},{1,0},{0,1}}}; // 选墙/行走并列按左、上、下、右
}

std::array<int, MineGrid::Count> MineGrid::WorkDistances(int start) const
{
	std::array<int, Count> result;
	result.fill(Unreachable);
	if (start < 0 || start >= Count || start % Columns < 2 || rock[start] || !connected[start]) return result;
	std::array<int, Count> queue{};
	int head = 0, tail = 0;
	queue[tail++] = start;
	result[start] = 0;
	while (head < tail) {
		const int cell = queue[head++];
		for (const auto& d : kDirections) {
			const int r = cell / Columns + d[0], c = cell % Columns + d[1];
			if (!Valid(r,c) || c < 2) continue;
			const int next = Index(r,c);
			if (rock[next] || !connected[next] || result[next] != Unreachable) continue;
			result[next] = result[cell] + 1;
			queue[tail++] = next;
		}
	}
	return result;
}

bool MineGrid::FindExcavation(int start, const std::array<bool, Count>& excluded, int& wall, int& stand) const
{
	wall = stand = -1;
	if (start < 0 || start >= Count || distance[start] >= Unreachable) return false;
	const auto walk = WorkDistances(start);
	std::tuple<int,int,int,int> best{0,0,0,0};
	for (int target = 0; target < Count; ++target) {
		if (excluded[target] || !CanExcavate(target / Columns, target % Columns)) continue;
		MineGrid opened = *this;
		opened.rock[target] = false;
		opened.Rebuild();
		for (int direction = 0; direction < 4; ++direction) {
			const int r = target / Columns - kDirections[direction][0];
			const int c = target % Columns - kDirections[direction][1];
			if (!Valid(r,c)) continue;
			const int work = Index(r,c);
			if (walk[work] >= Unreachable || opened.distance[work] >= Unreachable) continue;
			const int gain = distance[start] - walk[work] - opened.distance[work];
			const auto rank = std::make_tuple(-gain,direction,target,work);
			if (gain <= 0 || (wall >= 0 && rank >= best)) continue;
			best = rank; wall = target; stand = work;
		}
	}
	return wall >= 0;
}

int MineGrid::NextWork(int start, int stand) const
{
	if (start < 0 || start >= Count) return -1;
	const auto distances = WorkDistances(stand);
	if (distances[start] == 0 || distances[start] >= Unreachable) return -1;
	for (const auto& d : kDirections) {
		const int r = start / Columns + d[0], c = start % Columns + d[1];
		if (!Valid(r,c)) continue;
		const int next = Index(r,c);
		if (distances[next] + 1 == distances[start]) return next;
	}
	return -1;
}
