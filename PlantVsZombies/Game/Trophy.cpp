#include "Trophy.h"
#include "Board.h"
#include "../GameApp.h"
#include "GameScene.h"
#include "../DeltaTime.h"
#include "GameObjectManager.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../Graphics.h"

Trophy::Trophy(Board * board, const Vector & position)
	: Coin(board, AnimationType::ANIM_NONE, position,
		Vector(60, 60), Vector(0, 0),
		999.0f,
		BASE_SCALE, "Trophy", false, false)
{
	mCoinType = CoinType::COIN_TROPHY;
}

void Trophy::Start()
{
	AnimatedObject::Start();

	// 注册点击组件
	auto clickComponent = AddComponent<ClickableComponent>();
	if (clickComponent)
	{
		clickComponent->IsClickable = true;
		SetOnClickBack(clickComponent);
	}
}

void Trophy::SetOnClickBack(std::shared_ptr<ClickableComponent> click)
{
	if (!click) return;
	click->onClick = [this]() {
		if (mIsGrowing) return;

		mStartPosition = GetPosition();
		mTargetPosition = Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2);

		mIsGrowing = true;
		mIsMoving = true;
		mBoard->mBoardState = BoardState::WIN;
		AudioSystem::StopMusic();
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_WINMUSIC, 0.5f);
		auto& gameApp = GameAPP::GetInstance();
		gameApp.mGameInfoSaver.DeleteLevelData(mBoard);
		// 判断是否是冒险模式
		// TODO: 若以后增加小游戏，就改这里
		if (mBoard->mLevel <= 50) {
			gameApp.mAdventureLevel++;
			gameApp.mHaveCards.push_back(static_cast<PlantType>(mBoard->mLevel));
		}

		// 禁用点击，防止重复触发
		if (auto c = GetComponent<ClickableComponent>())
			c->IsClickable = false;
		};
}

void Trophy::Update()
{
	if (!mIsGrowing) {
		Coin::Update();
		return;
	}
	AnimatedObject::Update();

	mGrowTimer += DeltaTime::GetDeltaTime();
	float t = mGrowTimer / GROW_DURATION;
	if (t > 1.0f) t = 1.0f;
	// cubic ease-out：先快后慢
	float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
	SetScale(BASE_SCALE + (MAX_SCALE - BASE_SCALE) * eased);

	if (mIsMoving) {
		Vector newPos = Vector::lerp(mStartPosition, mTargetPosition, eased);
		SetPosition(newPos);
	}

	if (mGrowTimer >= GROW_DURATION) {
		if (mBoard && mBoard->mGameScene)
			mBoard->mGameScene->SetReadyToBackMenu();
		GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
	}
}

void Trophy::Draw(Graphics* g)
{
	auto transform = mTransform.lock();
	float scale = transform ? transform->GetScale() : 1.0f;
	Vector pos = GetPosition();

	if (mIsGrowing) {
		float t = mGrowTimer / GROW_DURATION;
		if (t > 1.0f) t = 1.0f;
		float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

		// 计算光圈大小和透明度
		float glowRadius = INITIAL_GLOW_RADIUS + (MAX_GLOW_RADIUS - INITIAL_GLOW_RADIUS) * eased;

		// 使用纹理绘制光圈（保持批处理）
		auto glowTex = ResourceManager::GetInstance().GetTexture(ResourceKeys::Textures::IMAGE_GLOWING);
		if (glowTex) {
			float glowSize = glowRadius * 2.0f;
			g->DrawTexture(glowTex,
				pos.x - glowSize * 0.5f,
				pos.y - glowSize * 0.5f,
				glowSize, glowSize,
				0.0f,
				glm::vec4(255.0f, 255.0f, 255.0f, 240.0f));
		}
	}

	// 绘制奖杯本体
	auto tex = ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_TROPHY);
	if (tex) {
		float w = tex->width * scale;
		float h = tex->height * scale;
		g->DrawTexture(tex, pos.x - w * 0.5f, pos.y - h * 0.5f, w, h);
	}
}