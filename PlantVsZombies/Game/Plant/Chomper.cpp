#include "Chomper.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"
#include <cfloat>

int Chomper::FindTargetZombieID()
{
	if (!mBoard) return NULL_ZOMBIE_ID;

	int   closestID = NULL_ZOMBIE_ID;
	float closestDistance = FLT_MAX;
	Vector myPos = GetPosition();

	// 按行索引：只遍历本行僵尸，mRow 过滤已由桶保证。
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* z) {
		if (z->IsMindControlled()) return;
		if (!z->HasHead()) return;

		float dx = z->GetPosition().x - myPos.x;
		if (dx < 0.0f) return;
		if (dx > CHOMP_RANGE_X) return;

		if (dx < closestDistance) {
			closestDistance = dx;
			closestID = z->mZombieID;
		}
	});
	return closestID;
}

void Chomper::StartBite(int zombieID)
{
	mState = State::BITING;
	mTargetZombieID = zombieID;

	PlayTrackOnce("anim_bite", "anim_chew", 1.4f * GetWeatherActionSpeedMultiplier(), 0.05f);
	mAnimator->AddFrameEvent(BITE_KILL_FRAME, [this]() {
		OnBiteKillFrame();
		});
}

void Chomper::OnBiteKillFrame()
{
	if (mBoard) {
		if (auto* z = mBoard->mEntityManager.GetZombie(mTargetZombieID)) {
			// 直杀统一走僵尸入口：普通目标保持原行为，特殊目标可把这次咬杀降级为数值伤害。
			z->TakePlantInstantKill();
		}
	}
	mTargetZombieID = NULL_ZOMBIE_ID;
	AudioSystem::PlaySound("SOUND_CHOMPPLANT", 0.6f);

	mState = State::DIGESTING;
	mDigestTimer = 0.0f;
}

void Chomper::EndDigest()
{
	mState = State::IDLE;
	mDigestTimer = 0.0f;
	PlayTrackOnce("anim_swallow", "anim_idle", 1.2f * GetWeatherActionSpeedMultiplier(), 0.05f);
}

void Chomper::PlantUpdate()
{
	float dt = DeltaTime::GetDeltaTime();
	switch (mState) {
	case State::IDLE:
		mDetectionTimer += dt;
		if (mDetectionTimer >= DETECTION_INTERVAL) {
			mDetectionTimer = 0.0f;
			int targetID = FindTargetZombieID();
			if (targetID != NULL_ZOMBIE_ID) {
				StartBite(targetID);
			}
		}
		break;
	case State::BITING:
		// 等待 BITE_KILL_FRAME 帧事件推进状态
		break;
	case State::DIGESTING:
		mDigestTimer += GetWeatherActionDeltaTime();
		if (mDigestTimer >= DIGEST_DURATION) {
			EndDigest();
		}
		break;
	}
}

void Chomper::SaveExtraData(nlohmann::json& j) const
{
	j["state"] = static_cast<int>(mState);
	j["digestTimer"] = mDigestTimer;
	j["targetZombieID"] = mTargetZombieID;
}

void Chomper::LoadExtraData(const nlohmann::json& j)
{
	mState = static_cast<State>(j.value("state", 0));
	mDigestTimer = j.value("digestTimer", 0.0f);
	mTargetZombieID = j.value("targetZombieID", NULL_ZOMBIE_ID);

	// 注意：GameInfoSaver 先恢复植物再恢复僵尸，此时 EntityManager 里还没有
	// 目标僵尸；不能在这里验证 mTargetZombieID 是否存活。BITE_KILL_FRAME 回调
	// 触发时 GetZombie 拿不到会自然跳过 Die()，由此天然兜底。

	// 存档只保存 animTrack/animFrame，不会保存 PlayTrackOnce 的
	// mPlayingState/mTargetTrack 与帧事件。BITING/DIGESTING/IDLE 都需按状态
	// 重新装配动画与帧回调，否则 GameInfoSaver 之前的 PlayTrack 会让
	// anim_bite / anim_swallow 这类一次性轨道在 PLAY_REPEAT 下永远循环。
	switch (mState) {
	case State::IDLE:
		PlayTrack("anim_idle");
		break;
	case State::BITING: {
		// 保留 GameInfoSaver 已恢复的帧进度，避免咬合从头开始
		float savedFrame = mAnimator->GetCurrentFrame();
		// 若进度已越过杀伤帧（理论不应发生：43 帧回调会把状态切到 DIGESTING），
		// 强制从 0 开始让事件能正常触发，避免状态永远卡在 BITING
		if (savedFrame >= static_cast<float>(BITE_KILL_FRAME)) {
			savedFrame = 0.0f;
		}
		PlayTrackOnce("anim_bite", "anim_chew", 1.4f * GetWeatherActionSpeedMultiplier(), 0.05f);
		mAnimator->SetCurrentFrame(savedFrame);
		mAnimator->AddFrameEvent(BITE_KILL_FRAME, [this]() {
			OnBiteKillFrame();
			});
		break;
	}
	case State::DIGESTING:
		PlayTrack("anim_chew");
		break;
	}
}
