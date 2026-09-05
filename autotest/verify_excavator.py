"""Compare saved AutoTest states for timing, reservation, forecast and unlock contracts."""
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'build/clang-release/autotest/out'


def state(suite, name):
    return json.loads((OUT / suite / (name + '.json')).read_text(encoding='utf-8'))


def verify():
    """Check same-state projections; never depend on wall-clock sampled absolute positions."""
    main = 'smoke_excavator'
    before = state(main, 'paused_before')['zombies'][0]['excavatorWorkMs']
    assert state(main, 'paused_after')['zombies'][0]['excavatorWorkMs'] == before
    slow = state(main, 'slow_before')['zombies'][0]['excavatorWorkMs']
    assert abs(slow - state(main, 'slow_after')['zombies'][0]['excavatorWorkMs'] - 1000) <= 20
    edges = 'smoke_excavator_edges'
    reserved = state(edges, 'reserved_two')['zombies']
    restored = state(edges, 'reserved_restored')['zombies']
    assert [z['excavatorWall'] for z in reserved] == [11, -1]
    fields = ('id', 'excavatorWall', 'excavatorStand', 'excavatorPhase')
    assert [[z[f] for f in fields] for z in reserved] == [[z[f] for f in fields] for z in restored]
    assert state(edges, 'reservation_released')['zombies'][0]['excavatorWall'] == -1
    waves = 'smoke_excavator_waves'
    for wave in range(1, 21):
        plan = state(waves, f'plan_{wave:02}')['mine']['wavePlan']
        counts = Counter(t for t, row in plan)
        assert set(counts) <= ({0, 1, 3} if wave < 3 else {0, 1, 3, 52} if wave == 3 else {0, 1, 3, 11, 24, 52})
        assert counts[52] <= 1 and counts[24] <= 2, (wave, counts)
        if wave == 3:
            assert counts[52] == 1
        assert all(row in (1, 3) for t, row in plan)
    actual = state(waves, 'all_waves')['zombies']
    excavators = Counter(z['spawnWave'] for z in actual if z['type'] == 'ZOMBIE_EXCAVATOR')
    assert excavators[3] == 1 and all(n <= 1 for n in excavators.values())
    assert state(waves, 'actual_tutorial')['mine']['wavePlan'] == state(waves, 'plan_04')['mine']['wavePlan']
    assert state(waves, 'death_before_assert')['zombieCount'] == 0
    assert 'ZOMBIE_EXCAVATOR' not in state(waves, 'almanac_before')['zombieAlmanacEntries']
    assert 'ZOMBIE_EXCAVATOR' in state(waves, 'almanac_after')['zombieAlmanacEntries']
    smoke = state('smoke_excavator_visual', 'smoke')['particleEffectsByName']['ExcavatorSmoke'][0]
    assert smoke['renderQuadCount'] == 3 and smoke['worldBounds']['widthInt'] >= 20
    assert smoke['worldBounds']['heightInt'] >= 15
    assert state('smoke_excavator_almanac', 'selected')['zombieAlmanacSelected'] == 'ZOMBIE_EXCAVATOR'
    print('Excavator timing, reservations, 20 waves, death, unlock, preview and smoke geometry passed')


if __name__ == '__main__':
    verify()
