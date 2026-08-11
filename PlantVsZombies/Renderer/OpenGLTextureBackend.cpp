#include "OpenGLTextureBackend.h"

#include "OpenGLApi.h"
#include "../Logger.h"

#include <algorithm>
#include <cstddef>

namespace pvz {

	OpenGLTextureBackend::~OpenGLTextureBackend() {
		Shutdown();
	}

	bool OpenGLTextureBackend::Initialize(OpenGLApi* api) {
		if (!api) return false;
		mApi = api;
		mApi->GetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
		if (mMaxTextureSize < 1) {
			LOG_ERROR("OpenGLTexture") << "GL_MAX_TEXTURE_SIZE 无效: " << mMaxTextureSize;
			mApi = nullptr;
			return false;
		}
		return true;
	}

	void OpenGLTextureBackend::Shutdown() {
		if (mApi) {
			for (auto& item : mTextures) {
				const unsigned int name = item.first;
				if (name) mApi->DeleteTextures(1, &name);
			}
		}
		mTextures.clear();
		mMaxTextureSize = 0;
		mApi = nullptr;
	}

	std::vector<std::uint8_t> OpenGLTextureBackend::PremultiplyRGBA8(
		int width, int height, const void* pixels) {
		const auto* source = static_cast<const std::uint8_t*>(pixels);
		const std::size_t pixelCount = static_cast<std::size_t>(width)
			* static_cast<std::size_t>(height);
		std::vector<std::uint8_t> result(pixelCount * 4);
		for (std::size_t i = 0; i < pixelCount; ++i) {
			const std::uint32_t alpha = source[i * 4 + 3];
			result[i * 4 + 0] = static_cast<std::uint8_t>((source[i * 4 + 0] * alpha + 127) / 255);
			result[i * 4 + 1] = static_cast<std::uint8_t>((source[i * 4 + 1] * alpha + 127) / 255);
			result[i * 4 + 2] = static_cast<std::uint8_t>((source[i * 4 + 2] * alpha + 127) / 255);
			result[i * 4 + 3] = static_cast<std::uint8_t>(alpha);
		}
		return result;
	}

	void OpenGLTextureBackend::ConfigureTexture(unsigned int name, bool mipmapped) {
		mApi->BindTexture(GL_TEXTURE_2D, name);
		mApi->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		mApi->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		mApi->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		mApi->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	OpenGLTexture* OpenGLTextureBackend::AdoptTexture(unsigned int name, int width, int height) {
		auto texture = std::make_unique<OpenGLTexture>();
		texture->backend = RendererBackend::OpenGL;
		texture->bindingId = name;
		texture->width = width;
		texture->height = height;
		texture->name = name;
		OpenGLTexture* raw = texture.get();
		mTextures.emplace(name, std::move(texture));
		return raw;
	}

	OpenGLTexture* OpenGLTextureBackend::CreateTextureRGBA8(
		int width, int height, const void* pixels) {
		if (!mApi || width <= 0 || height <= 0 || !pixels
			|| width > mMaxTextureSize || height > mMaxTextureSize) return nullptr;

		const std::vector<std::uint8_t> premultiplied = PremultiplyRGBA8(width, height, pixels);
		unsigned int name = 0;
		mApi->GenTextures(1, &name);
		if (!name) return nullptr;
		ConfigureTexture(name, true);
		mApi->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
		mApi->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, premultiplied.data());
		mApi->GenerateMipmap(GL_TEXTURE_2D);
		mApi->BindTexture(GL_TEXTURE_2D, 0);

		if (mApi->GetError() != GL_NO_ERROR) {
			mApi->DeleteTextures(1, &name);
			return nullptr;
		}
		return AdoptTexture(name, width, height);
	}

	bool OpenGLTextureBackend::UpdateTextureRGBA8(RenderTexture* texture, int x, int y,
		int width, int height, const void* pixels) {
		if (!mApi || !texture || texture->backend != RendererBackend::OpenGL || !pixels
			|| x < 0 || y < 0 || width <= 0 || height <= 0
			|| x + width > texture->width || y + height > texture->height) return false;

		const std::vector<std::uint8_t> premultiplied = PremultiplyRGBA8(width, height, pixels);
		mApi->BindTexture(GL_TEXTURE_2D, texture->bindingId);
		mApi->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
		mApi->TexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height,
			GL_RGBA, GL_UNSIGNED_BYTE, premultiplied.data());
		mApi->GenerateMipmap(GL_TEXTURE_2D);
		mApi->BindTexture(GL_TEXTURE_2D, 0);
		return mApi->GetError() == GL_NO_ERROR;
	}

	void OpenGLTextureBackend::DestroyTexture(RenderTexture* texture) {
		if (!mApi || !texture || texture->backend != RendererBackend::OpenGL) return;
		const unsigned int name = texture->bindingId;
		auto found = mTextures.find(name);
		if (found == mTextures.end() || found->second.get() != texture) return;
		mApi->DeleteTextures(1, &name);
		mTextures.erase(found);
	}

	OpenGLTexture* OpenGLTextureBackend::CreateAtlasPage(
		int width, int height, const std::vector<AtlasCopy>& copies) {
		if (!mApi || width <= 0 || height <= 0 || copies.empty()
			|| width > mMaxTextureSize || height > mMaxTextureSize) return nullptr;

		unsigned int pageName = 0;
		mApi->GenTextures(1, &pageName);
		if (!pageName) return nullptr;
		ConfigureTexture(pageName, true);
		mApi->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		unsigned int readFramebuffer = 0;
		unsigned int drawFramebuffer = 0;
		mApi->GenFramebuffers(1, &readFramebuffer);
		mApi->GenFramebuffers(1, &drawFramebuffer);
		mApi->BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
		mApi->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, pageName, 0);
		if (mApi->CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("OpenGLTexture") << "图集页 framebuffer 不完整";
			mApi->BindFramebuffer(GL_FRAMEBUFFER, 0);
			mApi->DeleteFramebuffers(1, &readFramebuffer);
			mApi->DeleteFramebuffers(1, &drawFramebuffer);
			mApi->DeleteTextures(1, &pageName);
			return nullptr;
		}

		mApi->Disable(GL_SCISSOR_TEST);
		mApi->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		mApi->Clear(GL_COLOR_BUFFER_BIT);

		auto blit = [&](int sx0, int sy0, int sx1, int sy1,
			int dx0, int dy0, int dx1, int dy1) {
			mApi->BlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
		};

		for (const AtlasCopy& copy : copies) {
			if (!copy.source || copy.source->backend != RendererBackend::OpenGL) continue;
			mApi->BindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
			mApi->FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, copy.source->bindingId, 0);
			mApi->BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
			if (mApi->CheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				LOG_ERROR("OpenGLTexture") << "图集源 framebuffer 不完整";
				continue;
			}

			const int x = copy.x;
			const int y = copy.y;
			const int w = copy.width;
			const int h = copy.height;
			const int p = copy.padding;
			blit(0, 0, w, h, x, y, x + w, y + h);
			if (p > 0) {
				// 把四条边和四个角外扩进 padding，避免 mipmap/双线性采样渗入透明间隙。
				blit(0, 0, 1, h, x - p, y, x, y + h);
				blit(w - 1, 0, w, h, x + w, y, x + w + p, y + h);
				blit(0, 0, w, 1, x, y - p, x + w, y);
				blit(0, h - 1, w, h, x, y + h, x + w, y + h + p);
				blit(0, 0, 1, 1, x - p, y - p, x, y);
				blit(w - 1, 0, w, 1, x + w, y - p, x + w + p, y);
				blit(0, h - 1, 1, h, x - p, y + h, x, y + h + p);
				blit(w - 1, h - 1, w, h, x + w, y + h, x + w + p, y + h + p);
			}
		}

		mApi->BindFramebuffer(GL_FRAMEBUFFER, 0);
		mApi->DeleteFramebuffers(1, &readFramebuffer);
		mApi->DeleteFramebuffers(1, &drawFramebuffer);
		mApi->BindTexture(GL_TEXTURE_2D, pageName);
		mApi->GenerateMipmap(GL_TEXTURE_2D);
		mApi->BindTexture(GL_TEXTURE_2D, 0);
		if (mApi->GetError() != GL_NO_ERROR) {
			mApi->DeleteTextures(1, &pageName);
			return nullptr;
		}
		return AdoptTexture(pageName, width, height);
	}

} // namespace pvz
