#pragma once

#include "RenderBackend.h"

#include <memory>
#include <unordered_map>

namespace pvz {

	struct OpenGLApi;

	struct OpenGLTexture final : RenderTexture {
		unsigned int name = 0;
	};

	/** OpenGL Context 线程上的普通 RGBA8 纹理管理器。 */
	class OpenGLTextureBackend final : public TextureBackend {
	public:
		OpenGLTextureBackend() = default;
		~OpenGLTextureBackend() override;

		OpenGLTextureBackend(const OpenGLTextureBackend&) = delete;
		OpenGLTextureBackend& operator=(const OpenGLTextureBackend&) = delete;

		bool Initialize(OpenGLApi* api);
		void Shutdown();

		RendererBackend Backend() const override { return RendererBackend::OpenGL; }
		OpenGLTexture* CreateTextureRGBA8(int width, int height, const void* pixels) override;
		bool UpdateTextureRGBA8(RenderTexture* texture, int x, int y,
			int width, int height, const void* pixels) override;
		void DestroyTexture(RenderTexture* texture) override;
		int MaxTextureSize() const override { return mMaxTextureSize; }
		OpenGLTexture* CreateAtlasPage(int width, int height,
			const std::vector<AtlasCopy>& copies) override;

	private:
		static std::vector<std::uint8_t> PremultiplyRGBA8(
			int width, int height, const void* pixels);
		OpenGLTexture* AdoptTexture(unsigned int name, int width, int height);
		void ConfigureTexture(unsigned int name, bool mipmapped);

		OpenGLApi* mApi = nullptr;
		int mMaxTextureSize = 0;
		std::unordered_map<unsigned int, std::unique_ptr<OpenGLTexture>> mTextures;
	};

} // namespace pvz
