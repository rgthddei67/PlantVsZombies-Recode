#include "InternedString.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_set>

namespace {
	/** 统一驻留池；unordered_set 扩容不会使元素引用或指针失效。 */
	class RuntimeStringPool {
	private:
		std::shared_mutex mMutex;
		std::unordered_set<std::string> mValues;

	public:
		RuntimeStringPool()
		{
			mValues.reserve(256);
		}

		const std::string& Intern(const std::string& value)
		{
			// 动画 worker 上的 PlayTrack 绝大多数只读取既有名称，共享锁避免同步转轨串行化。
			{
				std::shared_lock<std::shared_mutex> readLock(mMutex);
				const auto found = mValues.find(value);
				if (found != mValues.end()) return *found;
			}
			std::unique_lock<std::shared_mutex> writeLock(mMutex);
			return *mValues.insert(value).first;
		}
	};
}

const std::string& InternRuntimeString(const std::string& value)
{
	static RuntimeStringPool pool;
	return pool.Intern(value);
}
