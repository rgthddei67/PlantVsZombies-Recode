#include "Plant.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "../GameObjectManager.h"
#include "../ShadowComponent.h"
#include "GameDataManager.h"
#include "../../GameAPP.h"	// GameAPP::mShowPlantHP / Graphics / DrawText
#include <cmath>

namespace {
	constexpr float kPoolBobAmplitude = 2.0f;              // 水面植物上下浮动振幅（像素）
	constexpr float kPoolBobRadiansPerFrame = 3.14159265f / 60.0f; // 60Hz 下两秒一个周期
	constexpr float kPoolBobRowPhase = 3.14159265f;        // 相邻行交错半个周期
	constexpr float kPoolBobColumnPhase = 3.14159265f / 4.0f; // 相邻列相差八分之一周期
	constexpr float kSquishScaleY = 0.5f;                  // C# ratioSquished：纵向压到一半，横向保持原宽
	constexpr float kSquishDurationSeconds = 5.0f;         // 主人调短的压扁残影总保留时间，单位：秒
	constexpr float kSquishFadeSeconds = 1.0f;             // 保持 C# 末段占总时长 20% 的线性渐隐比例
	constexpr float kDefaultSquishPivotOffsetY = 100.0f;   // 无 Board 的预防性回退；正式关卡使用当前地图格高
}

Plant::Plant(Board* board, PlantType plantType, int row, int column,
	AnimationType animType, float scale, bool isPreview)
	: AnimatedObject(ObjectType::OBJECT_PLANT, board,
		Vector(0, 0), // 位置会在后面计算
		animType,
		ColliderType::BOX,
		Vector(65, 65),
		Vector(-30, -30),
		scale,
		"Plant",
		false)
{
	mBoard = board;
	mPlantType = plantType;
	mRow = row;
	mColumn = column;
	mIsPreview = isPreview;
	// mIsSleeping / mPlantHealth / mPlantMaxHealth 由头文件就地初始化（false / 300 / 300）

	GameDataManager& plantMgr = GameDataManager::GetInstance();

	mVisualOffset = plantMgr.GetPlantOffset(plantType);
	auto shadowcomponent = AddComponent<ShadowComponent>
		(ResourceManager::GetInstance().GetTexture
		(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
	shadowcomponent->SetDrawOrder(-80);

	// 设置植物在格子中的位置
	if (!mIsPreview) {
		if (auto collider = GetColliderComponent()) {
			collider->isStatic = true;
			collider->layerMask = CollisionLayer::PLANT;
			collider->collisionMask = CollisionLayer::ZOMBIE;
		}

		Vector cellCenterPosition = mBoard
			? mBoard->GetCellCenterPosition(row, column)
			: Vector(CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + CELL_COLLIDER_SIZE_X / 2,
				CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + CELL_COLLIDER_SIZE_Y / 2);
		SetPosition(cellCenterPosition);  // 逻辑位置
	}
	else {
		SetPosition(Vector(-512, -512));
	}
}

void Plant::SetupPlant()
{
}

void Plant::Start()
{
	GameObject::Start();
	if (this->mIsPreview) {
		RemoveComponent<ColliderComponent>();
		mCollider = nullptr;  // 缓存的裸指针随之失效，显式置空
	}

	this->PlayTrack("anim_idle");
	this->SetupPlant();
}

void Plant::TakeDamage(int damage, DamageSource source) {
	if (mIsPreview || mIsSquished) return;
	// 僵尸增伤只放大僵尸来源；植物韧性则对所有实际承伤生效。两者均在 0 层返回单位元。
	int scaledDamage = damage;
	if (mBoard) {
		if (source == DamageSource::ZOMBIE) {
			scaledDamage = mBoard->GetPerkManager().ScaleZombieDamage(scaledDamage);
		}
		scaledDamage = mBoard->GetPerkManager().ScaleDamageToPlant(scaledDamage);
	}
	mPlantHealth -= scaledDamage;
	SetGlowingTimer(0.1f);
	if (mPlantHealth <= 0) {
		Die();
	}
}

void Plant::Die() {
	StopAnimation();

	// 禁用碰撞体
	if (mCollider) {
		mCollider->mEnabled = false;
	}

	ReleaseGridSlot();
	GameObjectManager::GetInstance().DestroyGameObject(this);
}

void Plant::Update()
{
	AnimatedObject::Update();   // 待机动画照常推进，让植物在选卡阶段仍"活着"
	if (mIsSquished) {
		if (!mIsPreview && mBoard && mBoard->mBoardState == BoardState::GAME) {
			UpdateSquish();
		}
		return;
	}
	UpdateGridMoveVisual();
	// 仅在对战进行中(GAME)才跑行为逻辑：生存轮间选卡(CHOOSE_CARD)时场上保留的植物应冻结，
	// 否则向日葵会继续产阳光、射手继续计时等。WIN/LOSE 同理不再行动。
	if (!mIsPreview && !mIsSleeping &&
		mBoard && mBoard->mBoardState == BoardState::GAME) {
		PlantUpdate();
	}
}

Vector Plant::GetVisualPosition() const {
	if (mIsSquished) return mSquishVisualPosition;

	Vector visual = GetTransformComponent()->GetPosition() + mVisualOffset + mGridMoveVisualOffset;
	if (!mIsPreview && mBoard && mBoard->IsPoolRow(mRow)) {
		const float phase = static_cast<float>(mBoard->mBoardFrame) * kPoolBobRadiansPerFrame
			+ static_cast<float>(mRow) * kPoolBobRowPhase
			+ static_cast<float>(mColumn) * kPoolBobColumnPhase;
		visual.y += std::sin(phase) * kPoolBobAmplitude;
	}
	return visual;
}

void Plant::PlantUpdate()
{
}

void Plant::Squish()
{
	if (mIsPreview || mIsSquished) return;

	// 必须在置位前采样；GetVisualPosition() 在压扁态会直接返回冻结坐标。
	mSquishVisualPosition = GetVisualPosition();
	mIsSquished = true;
	mSquishTimer = kSquishDurationSeconds;
	ApplySquishedPresentation();

	// C# FoleyType.Squish 在 SOUND_CHOMP / SOUND_CHOMP2 中随机选一个。
	AudioSystem::PlaySound(GameRandom::Chance()
		? ResourceKeys::Sounds::SOUND_ZOMBIE_EAT
		: ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.3f);
}

void Plant::RestoreSquishState(float remainingSeconds, const Vector& visualPosition)
{
	if (mIsPreview) return;
	mIsSquished = true;
	mSquishTimer = std::max(0.0f, remainingSeconds);
	mSquishVisualPosition = visualPosition;
	ApplySquishedPresentation();
	if (mSquishTimer <= 0.0f) {
		Die();
	}
}

void Plant::ApplySquishedPresentation()
{
	PauseAnimation();
	const float pivotOffsetY = mBoard ? mBoard->GetCellHeight() : kDefaultSquishPivotOffsetY;
	if (mAnimator) {
		// 根暂停已经能阻止子树更新；显式递归暂停还会同步每个子 Animator 的播放状态。
		mAnimator->PauseSubtree();
		mAnimator->SetRenderScale(1.0f, kSquishScaleY,
			mSquishVisualPosition.x, mSquishVisualPosition.y + pivotOffsetY);
	}
	if (mCollider) {
		mCollider->mEnabled = false;
	}
	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetVisible(false);
	}
	ReleaseGridSlot();

	const float alpha = kSquishFadeSeconds > 0.0f
		? std::clamp(mSquishTimer / kSquishFadeSeconds, 0.0f, 1.0f)
		: 1.0f;
	SetAlpha(alpha);
}

void Plant::ReleaseGridSlot()
{
	if (!mBoard) return;
	auto cell = mBoard->GetCell(mRow, mColumn);
	if (cell && cell->GetUnderPlantID() == mPlantID) {
		cell->ClearUnderPlantID();
	}
	if (cell && cell->GetNormalPlantID() == mPlantID) {
		cell->ClearNormalPlantID();
	}
}

void Plant::UpdateSquish()
{
	mSquishTimer = std::max(0.0f, mSquishTimer - DeltaTime::GetDeltaTime());
	if (mSquishTimer <= 0.0f) {
		Die();
		return;
	}
	SetAlpha(std::clamp(mSquishTimer / kSquishFadeSeconds, 0.0f, 1.0f));
}

float Plant::GetWeatherActionSpeedMultiplier() const
{
	return mBoard ? mBoard->GetPlantRainActionSpeedMultiplier() : 1.0f;
}

float Plant::GetWeatherActionDeltaTime() const
{
	return DeltaTime::GetDeltaTime() * GetWeatherActionSpeedMultiplier();
}

float Plant::GetAttackSpeedMultiplier() const
{
	const float perkMultiplier = mBoard
		? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier())
		: 1.0f;
	return perkMultiplier * GetWeatherActionSpeedMultiplier();
}

Vector Plant::GetPosition() const
{
	return GetTransformComponent()->GetPosition();
}

void Plant::SetPosition(const Vector& position)
{
	this->GetTransformComponent()->SetPosition(position);
}

void Plant::MoveToGridCell(int row, int column, float visualDuration)
{
	// 逻辑格和碰撞箱必须在同一帧落到目标格；旧画面位置只作为瞬态绘制偏移保留。
	const Vector currentVisualBase = GetPosition() + mGridMoveVisualOffset;
	const Vector target = mBoard
		? mBoard->GetCellCenterPosition(row, column)
		: Vector(CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + CELL_COLLIDER_SIZE_X / 2,
			CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + CELL_COLLIDER_SIZE_Y / 2);
	mRow = row;
	mColumn = column;
	SetPosition(target);

	mGridMoveVisualStart = currentVisualBase - target;
	mGridMoveVisualOffset = mGridMoveVisualStart;
	mGridMoveVisualDuration = std::max(0.0f, visualDuration);
	mGridMoveVisualTimer = mGridMoveVisualDuration;
	if (mGridMoveVisualDuration <= 0.0f) {
		mGridMoveVisualStart = Vector(0.0f, 0.0f);
		mGridMoveVisualOffset = Vector(0.0f, 0.0f);
	}
}

void Plant::UpdateGridMoveVisual()
{
	if (mGridMoveVisualTimer <= 0.0f || mGridMoveVisualDuration <= 0.0f) return;
	mGridMoveVisualTimer = std::max(0.0f,
		mGridMoveVisualTimer - DeltaTime::GetDeltaTime());
	const float linear = std::clamp(1.0f
		- mGridMoveVisualTimer / mGridMoveVisualDuration, 0.0f, 1.0f);
	const float eased = linear * linear * (3.0f - 2.0f * linear);
	mGridMoveVisualOffset = mGridMoveVisualStart * (1.0f - eased);
	if (mGridMoveVisualTimer <= 0.0f) {
		mGridMoveVisualStart = Vector(0.0f, 0.0f);
		mGridMoveVisualOffset = Vector(0.0f, 0.0f);
	}
}

void Plant::Draw(Graphics* g)
{
	if (mIsSquished) {
		// 非等比缩放已烘进 Animator 的快/慢绘制路径；这里只抑制压扁态血量文字。
		AnimatedObject::Draw(g);
		return;
	}

	AnimatedObject::Draw(g);	// 先画本体动画

	if (!g || mIsPreview || !GameAPP::GetInstance().mShowPlantHP) return;
	// 视口剔除：屏外植物不画血量文字（与 Zombie::Draw 同构，省 batch VBO + CPU）。
	if (!g->IsWorldPointVisible(GetPosition().x, GetPosition().y)) return;

	// 直接用逻辑坐标：DrawText 与 Animator 的 DrawTextureMatrix 共享同一 projView，
	// Animator 画 sprite 时就是用裸逻辑坐标，文字必须同坐标系才能叠在对象上（勿转 World）
	// 血条属于画面反馈，随台风平滑位移；碰撞箱仍使用已经落在目标格的 Transform。
	Vector pos = GetGridVisualPosition() + Vector(-21, -11);

	std::string text = std::to_string(mPlantHealth) + u8"/" + std::to_string(mPlantMaxHealth);
	// 颜色是 0..255 范围（ToSDLColor 直接 static_cast，不乘 255），勿写成 0..1 否则全透明隐形
	const glm::vec4 green(0.0f, 255.0f, 0.0f, 255.0f);
	g->DrawGlyphRun(text, ResourceKeys::Fonts::FONT_FZCQ, 17, green, pos.x, pos.y);
}
