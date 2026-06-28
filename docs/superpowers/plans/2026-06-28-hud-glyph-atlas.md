# HUD 字形图集（方案A · 消尖峰）实现 Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用字形图集 + 逐字形 quad 替换僵尸/植物血量数字的"整串→纹理"LRU 路径，消除生存后期光栅化 thrash 造成的 `7.Draw_replay` 周期性尖峰。

**Architecture:** 主线程懒构建一张白色字形图集（单行打包，复用现有 TTF→ABGR8888→`CreateTextureRGBA8` 上传路径），新原语 `DrawGlyphRun` 逐字形拼 quad、顶点色 tint。worker 侧只记轻量 `DeferredGlyphRun` 命令、零光栅化，串行 `ReplayAndEndParallel` 就地发射（镜像 `DeferredText`，z-order 不变）。

**Tech Stack:** C++17 / Vulkan（自研 batch 渲染）/ SDL2_ttf / 现有 `Graphics` 类。

---

## 测试策略（诚实适配）

本仓库**无单元测试框架**；渲染管线（Vulkan/TTF）无法在无 GPU/字体的环境单测；且 CLAUDE.md +
主人明确指示"**验证主人自己来、不用 AutoTest**"（AutoTest 名表含未实现植物会一建即崩 + 触发杀毒拦
shell，见 [[project_pvz_zombie_hp_text_thrash]]）。因此：

- **每个 Task 的验证关口 = `clang-release` 零警告零错误编译**（clang-release 是唯一报
  `-Wunused-*`/`-Wreorder`/`-Wswitch` 等诊断的配置，warning-zero 验证只在此进行）。
- **端到端行为验证 = 主人手动**：全部 Task 完成后，主人在 VS / 真实游戏开 `mShowZombieHP` /
  `mShowPlantHP`，生存黑夜无尽后期看 `-Profile` 三行（`textRaster(miss)/frame`→~0、
  `7.Draw_replay` 尖峰消失）+ 目视血量正确。见 Task 4。
- Worker 不写 `git push`（commit 是 Claude 的活，push 是主人的）。

## 构建命令（Build）

**同一会话首次构建**需先导入 VS 开发环境（之后同会话只需第 2 段）：

```powershell
# 1) 导入 VS 开发环境（Installer 先进 PATH 以消除 vswhere 噪声）
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;" + $env:PATH
$vs = & vswhere -latest -property installationPath
cmd /c "`"$vs\Common7\Tools\VsDevCmd.bat`" -arch=x64 -no_logo && set" |
  ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }

# 2) 构建（warning-zero 验证配置）
cmake --build --preset clang-release
```

预期：`0 warning(s)` / `0 error(s)`，链接出 `build\clang-release\PlantsVsZombies.exe`。

## 提交边界（贯穿全 plan）

- 本 feature 只 `git add`：`Graphics.h`、`Graphics.cpp`、`Profiler.h`（诊断探针，已在工作树）、
  `Game/Zombie/Zombie.cpp`、`Game/Plant/Plant.cpp`。
- **绝不** `git add`：`Game/Board.h`、`Game/Plant/GameDataManager.cpp`（主人 DoorZombie WIP）。
- 分支：`feature/hud-glyph-atlas`（已建，设计文档 commit `835c357` 在其上）。

---

## Task 1: 字形图集子系统 + `DrawGlyphRun` 串行发射

产出："串行路径完整可用"的图集 + 原语（worker 分支暂 fallback 到旧 `DeferredText`，本 Task 不接入
Zombie/Plant，故运行行为不变；只验证编译）。

**Files:**
- Modify: `PlantVsZombies/Graphics.h`（加结构体 + 成员 + 方法声明）
- Modify: `PlantVsZombies/Graphics.cpp`（实现 + 清理钩子；含已在工作树的诊断探针）
- Modify: `PlantVsZombies/Profiler.h`（仅一并提交已在工作树的探针，本 Task 不改其内容）

- [ ] **Step 1: Graphics.h — 加 `GlyphInfo` / `GlyphAtlas` 结构体**

在 `CachedText` 结构体之后（约 `Graphics.h:106` 之后）插入。确保文件顶部 include 含 `<set>`、
`<unordered_map>`、`<string>`（`<unordered_map>` 已有；若无 `<set>` 在 include 区补 `#include <set>`）：

```cpp
/**
 * @brief 图集内单个字形的 UV + 度量（物理像素口径）。
 */
struct GlyphInfo {
	float u0 = 0, v0 = 0, u1 = 0, v1 = 0;  // 图集内归一化 UV（左上 u0,v0 / 右下 u1,v1）
	int   pxW = 0, pxH = 0;                // 字形位图物理像素尺寸
	int   advance = 0;                     // 笔触前进（物理像素）
	int   bearingX = 0;                    // minx（物理像素）
	int   bearingY = 0;                    // maxy 相对基线（物理像素）
};

/**
 * @brief 一张 HUD 字形图集：单行打包的白色字形纹理（一个 bindless 槽），按 (字体,字号) 缓存。
 *        白色烘焙 + 顶点色 tint → 一张图集服务所有颜色。physSize 变化（letterbox）时整张重建。
 */
struct GlyphAtlas {
	pvz::VulkanTexture* vkTex = nullptr;     // 图集纹理句柄（ClearGlyphAtlases 负责还给 pool）
	uint32_t            textureID = 0;       // vkTex->bindlessIndex；0 = 未建/建失败
	int                 texW = 0, texH = 0;
	int                 ascent = 0;          // physSize 字体的 TTF_FontAscent（物理像素）
	int                 physSize = 0;        // 烘焙物理字号（= ComputeTextRasterSize 结果）
	float               superSample = 1.0f;  // physSize / fontSize
	std::unordered_map<uint32_t, GlyphInfo> glyphs;  // codepoint → 字形
	std::set<uint32_t>  covered;             // 已纳入的码点全集（重建时的烘焙集合）
};
```

- [ ] **Step 2: Graphics.h — 加公有原语声明**

在 `DrawTextOnTop` 声明之后（约 `Graphics.h:405` 之后）插入：

```cpp
	/**
	 * @brief 用 HUD 字形图集逐字形绘制一段文字（与对象同 z-order，等价 DrawText 的层序）。
	 *        替代频繁变化的数字串（血量等）走的"整串→纹理"LRU 路径：字形只光栅化一次进图集，
	 *        变化的数字成本≈0。worker 路径记轻量命令、零光栅化；图集只在主线程构建。
	 * @note  color 是 **0..255** 范围，同 DrawText。建图集失败时自动 fallback 到 DrawText。
	 */
	void DrawGlyphRun(const std::string& text, const std::string& fontKey, int fontSize,
		const glm::vec4& color, float x, float y, float scale = 1.0f);
```

- [ ] **Step 3: Graphics.h — 加私有方法声明 + 成员**

在私有区 `RenderTextToVulkanTexture` 声明之后（约 `Graphics.h:894` 之后）插入：

```cpp
	/**
	 * @brief 取/建 (fontKey,fontSize) 的字形图集，并确保 needed 里的码点全部已烘入（当帧收敛）。
	 *        physSize 变化（letterbox）或有新码点时整张重建。返回引用恒有效（unordered_map 引用稳定）。
	 */
	GlyphAtlas& GetOrBuildGlyphAtlas(const std::string& fontKey, int fontSize,
		const std::vector<uint32_t>& needed);

	/**
	 * @brief 按 atlas.covered 全集（重新）烘焙图集：逐字形 TTF 度量+渲染→单行打包→上传一张纹理。
	 *        旧纹理先还给 pool。失败时令 atlas.textureID=0（上层 fallback）。
	 */
	void BuildGlyphAtlas(const std::string& fontKey, int fontSize, GlyphAtlas& atlas);

	/**
	 * @brief 释放全部字形图集纹理（还给 pool），清空缓存。ShutdownVulkan / ClearTextCache 时调。
	 */
	void ClearGlyphAtlases();
```

在成员区 `m_textGeneration`（约 `Graphics.h:797`）附近插入：

```cpp
	std::unordered_map<std::string, GlyphAtlas> m_glyphAtlases;  ///< HUD 字形图集，键 = "fontKey|fontSize"
```

- [ ] **Step 4: Graphics.h — 加 worker record 声明（为 Task 2 预留，本 Task 不实现）**

在 `RecordDrawText` 声明之后（约 `Graphics.h:932` 之后）插入：

```cpp
	void RecordDrawGlyphRun(WorkerRecord& r,
		const std::string& text, const std::string& fontKey, int fontSize,
		const glm::vec4& color, float x, float y, float scale);
```

- [ ] **Step 5: Graphics.cpp — 加 UTF-8 解码 helper**

在 `ComputeTextRasterSize`（约 `Graphics.cpp:1193`）之前插入文件内 static helper：

```cpp
// UTF-8 解码：从 s[i] 起解一个码点，前进 i。非法字节返回 0xFFFD 并前进 1。
// HP 字符集（数字/标点/CJK 本体一类二类）全在 BMP，但仍按通用 UTF-8 正确解多字节。
static uint32_t DecodeUtf8(const std::string& s, size_t& i) {
	const unsigned char c = (unsigned char)s[i];
	uint32_t cp; int extra;
	if (c < 0x80)            { cp = c;        extra = 0; }
	else if ((c >> 5) == 0x6){ cp = c & 0x1F; extra = 1; }
	else if ((c >> 4) == 0xE){ cp = c & 0x0F; extra = 2; }
	else if ((c >> 3) == 0x1E){ cp = c & 0x07; extra = 3; }
	else { ++i; return 0xFFFD; }
	++i;
	for (int k = 0; k < extra && i < s.size(); ++k, ++i)
		cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
	return cp;
}
```

- [ ] **Step 6: Graphics.cpp — 实现 `BuildGlyphAtlas`**

在 `RenderTextToVulkanTexture`（约 `Graphics.cpp:1306`）之后插入。单行打包：第一趟逐字形渲染拿尺寸、
累计图集宽高；建一张清零的 ABGR8888 surface；第二趟 blit（`SDL_BLENDMODE_NONE` 直接覆盖含 alpha）；
上传一张纹理。

```cpp
void Graphics::BuildGlyphAtlas(const std::string& fontKey, int fontSize, GlyphAtlas& atlas) {
	atlas.glyphs.clear();
	// 旧纹理先还给 pool（重建场景：新码点 / letterbox 变化）。
	if (m_vk && m_vk->texPool && atlas.vkTex) {
		m_vk->texPool->DestroyTexture(atlas.vkTex);
		atlas.vkTex = nullptr;
	}
	atlas.textureID = 0;

	if (!m_vk || !m_vk->texPool || atlas.covered.empty()) return;

	float superSample;
	const int physSize = ComputeTextRasterSize(fontSize, m_letterboxScale, superSample);
	TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, physSize);
	if (!font) {
		LOG_WARN("Graphics") << "BuildGlyphAtlas: 找不到字体 " << fontKey << " size=" << physSize;
		return;  // textureID 保持 0 → 上层 fallback
	}

	const SDL_Color white{ 255, 255, 255, 255 };
	const int pad = 1;

	// 第一趟：渲染每个字形、量尺寸、算单行布局。
	struct Pending { uint32_t cp; SDL_Surface* surf; int minx, maxy, advance; int x; };
	std::vector<Pending> pend;
	pend.reserve(atlas.covered.size());
	int atlasW = pad, atlasH = 1;
	for (uint32_t cp : atlas.covered) {
		int minx, maxx, miny, maxy, adv;
		if (TTF_GlyphMetrics(font, (Uint16)cp, &minx, &maxx, &miny, &maxy, &adv) != 0)
			continue;  // 该字形不可用，跳过（绘制时缺字形会触发整串 fallback）
		SDL_Surface* gs = TTF_RenderGlyph_Blended(font, (Uint16)cp, white);  // 空白字形可能返回 null
		const int gw = gs ? gs->w : 0;
		const int gh = gs ? gs->h : 0;
		Pending p{ cp, gs, minx, maxy, adv, atlasW };
		pend.push_back(p);
		atlasW += gw + pad;
		if (gh > atlasH) atlasH = gh;
	}
	if (pend.empty()) return;

	// 目标图集 surface（ABGR8888，清零）。
	SDL_Surface* atlasSurf = SDL_CreateRGBSurfaceWithFormat(0, atlasW, atlasH, 32, SDL_PIXELFORMAT_ABGR8888);
	if (!atlasSurf) {
		for (auto& p : pend) if (p.surf) SDL_FreeSurface(p.surf);
		LOG_ERROR("Graphics") << "BuildGlyphAtlas: SDL_CreateRGBSurfaceWithFormat 失败: " << SDL_GetError();
		return;
	}
	SDL_FillRect(atlasSurf, nullptr, SDL_MapRGBA(atlasSurf->format, 0, 0, 0, 0));

	const int ascent = TTF_FontAscent(font);

	// 第二趟：blit 进图集（NONE 混合=直接覆盖含 alpha），记录 GlyphInfo。
	for (auto& p : pend) {
		const int gw = p.surf ? p.surf->w : 0;
		const int gh = p.surf ? p.surf->h : 0;
		if (p.surf && gw > 0 && gh > 0) {
			SDL_SetSurfaceBlendMode(p.surf, SDL_BLENDMODE_NONE);
			SDL_Rect dst{ p.x, 0, gw, gh };
			SDL_BlitSurface(p.surf, nullptr, atlasSurf, &dst);
		}
		GlyphInfo gi;
		gi.u0 = (float)p.x / (float)atlasW;
		gi.v0 = 0.0f;
		gi.u1 = (float)(p.x + gw) / (float)atlasW;
		gi.v1 = (float)gh / (float)atlasH;
		gi.pxW = gw;
		gi.pxH = gh;
		gi.advance = p.advance;
		gi.bearingX = p.minx;
		gi.bearingY = p.maxy;
		atlas.glyphs[p.cp] = gi;
		if (p.surf) SDL_FreeSurface(p.surf);
	}

	pvz::VulkanTexture* vkt = m_vk->texPool->CreateTextureRGBA8(atlasW, atlasH, atlasSurf->pixels);
	SDL_FreeSurface(atlasSurf);
	if (!vkt) {
		LOG_ERROR("Graphics") << "BuildGlyphAtlas: CreateTextureRGBA8 失败";
		atlas.glyphs.clear();
		return;
	}
	atlas.vkTex = vkt;
	atlas.textureID = vkt->bindlessIndex;
	atlas.texW = atlasW;
	atlas.texH = atlasH;
	atlas.ascent = ascent;
	atlas.physSize = physSize;
	atlas.superSample = superSample;
}
```

- [ ] **Step 7: Graphics.cpp — 实现 `GetOrBuildGlyphAtlas`**

紧接 `BuildGlyphAtlas` 之后插入：

```cpp
GlyphAtlas& Graphics::GetOrBuildGlyphAtlas(const std::string& fontKey, int fontSize,
	const std::vector<uint32_t>& needed) {
	const std::string key = fontKey + "|" + std::to_string(fontSize);
	GlyphAtlas& atlas = m_glyphAtlases[key];

	float ss;
	const int curPhys = ComputeTextRasterSize(fontSize, m_letterboxScale, ss);
	// physSize 变化（letterbox）或图集未建 → 需重建；有新码点 → 加入 covered 后重建。
	bool needRebuild = (atlas.textureID == 0) || (atlas.physSize != curPhys);
	for (uint32_t cp : needed)
		if (atlas.covered.insert(cp).second) needRebuild = true;

	if (needRebuild) BuildGlyphAtlas(fontKey, fontSize, atlas);
	return atlas;
}
```

- [ ] **Step 8: Graphics.cpp — 实现 `DrawGlyphRun`（主线程发射；worker 暂 fallback）**

在 `DrawTextOnTop`（约 `Graphics.cpp:1431`）之后插入。本 Task worker 分支先走旧 `RecordDrawText`
（Task 2 改为 `RecordDrawGlyphRun`）：

```cpp
void Graphics::DrawGlyphRun(const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale) {
	// worker：Task 1 暂走旧 DeferredText（整串光栅化）路径；Task 2 改为 RecordDrawGlyphRun。
	if (tl_record) { RecordDrawText(*tl_record, text, fontKey, fontSize, color, x, y, scale, /*onTop*/false); return; }
	if (text.empty()) return;

	// 1. 解码本串全部码点。
	std::vector<uint32_t> cps;
	for (size_t i = 0; i < text.size(); ) {
		uint32_t cp = DecodeUtf8(text, i);
		if (cp) cps.push_back(cp);
	}
	if (cps.empty()) return;

	// 2. 保 z-order（同 DrawText）：先 flush 已排队 instance，文字落在已画对象之上、后续对象之下。
	FlushInstances();

	// 3. 取/建图集（当帧把缺码点并入并重建 → 此后所有字形必在）。
	GlyphAtlas& atlas = GetOrBuildGlyphAtlas(fontKey, fontSize, cps);
	if (atlas.textureID == 0) {  // 建失败 → fallback 整串 DrawText，保证不消失
		DrawText(text, fontKey, fontSize, color, x, y, scale);
		return;
	}

	// 4. 逐字形拼 quad。度量是物理像素，× invSS 转逻辑尺寸。烘白 + 顶点色 tint（NormalizeColor）。
	const int texIndex = BindTexture(atlas.textureID);
	const glm::vec4 nc = NormalizeColor(color);
	const float invSS = scale / atlas.superSample;
	const float ascent = (float)atlas.ascent;
	float penX = 0.0f;
	for (uint32_t cp : cps) {
		auto it = atlas.glyphs.find(cp);
		if (it == atlas.glyphs.end()) continue;  // 理论不达（当帧已建全）；防御
		const GlyphInfo& gi = it->second;
		if (gi.pxW > 0 && gi.pxH > 0) {
			const float gx = x + (penX + gi.bearingX) * invSS;
			const float gy = y + (ascent - gi.bearingY) * invSS;
			const float gw = gi.pxW * invSS;
			const float gh = gi.pxH * invSS;
			glm::mat4 local = glm::translate(glm::mat4(1.0f), glm::vec3(gx, gy, 0.0f));
			local = glm::scale(local, glm::vec3(gw, gh, 1.0f));
			const glm::mat4 finalMatrix = m_transformStack.back() * local;
			const int matrixIndex = AddMatrix(finalMatrix);
			const BatchVertex verts[6] = {
				{0.0f, 1.0f, gi.u0, gi.v1, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, 0.0f},
				{1.0f, 1.0f, gi.u1, gi.v1, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, 0.0f},
				{1.0f, 0.0f, gi.u1, gi.v0, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, 0.0f},
				{0.0f, 1.0f, gi.u0, gi.v1, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, 0.0f},
				{0.0f, 0.0f, gi.u0, gi.v0, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, 0.0f},
				{1.0f, 0.0f, gi.u1, gi.v0, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, 0.0f}
			};
			AddVertices(verts, 6);
			CheckBatch();
		}
		penX += gi.advance;
	}
}
```

- [ ] **Step 9: Graphics.cpp — 实现 `ClearGlyphAtlases` + 接清理钩子**

紧接 `GetOrBuildGlyphAtlas` 之后插入：

```cpp
void Graphics::ClearGlyphAtlases() {
	if (m_vk && m_vk->texPool) {
		for (auto& kv : m_glyphAtlases)
			if (kv.second.vkTex) m_vk->texPool->DestroyTexture(kv.second.vkTex);
	}
	m_glyphAtlases.clear();
}
```

在 `ClearTextCache()`（约 `Graphics.cpp:1433`）函数体末尾追加一行（图集与文字 LRU 同生命周期，
随 Shutdown/清缓存一并释放）：

```cpp
	ClearGlyphAtlases();
```

> 注：`ClearTextCache` 在 `ShutdownVulkan` 路径被调用以还纹理；确认调用点在 pool 销毁之前
> （与现有 `ClearTextCache` 同位置，天然满足）。无需 letterbox 专门钩子——`GetOrBuildGlyphAtlas`
> 用 `physSize != curPhys` 自动在 letterbox 变化后重建。

- [ ] **Step 10: 构建验证**

运行 Build（见上）。预期：`0 warning(s)` / `0 error(s)`。
重点排查：`<set>` 未 include（报 `std::set` 未定义）、`SDL_ttf.h` 已被 Graphics.cpp 间接包含
（现有 `TTF_RenderUTF8_Blended` 已用，无需新 include）、`glm::translate/scale` 已用（DrawText 同款）。

- [ ] **Step 11: Commit**

```bash
git add PlantVsZombies/Graphics.h PlantVsZombies/Graphics.cpp PlantVsZombies/Profiler.h
git commit -F - <<'EOF'
feat(perf): HUD 字形图集子系统 + DrawGlyphRun 串行发射(方案A Task1)

GlyphAtlas 单行打包白色字形(复用 TTF→ABGR8888→CreateTextureRGBA8 上传),
DrawGlyphRun 逐字形 quad + 顶点色 tint, 当帧解码缺码点并入重建(收敛即时),
physSize 变化自动重建; 建失败 fallback DrawText。worker 暂走旧 DeferredText,
本 Task 不接入 Zombie/Plant。一并提交诊断探针(Profiler.CountText / 7a.TextRaster_miss)。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
```

---

## Task 2: worker defer + replay 就地发射（镜像 DeferredText）

产出：血量文字在并行 Draw 路径走"零光栅化"——worker 只记轻量命令，串行 replay 就地发射字形 quad。
这是消除尖峰的核心一环。

**Files:**
- Modify: `PlantVsZombies/Graphics.h`（枚举 + 结构体 + WorkerRecord 字段 + Reset）
- Modify: `PlantVsZombies/Graphics.cpp`（`RecordDrawGlyphRun` 实现 + replay case + `DrawGlyphRun` worker 分支改接）

- [ ] **Step 1: Graphics.h — `RecCmdType` 加枚举值**

把 `enum class RecCmdType`（约 `Graphics.h:142`）的 `DeferredText` 行改为加一项（注意原 `DeferredText`
末尾补逗号）：

```cpp
enum class RecCmdType : uint8_t {
	PushClip,       ///< 推入裁剪矩形（payloadIdx 指向 clips）
	PopClip,        ///< 弹出裁剪矩形
	SetBlend,       ///< 切换混合模式（payloadIdx 指向 blendModes）
	DeferredText,   ///< DrawText 延迟到主线程执行（payloadIdx 指向 textCmds）
	DeferredGlyphRun ///< DrawGlyphRun 延迟到主线程就地发射（payloadIdx 指向 glyphRunCmds）
};
```

- [ ] **Step 2: Graphics.h — 加 `DeferredGlyphRunCmd` 结构体**

在 `DeferredTextCmd` 结构体（约 `Graphics.h:178`）之后插入：

```cpp
/**
 * @brief 延迟执行的 DrawGlyphRun 参数。worker 只记此命令、不碰图集/纹理；主线程 replay 时
 *        就地调 DrawGlyphRun 发射字形 quad（与对象同 z-order）。血量只需当前层，无 onTop 变体。
 */
struct DeferredGlyphRunCmd {
	std::string text;
	std::string fontKey;
	int         fontSize = 0;
	glm::vec4   color = glm::vec4(255.0f);
	float       x = 0.0f;
	float       y = 0.0f;
	float       scale = 1.0f;
};
```

- [ ] **Step 3: Graphics.h — `WorkerRecord` 加字段 + Reset 清理**

在 `WorkerRecord`（约 `Graphics.h:213`）的 `textCmds` 字段后加：

```cpp
	std::vector<DeferredGlyphRunCmd> glyphRunCmds;  ///< DeferredGlyphRun 的 payload
```

并在其 `Reset()` 里 `textCmds.clear();` 之后加：

```cpp
		glyphRunCmds.clear();
```

- [ ] **Step 4: Graphics.cpp — 实现 `RecordDrawGlyphRun`**

在 `RecordDrawText`（约 `Graphics.cpp:2354`）之后插入（镜像 `RecordDrawText` 的 cmd 记录，但 payload
是轻量 glyph run、零光栅化）：

```cpp
void Graphics::RecordDrawGlyphRun(WorkerRecord& r,
	const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale)
{
	// 只打包参数；图集构建 + 字形 quad 发射全部 defer 到主线程 replay（就地、当前层 z-order）。
	DeferredGlyphRunCmd t;
	t.text = text;
	t.fontKey = fontKey;
	t.fontSize = fontSize;
	t.color = color;
	t.x = x;
	t.y = y;
	t.scale = scale;
	r.glyphRunCmds.push_back(std::move(t));

	RecordCmd c{};
	c.type = RecCmdType::DeferredGlyphRun;
	c.vertOffsetAtCmd = r.slice.vboCount;
	c.instOffsetAtCmd = r.slice.instCount;
	c.payloadIdx = (uint32_t)(r.glyphRunCmds.size() - 1);
	r.cmds.push_back(c);
}
```

- [ ] **Step 5: Graphics.cpp — `DrawGlyphRun` worker 分支改接**

把 Task 1 Step 8 写的 worker fallback 行（`Graphics.cpp` 的 `DrawGlyphRun` 第一行）：

```cpp
	if (tl_record) { RecordDrawText(*tl_record, text, fontKey, fontSize, color, x, y, scale, /*onTop*/false); return; }
```

改为：

```cpp
	if (tl_record) { RecordDrawGlyphRun(*tl_record, text, fontKey, fontSize, color, x, y, scale); return; }
```

- [ ] **Step 6: Graphics.cpp — `ReplayAndEndParallel` 加 `DeferredGlyphRun` case**

在 `ReplayAndEndParallel` 的 `switch (cmd.type)` 里、`case RecCmdType::DeferredText:` 块（约
`Graphics.cpp:2084-2100`）之后插入新 case（逐字镜像 `DeferredText` 的 `onTop=false` 分支）：

```cpp
			case RecCmdType::DeferredGlyphRun: {
				const DeferredGlyphRunCmd& t = r.glyphRunCmds[cmd.payloadIdx];
				// 就地发射：emitUpTo 已把本对象 sprite emit 完，此处画字形 quad，夹在本对象与
				// 后续对象之间 = 与对象同 z-order。DrawGlyphRun/FlushBatch 会重绑 pipeline/vbo，
				// 画完清空 boundPipe 哨兵，强制下次 emitUpTo 重绑。
				DrawGlyphRun(t.text, t.fontKey, t.fontSize, t.color, t.x, t.y, t.scale);
				FlushBatch();
				boundPipe = BoundPipe::None;
				break;
			}
```

> 注：此处 `DrawGlyphRun` 在主线程执行（replay 阶段 `tl_record` 为空），走主线程发射分支；
> 其内部 `FlushInstances()` 时主线程 instance 队列为空（并行 instance 在 slice 里），是 no-op，
> 与 `DeferredText` 路径调 `DrawText`→`FlushInstances()` 同构、安全。

- [ ] **Step 7: 构建验证**

运行 Build。预期：`0 warning(s)` / `0 error(s)`。重点：`switch` 新增 case 后无 `-Wswitch` 警告
（已覆盖新枚举值）；`-Wreorder` 无（`DeferredGlyphRunCmd` 成员按声明序初始化）。

- [ ] **Step 8: Commit**

```bash
git add PlantVsZombies/Graphics.h PlantVsZombies/Graphics.cpp
git commit -F - <<'EOF'
feat(perf): DrawGlyphRun worker defer + replay 就地发射(方案A Task2)

新增 RecCmdType::DeferredGlyphRun + DeferredGlyphRunCmd + WorkerRecord.glyphRunCmds;
worker 只记轻量命令零光栅化, ReplayAndEndParallel 就地发射字形 quad(镜像 DeferredText
onTop=false: DrawGlyphRun + FlushBatch + boundPipe=None), z-order 与现状一致。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
```

---

## Task 3: 接入 Zombie + Plant 血量

产出：两处血量 debug 叠层从 `DrawText` 切到 `DrawGlyphRun`，feature 端到端生效。

**Files:**
- Modify: `PlantVsZombies/Game/Zombie/Zombie.cpp:584`
- Modify: `PlantVsZombies/Game/Plant/Plant.cpp:144`

- [ ] **Step 1: Zombie.cpp — `drawLine` 改用 `DrawGlyphRun`**

把 `Zombie::Draw` 里 `drawLine` lambda（约 `Zombie.cpp:583-586`）的：

```cpp
	auto drawLine = [&](const std::string& text) {
		g->DrawText(text, ResourceKeys::Fonts::FONT_FZJZ, fontSize, lightBlue, pos.x, y);
		y += lineHeight;
		};
```

改为：

```cpp
	auto drawLine = [&](const std::string& text) {
		g->DrawGlyphRun(text, ResourceKeys::Fonts::FONT_FZJZ, fontSize, lightBlue, pos.x, y);
		y += lineHeight;
		};
```

- [ ] **Step 2: Plant.cpp — 血量行改用 `DrawGlyphRun`**

把 `Plant::Draw`（约 `Plant.cpp:144`）的：

```cpp
	g->DrawText(text, ResourceKeys::Fonts::FONT_FZCQ, 17, green, pos.x, pos.y);
```

改为：

```cpp
	g->DrawGlyphRun(text, ResourceKeys::Fonts::FONT_FZCQ, 17, green, pos.x, pos.y);
```

- [ ] **Step 3: 构建验证**

运行 Build。预期：`0 warning(s)` / `0 error(s)`。

- [ ] **Step 4: Commit**

```bash
git add PlantVsZombies/Game/Zombie/Zombie.cpp PlantVsZombies/Game/Plant/Plant.cpp
git commit -F - <<'EOF'
feat(perf): 僵尸/植物血量切到 DrawGlyphRun(方案A Task3 接入)

Zombie::Draw / Plant::Draw 的血量 DrawText → DrawGlyphRun, 血量数字不再走
整串→纹理 LRU, 消除生存后期光栅化 thrash 尖峰。z-order/视口剔除/颜色不变。

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
```

---

## Task 4: 主人手动验证（关口，非 Claude 执行）

> Claude 不跑游戏验证本 feature（AutoTest 崩溃风险 + 主人指示）。Claude 完成 Task 1-3 编译验证后，
> 把以下清单交给主人，由主人在 VS / 真实游戏确认。

- [ ] **目视正确性**：开 `mShowZombieHP` / `mShowPlantHP`，进任意关卡。僵尸血量浅蓝、植物血量绿、
  数字清晰、正确叠在身体上方、屏外不显示（视口剔除仍生效）。
  - 若颜色发暗/发白：按预乘 alpha 三层契约 [[project_pvz_premultiplied_alpha]] 微调（顶点色已用
    `NormalizeColor`，理论与现路径等价；若不符，检查图集纹理是否被 `CreateTextureRGBA8` 二次预乘）。
  - 若字形错位/重叠：检查 `ascent - bearingY` 的基线换算与 `advance` 排版。
- [ ] **消尖峰**：生存黑夜无尽后期（数百僵尸、伤害密集），`-Profile` 三行：
  - `textRaster(miss)/frame` → ~0（血量不再光栅化；别处普通 DrawText 残余少量正常）。
  - `7.Draw_replay` 周期性尖峰消失（伤害密集时不再 4.9→7.1ms 飙升）。
- [ ] **回归**：中文 UI 串（菜单/卡片/almanac）显示正常（现有 `DrawText`/LRU 路径未动）。
- [ ] **决定方案B**：若稳态 `7.Draw_replay` 地板仍偏高且值得动，再启动方案B（把字形 quad 发射搬进
  worker slice，需重验 batch/instance 层序）——见设计文档 §9。

---

## Self-Review（写 plan 后自查）

- **Spec 覆盖**：§3.1 数据结构→Task1 Step1/3；§3.2 懒构建+当帧收敛→Task1 Step6/7；§3.3 发射→Task1
  Step8；§3.4 worker+replay→Task2；§4 接入→Task3；§5 不变量（预乘/0..255/z-order/worker不建纹理/
  letterbox 失效）→Task1 Step6-9 注释 + Task2 Step6 注释；§6 回退→Task1 Step8 fallback；§7 验证→
  Task4；§8 提交边界→各 Task commit 的 `git add` 白名单。✔ 无遗漏。
- **占位符扫描**：无 TBD/TODO；所有 code step 给出完整代码。✔
- **类型一致**：`DrawGlyphRun` / `GetOrBuildGlyphAtlas` / `BuildGlyphAtlas` / `ClearGlyphAtlases` /
  `RecordDrawGlyphRun` / `DeferredGlyphRunCmd` / `GlyphAtlas` / `GlyphInfo` / `m_glyphAtlases` 跨
  Task 命名一致；`RecCmdType::DeferredGlyphRun` 枚举名一致。✔
