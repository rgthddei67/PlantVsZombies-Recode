# 炉芯花卡图来源

- `FurnaceCoreFlower_card_source.png`：2026-08-26 使用 Codex ImageGen 生成的透明原图。
- `FurnaceCoreFlower_core_source.png`：由同一卡图继续编辑得到的无火焰炉膛脸透明原图；两只空炉孔留给运行时状态 follower。
- 参考资产：原版 `SunFlower.png`、`TorchWood.png`、`Torchwood_body.png` 与 `Torchwood_fire1a.png`。
- 提示词目标：经典 PvZ 手绘卡图风格；橙红花瓣、木质炉芯面部、左右各一枚清晰火焰炉芯；透明背景，无文字、边框、水印与场景。
- `scripts/generate_furnace_core_flower_assets.ps1` 校验两张生成原图的 SHA-256，再生成 120x100 卡图及 57x43 炉芯面部分件。
