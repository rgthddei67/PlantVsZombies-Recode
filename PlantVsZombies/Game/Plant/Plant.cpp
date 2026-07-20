#include "Plant.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "../GameObjectManager.h"
#include "../ShadowComponent.h"
#include "GameDataManager.h"
#include "../../GameAPP.h"	// GameAPP::mShowPlantHP / Graphics / DrawText

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

		Vector cellCenterPosition(
			CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + CELL_COLLIDER_SIZE_X / 2,
			CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + CELL_COLLIDER_SIZE_Y / 2
		);
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
	if (mIsPreview) return;
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

	// 清理植物在Cell上的ID
	if (mBoard) {
		auto cell = mBoard->GetCell(mRow, mColumn);
		if (cell && cell->GetPlantID() == mPlantID) {
			cell->ClearPlantID();
		}
	}
	GameObjectManager::GetInstance().DestroyGameObject(this);
}

void Plant::Update()
{
	AnimatedObject::Update();   // 待机动画照常推进，让植物在选卡阶段仍"活着"
	// 仅在对战进行中(GAME)才跑行为逻辑：生存轮间选卡(CHOOSE_CARD)时场上保留的植物应冻结，
	// 否则向日葵会继续产阳光、射手继续计时等。WIN/LOSE 同理不再行动。
	if (!mIsPreview && !mIsSleeping &&
		mBoard && mBoard->mBoardState == BoardState::GAME) {
		PlantUpdate();
	}
}

Vector Plant::GetVisualPosition() const {
	return GetTransformComponent()->GetPosition() + mVisualOffset;
}

void Plant::PlantUpdate()
{
}

Vector Plant::GetPosition() const
{
	return GetTransformComponent()->GetPosition();
}

void Plant::SetPosition(const Vector& position)
{
	this->GetTransformComponent()->SetPosition(position);
}

void Plant::Draw(Graphics* g)
{
	AnimatedObject::Draw(g);	// 先画本体动画

	if (!g || mIsPreview || !GameAPP::GetInstance().mShowPlantHP) return;
	// 视口剔除：屏外植物不画血量文字（与 Zombie::Draw 同构，省 batch VBO + CPU）。
	if (!g->IsWorldPointVisible(GetPosition().x, GetPosition().y)) return;

	// 直接用逻辑坐标：DrawText 与 Animator 的 DrawTextureMatrix 共享同一 projView，
	// Animator 画 sprite 时就是用裸逻辑坐标，文字必须同坐标系才能叠在对象上（勿转 World）
	Vector pos = GetPosition() + Vector(-21, -11);

	std::string text = std::to_string(mPlantHealth) + u8"/" + std::to_string(mPlantMaxHealth);
	// 颜色是 0..255 范围（ToSDLColor 直接 static_cast，不乘 255），勿写成 0..1 否则全透明隐形
	const glm::vec4 green(0.0f, 255.0f, 0.0f, 255.0f);
	g->DrawGlyphRun(text, ResourceKeys::Fonts::FONT_FZCQ, 17, green, pos.x, pos.y);
}
