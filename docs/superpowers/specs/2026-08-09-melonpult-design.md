# Melon-pult implementation design

> 2026-08-14 current-balance note: the authoritative repository values are now 325 sun,
> 10 seconds recharge, 120 direct damage, and 40 secondary splash. The 300/7.5/80/26
> values below describe the original 2026-08-09 design snapshot and must not be used as
> current AutoTest expectations.

## Scope

Implement the classic Melon-pult as a normal placeable plant using the supplied `Melonpult.reanim`, the owner-confirmed global fire frame 44, and `Melonpult_melon.png`. The implementation includes its projectile, row-adjacent splash, resources, save state, object-pool reset, Adventure 5-7 reward integration, and focused AutoTest coverage.

## Gameplay contract

- Cost 300 sun, recharge 7.5 seconds, base health 300.
- Random initial attack phase from 0 to 3 seconds. Later attacks repeat every 2.86 to 3.0 seconds and scale with the survival attack-speed perk.
- Acquire the nearest legal, headed, non-charmed ground zombie ahead in the plant's own row.
- Play `anim_shooting` at 35 fps and create the projectile only at global reanimation frame 44. The callback is guarded by the active track so unrelated tracks cannot fire it.
- Launch from the stable plant visual anchor plus `(-1, -79)`. This is the current-scene conversion of the C# `(+25, -46)` top-left launch position, using the same `(-26, -33)` anchor conversion already established by Cabbage-pult and Kernel-pult.
- Predict the target's horizontal position 1.2 seconds ahead and compensate Y against the Board-owned continuous roof slope. Freeze this landing point at the fire event. The analytic lob lasts 1.2 seconds with a 210 px apex.

## Damage and interaction matrix

- The directly collided zombie takes 80 damage.
- Other legal ground zombies in the impact row and the two adjacent rows take `floor(80 / 3) = 26` damage when their collider overlaps the 60 px horizontal impact window.
- Snapshot targets before applying damage so deaths and entity removal cannot invalidate row-bucket iteration.
- Preserve the original secondary-damage budget: total secondary damage is capped at seven times direct damage, with a minimum of 1 damage per secondary target under extreme crowding.
- Melon impact uses projectile shield penetration: a second-layer shield takes the hit and the same damage also reaches the zombie body. Helmets remain normal first-layer protection. This is deliberately different from Cabbage-pult's request to bypass ordinary shields untouched.
- Charmed, dying, inactive, or airborne zombies are excluded from secondary splash. A direct collision target remains the direct target.
- Emit one `MelonSplash` effect and request one of the two `melonimpact` Foley sounds per hit. A missed landing emits the same audiovisual feedback without damage.

## Resources and persistence

- Register `Melonpult.reanim`, card image, melon projectile image, the nine-frame `Melonpult_particles.png` strip, `MelonSplash.xml`, and both impact sounds through their authoritative loaders and exact runtime keys.
- Draw only the Melon-pult card art at 0.80 of the normal card-art scale with final UI offset `(+3,+1)`; keep world animation scale unchanged. Move only the melon projectile ground shadow 6 px right after visible calibration.
- Add `ANIM_MELONPULT` only at the end of the animation enum to preserve old save values.
- Use the existing `BULLET_MELON` enum slot and add it to the generic bullet pool. Reset damage, presentation, spin, and lob state on every acquire.
- Save and restore the plant's shoot timer and interval. Existing generic bullet serialization covers the melon type, frozen landing point, arc progress, damage, rotation, and pool type.

## Acceptance coverage

- Resource and simulation metadata probes all pass.
- Frame 44 is proven with a before/after bullet-count boundary.
- Direct and adjacent-row health deltas prove 80/26/26 damage while a row outside the radius is unchanged.
- A shield target proves shield and body penetration semantics.
- In-flight save/load preserves pool type, 1.2-second duration, target, arc state, damage, and rotating presentation.
- A missed landing proves sound, particle, and pool reuse/reset behavior.
- Day and roof screenshots show plant, held melon, flying melon, shadow, impact cloud, and pot/roof alignment.
- Adventure internal level 43 (5-7) exposes the existing Melon-pult reward mapping.
