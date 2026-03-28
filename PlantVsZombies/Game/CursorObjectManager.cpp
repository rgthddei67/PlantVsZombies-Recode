#include "CursorObjectManager.h"

void CursorObjectManager::Activate(CursorObjectType type, std::function<void()> clearCallback)
{
	if (mActiveType == type)
		return;

	ClearActive();
	mActiveType = type;
	mClearCallback = std::move(clearCallback);
}

void CursorObjectManager::ClearActive()
{
	if (mActiveType != CursorObjectType::NONE && mClearCallback) {
		mClearCallback();
	}
	mActiveType = CursorObjectType::NONE;
	mClearCallback = nullptr;
}
