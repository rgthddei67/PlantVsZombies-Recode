#pragma once

/**
 * @brief 植物可阻拦的僵尸跳跃类别。
 *
 * 高坚果等植物通过 Plant::BlocksZombieJump 按类别声明能力，僵尸状态机无需依赖具体植物类型。
 */
enum class ZombieJumpType {
	POLEVAULT,
	DOLPHIN_RIDER,
	POGO,
};
