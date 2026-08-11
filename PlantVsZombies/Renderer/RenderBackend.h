#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pvz {

	/** 游戏实际使用的渲染后端；资源和上层绘制代码只依赖这个稳定枚举。 */
	enum class RendererBackend : std::uint8_t {
		Vulkan,
		OpenGL,
	};

	/** 命令行请求的后端选择策略。 */
	enum class RendererPreference : std::uint8_t {
		Auto,
		Vulkan,
		OpenGL,
	};

	inline const char* RendererBackendName(RendererBackend backend) {
		return backend == RendererBackend::OpenGL ? "opengl" : "vulkan";
	}

	inline const char* RendererPreferenceName(RendererPreference preference) {
		switch (preference) {
		case RendererPreference::Vulkan: return "vulkan";
		case RendererPreference::OpenGL: return "opengl";
		default: return "auto";
		}
	}

	/**
	 * 后端无关纹理句柄。bindingId 只允许对应后端内部消费；资源对象不得把它解释成
	 * Vulkan bindless slot、OpenGL name、descriptor 或设备地址。
	 */
	struct RenderTexture {
		RendererBackend backend = RendererBackend::Vulkan;
		std::uint32_t bindingId = 0;
		int width = 0;
		int height = 0;

		virtual ~RenderTexture() = default;
	};

	/** 由资源层规划、由具体纹理后端执行的一次图集页复制。 */
	struct AtlasCopy {
		RenderTexture* source = nullptr;
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
		int padding = 0;
	};

	/**
	 * 统一纹理生命周期入口。所有调用发生在持有渲染 Context 的主线程。
	 * Vulkan 可保留延迟删除和 bindless 快路径，OpenGL 可使用普通 texture name。
	 */
	class TextureBackend {
	public:
		virtual ~TextureBackend() = default;

		virtual RendererBackend Backend() const = 0;
		virtual RenderTexture* CreateTextureRGBA8(int width, int height, const void* pixels) = 0;
		virtual bool UpdateTextureRGBA8(RenderTexture* texture, int x, int y,
			int width, int height, const void* pixels) = 0;
		virtual void DestroyTexture(RenderTexture* texture) = 0;
		virtual int MaxTextureSize() const = 0;

		/** 不支持图集页复制的后端返回 nullptr；Vulkan bindless 路径不需要创建图集。 */
		virtual RenderTexture* CreateAtlasPage(int, int, const std::vector<AtlasCopy>&) {
			return nullptr;
		}
	};

	using CaptureTicket = std::uint64_t;

	enum class CaptureStatus {
		Unknown,
		Pending,
		Succeeded,
		Failed,
	};

	/** AutoTest 截图使用的统一异步 ticket 接口。 */
	class CaptureBackend {
	public:
		virtual ~CaptureBackend() = default;
		virtual CaptureTicket RequestCapture(const std::string& pngPath) = 0;
		virtual CaptureStatus GetCaptureStatus(CaptureTicket ticket) const = 0;
		virtual std::string GetCaptureError(CaptureTicket ticket) const = 0;
	};

} // namespace pvz
