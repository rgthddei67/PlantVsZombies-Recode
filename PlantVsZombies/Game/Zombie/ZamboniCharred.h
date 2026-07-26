#pragma once

#include "ZombieCharred.h"

/**
 * @brief 冰车专属灰烬残骸，按主人指定在全局第 53 帧回收。
 */
class ZamboniCharred final : public ZombieCharred {
public:
	using ZombieCharred::ZombieCharred;

protected:
	int GetRemovalFrame() const override { return 53; }
};
