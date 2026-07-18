#include "Shooter.h"
#include "GameDataManager.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"

void Shooter::SetupPlant() {
	Plant::SetupPlant();

	auto reanim = mAnimator->GetReanimation();
	if (!reanim) return;

	mAnimator->PlayTrack("anim_idle");

	// 1. 创建头部动画器
	mHeadAnim = std::make_shared<Animator>(reanim);
	mHeadAnim->SetSpeed(this->GetAnimationSpeed());   // 同步身体动画速度
	mHeadAnim->PlayTrack("anim_head_idle");
	mHeadAnim->SetLocalPosition(GameDataManager::GetInstance().
		GetPlantOffset(this->mPlantType));

	// 2. 将头部附加到身体轨道
	if (!mAnimator->GetTracksByName("anim_stem").empty()) {
		mAnimator->AttachAnimator("anim_stem", mHeadAnim);
	}

	mAnimator->SetSpeed(GameRandom::Range(1.1f, 1.3f));

	mHeadAnim->AddFrameEvent(64, [this]() {
		if (GameRandom::Chance())
		{
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.3f);
		}
		else {
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
		}
		this->ShootBullet();
		}, true);
}

void Shooter::SaveExtraData(nlohmann::json& j) const
{
	j["shootTimer"] = mShootTimer;
	if (!mHeadAnim) return;

	// 头部 Animator 不属于 AnimatedObject 的主 Animator，必须在额外数据里独立保存整台
	// 播放状态机；只保存轨道/帧会令 PlayTrackOnce 射击在读档后退化为永久循环。
	j["headAnimTrack"] = mHeadAnim->GetCurrentTrackName();
	j["headAnimFrame"] = mHeadAnim->GetCurrentFrame();
	j["headAnimSpeed"] = mHeadAnim->GetSpeed();
	j["headAnimClipSpeed"] = mHeadAnim->GetClipSpeed();
	j["headAnimPlayState"] = static_cast<int>(mHeadAnim->GetPlayingState());
	j["headAnimTargetTrack"] = mHeadAnim->GetTargetTrack();
	j["headAnimTargetTrackSpeed"] = mHeadAnim->GetTargetTrackSpeed();
	j["headAnimPlaying"] = mHeadAnim->IsPlaying();
}

void Shooter::LoadExtraData(const nlohmann::json& j)
{
	mShootTimer = j.value("shootTimer", 1.0f);
	if (!mHeadAnim) return;

	const std::string track = j.value("headAnimTrack", std::string{});
	if (track.empty()) return;

	const float clipSpeed = j.value("headAnimClipSpeed", 0.0f);
	if (j.contains("headAnimPlayState")) {
		const int rawState = j.value("headAnimPlayState", static_cast<int>(PlayState::PLAY_REPEAT));
		const PlayState state = (rawState >= static_cast<int>(PlayState::PLAY_NONE)
			&& rawState <= static_cast<int>(PlayState::PLAY_ONCE_TO))
			? static_cast<PlayState>(rawState) : PlayState::PLAY_REPEAT;

		if (state == PlayState::PLAY_ONCE || state == PlayState::PLAY_ONCE_TO) {
			mHeadAnim->PlayTrackOnce(track,
				j.value("headAnimTargetTrack", std::string{}), clipSpeed, 0.0f,
				j.value("headAnimTargetTrackSpeed", 0.0f));
		}
		else {
			mHeadAnim->PlayTrack(track, clipSpeed);
		}
	}
	else if (track == "anim_shooting") {
		// 旧 Shooter 存档没有播放状态。射击轨道在所有 Shooter 中都只应播放一次；宁可
		// 回到 idle，也不能沿用旧逻辑的 PlayTrack() 把持久帧事件变成无限吐弹。
		mHeadAnim->PlayTrackOnce(track, "anim_head_idle", clipSpeed);
	}
	else {
		mHeadAnim->PlayTrack(track, clipSpeed);
	}

	if (j.contains("headAnimSpeed"))
		mHeadAnim->SetSpeed(j.value("headAnimSpeed", 1.0f));
	mHeadAnim->SetCurrentFrame(j.value("headAnimFrame", 0.0f));
	if (j.contains("headAnimPlaying") && !j.value("headAnimPlaying", true))
		mHeadAnim->Pause();
}

void Shooter::PlantUpdate()
{
	// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0，自动 no-op）。
	float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;
	this->mShootTimer += (DeltaTime::GetDeltaTime() * mult);
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			// 动画同比例加快：吐弹的第 64 帧 frame event 跟上更短间隔
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 1.5f * mult, 0.2f);
		}
	}
}

bool Shooter::HasZombieInRow()
{
	if (mBoard)
	{
		mCheckZombieTimer += DeltaTime::GetDeltaTime();
		if (mCheckZombieTimer >= 0.6f)
		{
			mCheckZombieTimer = 0.0f;
			// 按行索引：只遍历本行僵尸，mRow 过滤已由桶保证。
			const float thisX = GetPosition().x;
			bool found = false;
			mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
				if (found) return;  // 已命中，跳过本行其余
				float zombieX = zombie->GetPosition().x;
				if (!zombie->IsMindControlled() && zombieX >= thisX && zombieX <= SCENE_WIDTH && zombie->HasHead())
					found = true;
			});
			return found;
		}
	}
	return false;
}
