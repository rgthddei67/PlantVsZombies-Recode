---
name: project_pvz_chomper_rejected_bite
description: 2026-08-15 大嘴花拒吞伤害统一契约；默认20，特殊目标经虚入口调整，持杆跳跳为0
metadata:
  node_type: memory
  type: project
  updated_at: 2026-08-15
---

# 大嘴花拒吞伤害契约

## 当前实现（2026-08-15）

- `Zombie::TakePlantInstantKill()` 只回答目标是否确实被吞下。返回 true 时大嘴花进入 `anim_chew`；返回 false 时大嘴花保持 `anim_bite`，并把集中在 `Chomper::REJECTED_BITE_DAMAGE` 的默认 20 点传给目标。
- 目标可通过 `AdjustRejectedChomperBiteDamage(int)` 保留已有特殊数值，调整后仍统一走 `Zombie::TakeDamage(..., DamageSource::PLANT)`，因此植物增伤、僵尸免伤、护盾和受击层级保持正式语义。未来新增拒吞品种若没有明确特殊数值，不覆写此入口即可自动使用 20，禁止在每个僵尸类中复制默认常量。
- 普通巨人、红眼巨人和屋脊督军均拒吞并使用默认 20。既有特殊值保留：加固铁门持门时 10，镀金冰车 50。
- 跳跳僵尸持有弹跳器时因高度超出咬口而拒吞，并把伤害调整为 0；零伤害在 `TakeDamage` 入口直接返回，不消耗免伤次数、不触发受击白光。弹跳器脱落后 `TakePlantInstantKill()` 恢复基类行为，可被正常吞食；精英跳跳继承同一状态语义。

## 验证

- `clang-release` 编译、LTO 链接 exit 0，Win7 导入审计通过 378 项。
- 主人当前桌面可见 `smoke_chomper_rejected_bite.json` exit 0，82 条命令通过：普通僵尸被吞且进入 `anim_chew`；巨人 `3000→2980`、红眼 `6000→5980`、屋脊督军 `15000→14980`；镀金冰车保持特殊 50、加固铁门保持特殊 10；持杆跳跳保持 `500/500`、`hitFlashMask=0`、`renderedHitGlowMask=0`。四张同步截图已检查。
- 主人当前桌面可见 `smoke_roof_marshal_survivability.json` exit 0，105 条命令通过；大嘴花两口后屋脊督军为 `14960/15000`，其余灰烬、魅惑、水草与小推车抗性回归通过。
