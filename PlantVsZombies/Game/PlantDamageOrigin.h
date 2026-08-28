#pragma once

#include <cstdint>

#include "Plant/PlantUpgradeRules.h"

/** 植物伤害的语义来源；弹种变化与命中表现不得改写该值。 */
enum class PlantDamageOriginKind : std::uint8_t {
	NONE,
	PLANT_LINEAGE,
	ASH,
};

/**
 * 记录原发射植物谱系或统一灰烬类别，供需要按植物身份判断的目标使用。
 * 普通伤害系统仍由 DamageSource 区分阵营；无须识别谱系的目标不会承担额外分支。
 */
struct PlantDamageOrigin {
	PlantDamageOriginKind kind = PlantDamageOriginKind::NONE;
	PlantType lineage = PlantType::NUM_PLANT_TYPES;

	/** 把基础植物和紫卡升级归并为同一双向免疫谱系，不改变紫卡种植资格。 */
	static constexpr PlantDamageOrigin FromPlant(PlantType type)
	{
		PlantType baseType = GetUpgradeBasePlantType(type);
		// 尚未实装的经典紫卡枚举也先固定语义关系；不能写入 PlantUpgradeRules，
		// 否则会意外开放它们的覆盖种植资格。
		if (baseType == PlantType::NUM_PLANT_TYPES) {
			switch (type) {
			case PlantType::PLANT_GATLINGPEA:
				baseType = PlantType::PLANT_REPEATER;
				break;
			case PlantType::PLANT_CATTAIL:
				baseType = PlantType::PLANT_LILYPAD;
				break;
			case PlantType::PLANT_SPIKEROCK:
				baseType = PlantType::PLANT_SPIKEWEED;
				break;
			default:
				break;
			}
		}
		return {
			PlantDamageOriginKind::PLANT_LINEAGE,
			baseType == PlantType::NUM_PLANT_TYPES ? type : baseType,
		};
	}

	/** 所有真正的灰烬植物共享一个语义类别。 */
	static constexpr PlantDamageOrigin Ash()
	{
		return { PlantDamageOriginKind::ASH, PlantType::NUM_PLANT_TYPES };
	}

	constexpr bool IsValid() const
	{
		return kind == PlantDamageOriginKind::ASH
			|| (kind == PlantDamageOriginKind::PLANT_LINEAGE
				&& lineage >= PlantType::PLANT_PEASHOOTER
				&& lineage < PlantType::NUM_PLANT_TYPES);
	}

	constexpr bool operator==(const PlantDamageOrigin& other) const
	{
		return kind == other.kind && lineage == other.lineage;
	}
};
