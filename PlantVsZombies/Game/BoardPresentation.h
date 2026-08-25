#pragma once

#include "WeatherTypes.h"
#include "Plant/PlantType.h"
#include <string>
#include <unordered_map>
#include <utility>

using SurvivalCardCooldownMap =
	std::unordered_map<PlantType, std::pair<float, float>>;

/** 仅属于场景 UI 的天气瞬态；不承载天气玩法权威状态。 */
struct WeatherPresentationState {
	float currentWeatherNoticeTimer = 0.0f;
	float forecastFailureTimer = 0.0f;
	RainIntensity failedForecast = RainIntensity::CLEAR;
	RainIntensity actualForecast = RainIntensity::CLEAR;
	TyphoonStrength failedForecastTyphoon = TyphoonStrength::NONE;
	TyphoonStrength actualForecastTyphoon = TyphoonStrength::NONE;
};

/**
 * Board 向宿主场景发出的展示请求。
 *
 * Board 只保存该接口的非拥有指针；天气、波次和生存模式的玩法状态仍由
 * Board 唯一持有，场景不得通过此接口建立第二份玩法权威状态。
 */
class BoardPresentation {
public:
	virtual ~BoardPresentation() = default;

	virtual void ShowHeavyRainWarning(TyphoonStrength strength, int variant) = 0;
	/** 气象干扰提交时立即撤下仍在屏幕中央显示的大雨或暴雪预警。 */
	virtual void CancelHeavyRainWarning() = 0;
	virtual void ShowWeatherForecastFailure(
		RainIntensity forecast, RainIntensity actual,
		TyphoonStrength forecastTyphoon, TyphoonStrength actualTyphoon) = 0;
	virtual void ShowCurrentWeatherNotice() = 0;
	virtual void ShowLightningStrike(float duration = 0.42f) = 0;
	virtual void ShowScreenFlash(
		float duration = 0.5f, float peakAlpha = 200.0f) = 0;
	virtual void ShowPrompt(const std::string& textureKey,
		float appearDuration = 1.0f,
		float holdDuration = 3.0f,
		float fadeDuration = 1.0f) = 0;
	/** 屋脊督军锁定目标行时显示一次中央突击警报。 */
	virtual void ShowRoofMarshalAssaultWarning(int row, float duration) = 0;
	/** 路灯花燃料跌破安全线时显示不会受游戏倍速压缩的中央警报。 */
	virtual void ShowPlanternLowFuelWarning() = 0;
	virtual void ShowShovel() = 0;
	/** 点击路灯花本体后切换卡槽下方的挡位菜单。 */
	virtual void TogglePlanternGearMenu() = 0;
	virtual void GameOver() = 0;
	virtual void BeginSurvivalPerkSelect() = 0;
	virtual void SetReadyToBackMenu() = 0;

	/** 激活并按当前 Board 波数建立进度条。 */
	virtual void ActivateWaveProgress() = 0;
	/** 读档后按当前 Board 波数恢复进度条的旗子与滑块。 */
	virtual void RestoreWaveProgress() = 0;

	virtual WeatherPresentationState CaptureWeatherPresentationState() const = 0;
	virtual void RestoreWeatherPresentationState(
		const WeatherPresentationState& state) = 0;

	virtual const SurvivalCardCooldownMap& GetSurvivalCardCooldowns() const = 0;
	virtual void SetSurvivalCardCooldowns(
		SurvivalCardCooldownMap cooldowns) = 0;
};
