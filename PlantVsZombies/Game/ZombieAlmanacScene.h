#pragma once
#ifndef _ZOMBIEALMANAC_SCENE_H
#define _ZOMBIEALMANAC_SCENE_H

#include "Scene.h"
#include "Zombie/ZombieType.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <SDL2/SDL_ttf.h>

class Zombie;

class ZombieAlmanacScene : public Scene {
private:
	std::shared_ptr<Button> mBackMenuButton;
	std::shared_ptr<Button> mPreviousPageButton;
	std::shared_ptr<Button> mNextPageButton;
	std::vector<std::weak_ptr<Zombie>> mGridZombies;
	std::vector<Vector> mGridPositions;
	std::vector<ZombieType> mDisplayedZombieTypes;
	std::weak_ptr<Zombie> mPreviewZombie;
	std::vector<std::weak_ptr<Zombie>> mPreviewZombieMembers;

	ZombieType mCurrentZombieType = ZombieType::NUM_ZOMBIE_TYPES;
	int mCurrentPage = 0;
	std::unordered_map<std::string, std::string> mInfoMap;
	std::string mCurrentZombieName;
	float mZombieNameX = 0.0f;
	std::vector<std::string> mDescriptionLines;
	int   mDescriptionFontSize = 17;     // 自动收缩后的描述字号
	float mDescriptionLineHeight = 22.0f; // 与字号等比的行高

	/** 合并已通关 spawnlist 与永久实际遭遇记录，按首次推导顺序返回可见僵尸。 */
	std::vector<ZombieType> LoadEncounteredZombieTypes() const;
	/** 为已遭遇类型创建当前页的可点击图鉴网格和裁剪预览。 */
	void CreateAllZombieEntries();
	/** 销毁当前页的纯 UI 网格实体，避免翻页后遗留碰撞和绘制。 */
	void DestroyGridZombieEntries();
	/** 按当前页索引重建左侧网格；右侧详情预览保持不变。 */
	void CreateCurrentPageZombieEntries();
	/** 同步上一页和下一页按钮的可见性与朝向。 */
	void RefreshPageButtonState();
	/** 前往上一页，并重建新的网格实体。 */
	void GoToPreviousPage();
	/** 前往下一页，并重建新的网格实体。 */
	void GoToNextPage();
	void OnZombieClicked(ZombieType type);
	/** 创建详情窗预览；编队类型会同时创建全部纯展示成员。 */
	void CreatePreviewZombie(ZombieType type);
	/** 销毁详情窗当前预览组，避免切换条目后遗留编队成员。 */
	void DestroyPreviewZombie();
	void LoadInfoFile();
	void UpdateZombieInfo(ZombieType type);
	std::vector<std::string> WrapText(const std::string& text,
		float startX, float maxX, float wrapX,
		const std::string& fontKey, int fontSize);

public:
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

	/** 返回当前进度下实际展示的僵尸类型，顺序为冒险模式首次遭遇顺序。 */
	const std::vector<ZombieType>& GetDisplayedZombieTypes() const {
		return mDisplayedZombieTypes;
	}
	/** 返回当前图鉴页的 0-based 索引。 */
	int GetCurrentPage() const { return mCurrentPage; }
	/** 返回由当前已解锁条目数派生的总页数。 */
	int GetPageCount() const;
	/** 返回当前页实际显示的条目类型，顺序与左侧网格一致。 */
	std::vector<ZombieType> GetCurrentPageZombieTypes() const;
	/** 返回当前页指定类型的网格预览实例；未显示或创建失败时返回 nullptr。 */
	Zombie* GetGridZombiePreview(ZombieType type) const;
	/** 返回上一页按钮，供 AutoTest 以真实输入路径解析其中心。 */
	std::shared_ptr<Button> GetPreviousPageButton() const { return mPreviousPageButton; }
	/** 返回下一页按钮，供 AutoTest 以真实输入路径解析其中心。 */
	std::shared_ptr<Button> GetNextPageButton() const { return mNextPageButton; }

	/** 返回右侧详情当前选中的僵尸；空图鉴返回 NUM_ZOMBIE_TYPES。 */
	ZombieType GetCurrentZombieType() const { return mCurrentZombieType; }
	/** 返回右侧详情已解析的标题；仅供图鉴状态取证。 */
	const std::string& GetCurrentZombieName() const { return mCurrentZombieName; }
	/** 返回右侧详情已排版的说明行数；缺失 info 文本时为零。 */
	std::size_t GetDescriptionLineCount() const { return mDescriptionLines.size(); }
	/** 返回右侧详情预览实例；仅供只读状态取证。 */
	Zombie* GetPreviewZombie() const {
		const auto zombie = mPreviewZombie.lock();
		return zombie.get();
	}
	/** 返回详情窗当前展示的全部实体；编队类型可包含多个仅表现成员。 */
	const std::vector<std::weak_ptr<Zombie>>& GetPreviewZombieMembers() const {
		return mPreviewZombieMembers;
	}

	bool mReadyToSwitchAlmanacScene = false;

protected:
	void BuildDrawCommands() override;
};

#endif
