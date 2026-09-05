#include "Scene.h"
#include "../ParticleSystem/ParticleSystem.h"
#include "GameObjectManager.h"
#include "CollisionSystem.h"
#include "../ResourceManager.h"
#include "ClickableComponent.h"
#include "../GameApp.h"
#include "../Profiler.h"
#include "../Logger.h"

void Scene::BuildDrawCommands() {
	mDrawCommands.clear();

	// 添加纹理绘制命令
	RegisterDrawCommand("GameTextures",
		[this](Graphics* g) { this->DrawWorldTextures(g); },
		LAYER_BACKGROUND);

	// 添加游戏对象绘制命令
	RegisterDrawCommand("GameObjects",
		[this](Graphics* g) { this->DrawGameObjects(g); },
		LAYER_GAME_OBJECT);

	// 注册粒子系统绘制
	RegisterDrawCommand("ParticleSystem",
		[](Graphics* g) {
			if (g_particleSystem) {
				// 顶层特效（>= LAYER_UI）：世界层粒子由 GameObjectManager 的 pre-overlay hook 绘制
				g_particleSystem->DrawFrom(LAYER_UI);
			}
		},
		LAYER_EFFECTS);

	// 添加UI绘制命令
	RegisterDrawCommand("UI",
		[this](Graphics* g) { this->mUIManager.DrawControls(g); },
		LAYER_UI);
}

void Scene::RegisterDrawCommand(const std::string& name,
	std::function<void(Graphics*)> drawFunc,
	int renderOrder)
{
	auto it = std::find_if(mDrawCommands.begin(), mDrawCommands.end(),
		[&name](const DrawCommand& cmd) { return cmd.name == name; });

	if (it != mDrawCommands.end()) {
		it->drawFunc = drawFunc;
		it->renderOrder = renderOrder;
	}
	else {
		mDrawCommands.emplace_back(drawFunc, renderOrder, name);
	}
}

/** 按层执行场景绘制命令，并在 -Profile 下按注册名暴露 GameObjectManager 外围耗时。 */
void Scene::Draw(Graphics* g) {
	if (!mDrawCommandsBuilt) {
		BuildDrawCommands();
		SortDrawCommands();
		mDrawCommandsBuilt = true;
	}
	for (auto& cmd : mDrawCommands) {
		if (cmd.drawFunc) {
			ScopedProfile commandProfile(cmd.profileName.c_str());
			cmd.drawFunc(g);
		}
	}
	// 模态框不参与普通层号排序：主菜单入口、选关标签和 HUD 都可能晚于 LAYER_UI。
	// 在所有场景命令结束后提交，保证最后创建的活动框在视觉上也位于顶层。
	mUIManager.DrawMessageBoxes(g);
}

void Scene::Update()
{
	{
		PROFILE_SCOPE("1.Particles_Update");
		if (g_particleSystem)
		{
			g_particleSystem->UpdateAll();
		}
	}
	auto input = &GameAPP::GetInstance().GetInputHandler();
	// 关闭最后一层弹窗的释放事件也不能穿透到场景对象。
	const bool hadModalInput = mUIManager.GetTopActiveMessageBox() != nullptr;
	input->SetSceneMouseBlocked(false);
	{
		PROFILE_SCOPE("1a.UIManager");
		mUIManager.ProcessMouseEvent(input);
		mUIManager.UpdateAll(input);
	}
	input->SetSceneMouseBlocked(hadModalInput || mUIManager.GetTopActiveMessageBox() != nullptr);
	GameObjectManager::GetInstance().Update();
	UpdateAfterGameObjects();
	{
		PROFILE_SCOPE("1b.Clickable");
		if (!hadModalInput && !mUIManager.GetTopActiveMessageBox()) {
			ClickableComponent::ProcessMouseEvents();
		}
	}
	{
		PROFILE_SCOPE("3.Collision_Update");
		CollisionSystem::GetInstance().Update();
	}
}

void Scene::UnregisterDrawCommand(const std::string& name) {
	auto it = std::remove_if(mDrawCommands.begin(), mDrawCommands.end(),
		[&name](const DrawCommand& cmd) { return cmd.name == name; });
	mDrawCommands.erase(it, mDrawCommands.end());
}

void Scene::AddTexture(const std::string& textureName, float posX, float posY, float scaleX, float scaleY, int drawOrder, bool isUI) {
	const Texture* texture = ResourceManager::GetInstance().GetTexture(textureName);
	if (texture) {
		TextureInfo info{ texture, posX, posY };
		info.scaleX = scaleX;
		info.scaleY = scaleY;
		info.drawOrder = drawOrder;
		info.name = textureName;
		info.isUI = isUI;
		mTextures.push_back(info);
		LOG_DEBUG("Scene") << "场景 " << name << " 添加纹理: " << textureName
			<< " 位置: (" << posX << ", " << posY << ")"
			<< " 缩放X:" << scaleX << "Y:" << scaleY;
	}
	else {
		LOG_ERROR("Scene") << "场景 " << name << " 无法加载纹理: " << textureName;
	}
}

void Scene::RemoveTexture(const std::string& textureName) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	if (it != mTextures.end()) {
		LOG_DEBUG("Scene") << "场景 " << name << " 移除纹理: " << textureName;
		mTextures.erase(it);
	}
}

void Scene::SetTexturePosition(const std::string& textureName, float posX, float posY) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	if (it != mTextures.end()) {
		it->posX = posX;
		it->posY = posY;
	}
}

void Scene::SetTextureScale(const std::string& textureName, float scaleX, float scaleY) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	if (it != mTextures.end()) {
		it->scaleX = scaleX;
		it->scaleY = scaleY;
	}
}

void Scene::SetTextureScaleX(const std::string& textureName, float scaleX) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	if (it != mTextures.end()) {
		it->scaleX = scaleX;
	}
}

void Scene::SetTextureScaleY(const std::string& textureName, float scaleY) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	if (it != mTextures.end()) {
		it->scaleY = scaleY;
	}
}

void Scene::SetTextureVisible(const std::string& textureName, bool visible) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	if (it != mTextures.end()) {
		it->visible = visible;
	}
}

void Scene::SetTextureDrawOrder(const std::string& textureName, int drawOrder) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	if (it != mTextures.end()) {
		it->drawOrder = drawOrder;
	}
}

void Scene::ClearAllTextures() {
	LOG_DEBUG("Scene") << "场景 " << name << " 清空所有纹理";
	mTextures.clear();
}

void Scene::DrawWorldTextures(Graphics* g) {
	DrawTextureLayer(g, false);
}

void Scene::DrawUITextures(Graphics* g) {
	DrawTextureLayer(g, true);
}

/** 按坐标空间拆分 Scene 贴图，让 UI 贴图能插入世界天气覆盖层之后。 */
void Scene::DrawTextureLayer(Graphics* g, bool uiLayer) {
	// 按绘制顺序排序
	std::sort(mTextures.begin(), mTextures.end(),
		[](const TextureInfo& a, const TextureInfo& b) {
			return a.drawOrder < b.drawOrder;
		});

	for (size_t i = 0; i < mTextures.size(); ++i) {
		const auto& texInfo = mTextures[i];
		if (!texInfo.visible || texInfo.isUI != uiLayer) continue;

		// 计算显示尺寸
		float displayWidth = texInfo.texture->width * texInfo.scaleX;
		float displayHeight = texInfo.texture->height * texInfo.scaleY;
		Vector drawPosition = Vector(texInfo.posX, texInfo.posY);

		if (uiLayer) {
			drawPosition = g->LogicalToWorld(texInfo.posX, texInfo.posY);
		}

		g->DrawTexture(texInfo.texture, drawPosition.x, drawPosition.y,
			displayWidth, displayHeight);
	}
}

TextureInfo* Scene::GetTextureInfo(const std::string& textureName) {
	auto it = std::find_if(mTextures.begin(), mTextures.end(),
		[&textureName](const TextureInfo& info) {
			return info.name == textureName;
		});

	return it != mTextures.end() ? &(*it) : nullptr;
}
