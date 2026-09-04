#pragma once
#ifndef _UIMANAGER_H
#define _UIMANAGER_H
#include "../ResourceManager.h"
#include "ButtonManager.h"
#include "SliderManager.h"
#include "GameMessageBox.h"
#include "../Graphics.h"
#include <algorithm>
#include <vector>

class UIManager
{
private:
	ButtonManager buttonManager;
	SliderManager sliderManager;
	std::vector<std::shared_ptr<GameMessageBox>> messageBoxes;

	// 控件回调可能请求关闭弹窗；统一等 Button/Slider 完成遍历后再解除注册。
	void FlushClosedMessageBoxes()
	{
		for (const auto& messageBox : messageBoxes) {
			if (messageBox && messageBox->IsCloseRequested()) {
				messageBox->DetachControls();
			}
		}
		messageBoxes.erase(
			std::remove_if(messageBoxes.begin(), messageBoxes.end(),
				[](const std::shared_ptr<GameMessageBox>& messageBox) {
					return !messageBox || messageBox->IsCloseRequested();
				}),
			messageBoxes.end());
	}

public:
	~UIManager()
	{
		ClearAll();
	}

	std::shared_ptr<Button> CreateButton(Vector pos = Vector::zero(), Vector size = Vector(40, 40))
	{
		return buttonManager.CreateButton(pos, size);
	}

	void AddMessageBox(const std::shared_ptr<GameMessageBox>& messageBox)
	{
		if (messageBox) messageBoxes.push_back(messageBox);
	}

	/** 返回当前最上层的活动模态框；没有时返回 nullptr。 */
	std::shared_ptr<GameMessageBox> GetTopActiveMessageBox() const
	{
		for (auto it = messageBoxes.rbegin(); it != messageBoxes.rend(); ++it) {
			if (*it && (*it)->IsActive()) return *it;
		}
		return nullptr;
	}

	void RemoveButton(std::shared_ptr<Button> button)
	{
		buttonManager.RemoveButton(button);
	}

	void ClearAllButtons()
	{
		buttonManager.ClearAllButtons();
	}

	size_t GetButtonCount() const
	{
		return buttonManager.GetButtonCount();
	}

	std::shared_ptr<Button> GetButton(size_t index) const
	{
		return buttonManager.GetButton(index);
	}

	std::shared_ptr<Slider> CreateSlider(Vector pos = Vector::zero(),
		Vector size = Vector(135, 10),
		float minVal = 0.0f,
		float maxVal = 1.0f,
		float initialValue = 0.5f)
	{
		return sliderManager.CreateSlider(pos, size, minVal, maxVal, initialValue);
	}

	void RemoveSlider(std::shared_ptr<Slider> slider)
	{
		sliderManager.RemoveSlider(slider);
	}

	void ClearAllSliders()
	{
		sliderManager.ClearAllSliders();
	}

	size_t GetSliderCount() const
	{
		return sliderManager.GetSliderCount();
	}

	std::shared_ptr<Slider> GetSlider(size_t index) const
	{
		return sliderManager.GetSlider(index);
	}

	void ProcessMouseEvent(InputHandler* input)
	{
		buttonManager.ProcessMouseEvent(input);
		sliderManager.ProcessMouseEvent(input);
	}

	void UpdateAll(InputHandler* input)
	{
		buttonManager.UpdateAll(input);
		sliderManager.UpdateAll(input);
		FlushClosedMessageBoxes();
	}

	void DrawAll(Graphics* g) const
	{
		// 普通控件先绘制；模态框连同其自有控件最后提交，稳定覆盖场景内其他 UI。
		buttonManager.DrawAll(g);
		sliderManager.DrawAll(g);
		for (const auto& messageBox : messageBoxes) {
			if (messageBox && messageBox->IsActive()) messageBox->Draw(g);
		}
	}

	void ResetAllFrameStates()
	{
		buttonManager.ResetAllFrameStates();
	}

	void ClearAll()
	{
		for (const auto& messageBox : messageBoxes) {
			if (messageBox) messageBox->DetachControls();
		}
		messageBoxes.clear();
		buttonManager.ClearAllButtons();
		sliderManager.ClearAllSliders();
	}

	ButtonManager& GetButtonManager() { return buttonManager; }
	SliderManager& GetSliderManager() { return sliderManager; }
	const ButtonManager& GetButtonManager() const { return buttonManager; }
	const SliderManager& GetSliderManager() const { return sliderManager; }
};

#endif
