#pragma once
#ifndef _ANIMATIONTYPES_H
#define _ANIMATIONTYPES_H

// 新增植物/僵尸时，在此加一个动画类型枚举；完整新增步骤见
// Game/Plant/GameDataManager.cpp 的 InitializeHardcodedData 顶部说明。
enum class AnimationType
{
	ANIM_NONE,
	ANIM_SUN,

	ANIM_SUNFLOWER,
	ANIM_PEASHOOTER,
	ANIM_CHERRYBOMB,
	ANIM_WALLNUT,
	ANIM_POTATOMINE,
	ANIM_SNOWPEASHOOTER,
	ANIM_CHOMPER,
	ANIM_REPEAT,
	ANIM_PUFFSHROOM,
	ANIM_SUNSHROOM,
	ANIM_FUMESHROOM,
	// ANIM_GRAVEBUSTER,
	ANIM_HYPER,
	ANIM_SCAREDYSHROOM,
	ANIM_ICE_SHROOM,
	ANIM_ICEFUMESHROOM,
	ANIM_DOOM_SHROOM,
	ANIM_LILYPAD,

	ANIM_NORMAL_ZOMBIE,
	ANIM_CONE_ZOMBIE,
	ANIM_POLEVAULTER_ZOMBIE,
	ANIM_BUCKET_ZOMBIE,   // 铁通僵尸
	ANIM_PAPER_ZOMBIE,    // 报纸僵尸
	ANIM_DOOR_ZOMBIE,      // 铁门僵尸
	ANIM_FOOTBALL_ZOMBIE,  // 橄榄僵尸
	ANIM_PINK_FOOTBALL_ZOMBIE, // 粉色橄榄球僵尸
	ANIM_DANCE_ZOMBIE,     // 舞王僵尸
	ANIM_DANCERWITH_ZOMBIE, // 伴舞僵尸
	ANIM_ELITE_DANCE_ZOMBIE, // 精英舞王僵尸

	ANIM_ZOMBIE_CHARRED,

	ANIM_LAWNMOWER,
	ANIM_POOL_NORMAL_ZOMBIE,
	ANIM_POOL_CONE_ZOMBIE,
	ANIM_POOL_BUCKET_ZOMBIE,
	ANIM_POOL_CLEANER,
	ANIM_ELITE_SCAREDYSHROOM, // 复用 ScaredyShroom reanim；追加在末尾避免旧动画枚举值错位
	ANIM_SQUASH, // 经典倭瓜；追加在末尾避免旧动画枚举值错位
	ANIM_THREEPEATER, // 经典三线射手；追加在末尾避免旧动画枚举值错位
	ANIM_ELITE_POLEVAULTER_ZOMBIE, // 绿色精英撑杆；追加在末尾避免旧动画枚举值错位
	ANIM_TANGLEKELP, // 经典缠绕水草；追加在末尾避免旧动画枚举值错位
	ANIM_JALAPENO, // 经典火爆辣椒；追加在末尾避免旧动画枚举值错位
	ANIM_JALAPENO_FIRE, // 火爆辣椒整行火焰；非实体瞬时动画
	ANIM_ZAMBONI_ZOMBIE, // 经典冰车僵尸；追加在末尾避免旧动画枚举值错位
	ANIM_ZAMBONI_CHARRED, // 冰车专属灰烬残骸；第 53 帧回收
	ANIM_CALTROP, // 经典地刺；追加在末尾避免旧动画枚举值错位
	ANIM_GILDED_ZAMBONI_ZOMBIE, // 鎏金冰车独立黄色材质；复用普通冰车时间线
	ANIM_TORCHWOOD, // 经典火炬树桩；追加在末尾避免旧动画枚举值错位
	ANIM_FIREPEA, // 火炬树桩点燃后的循环动画子弹；非植物实体
	ANIM_DOLPHIN_RIDER_ZOMBIE, // 经典海豚僵尸；追加在末尾避免旧动画枚举值错位
	ANIM_TALLNUT, // 经典高坚果；追加在末尾避免旧动画枚举值错位
	ANIM_ELITE_DOLPHIN_RIDER_ZOMBIE, // 精英海豚独立粉白海豚与蓝色骑手材质
	ANIM_SEASHROOM, // 经典海蘑菇；追加在末尾避免旧动画枚举值错位
	ANIM_PLANTERN, // 经典路灯花；追加在末尾避免旧动画枚举值错位
	ANIM_JACK_IN_THE_BOX_ZOMBIE, // 经典小丑僵尸；追加在末尾避免旧动画枚举值错位
	ANIM_CACTUS, // 经典仙人掌；追加在末尾避免旧动画枚举值错位
	ANIM_BALLOON_ZOMBIE, // 经典气球僵尸；追加在末尾避免旧动画枚举值错位
	ANIM_BLOVER, // 经典三叶草；追加在末尾避免旧动画枚举值错位
	ANIM_ELITE_JACK_IN_THE_BOX_ZOMBIE, // 精英小丑独立午夜紫礼服与紫金盒子材质
	ANIM_SPLITPEA, // 经典双向射手；追加在末尾避免旧动画枚举值错位
	ANIM_DIGGER_ZOMBIE, // 经典矿工僵尸；追加在末尾避免旧动画枚举值错位
	ANIM_DIGGER_RISING_DIRT, // 矿工出土地块一次性动画
	ANIM_DIGGER_CHARRED, // 矿工专属灰烬动画（含镐/无镐两条轨道）
	ANIM_ZOMBIE_SURPRISE, // 地下丢镐后的问号一次性动画
	ANIM_STARFRUIT, // 经典杨桃；追加在末尾避免旧动画枚举值错位
	ANIM_ELITE_DIGGER_ZOMBIE, // 爆破工头独立工程警戒配色；复用普通矿工时间线
	ANIM_POGO_ZOMBIE, // 经典跳跳僵尸；追加在末尾避免旧动画枚举值错位
	ANIM_PUMPKIN, // 经典南瓜头；追加在末尾避免旧动画枚举值错位
	ANIM_MAGNETSHROOM, // 经典磁力菇；追加在末尾避免旧动画枚举值错位
};

#endif
