#pragma once

#include <string>

namespace SaveLocation {
	/**
	 * \brief 返回当前平台推荐的存档根目录。
	 *
	 * Windows 使用系统“保存的游戏”，Android 使用应用私有目录，其他平台及系统 API
	 * 失败时返回调用方提供的旧目录。Windows API 被隔离在 .cpp，避免宏污染游戏代码。
	 */
	std::string DiscoverPreferredRoot(const std::string& legacyRoot);
}
