#pragma once
#ifndef _CURSOR_OBJECT_MANAGER_H
#define _CURSOR_OBJECT_MANAGER_H

#include <functional>

enum class CursorObjectType {
	NONE,
	PLANT_PREVIEW,
	SHOVEL,
};

class CursorObjectManager {
public:
	CursorObjectManager() = default;

	void Activate(CursorObjectType type, std::function<void()> clearCallback);
	void ClearActive();
	CursorObjectType GetActiveType() const { return mActiveType; }
	bool IsActive(CursorObjectType type) const { return mActiveType == type; }

private:
	CursorObjectType mActiveType = CursorObjectType::NONE;
	std::function<void()> mClearCallback;
};

#endif
