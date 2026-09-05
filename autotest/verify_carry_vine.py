"""Check preserved entity/state contracts in the visible CarryVine smoke dumps."""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'build/clang-release/autotest/out/smoke_carry_vine'
def read(name):
    return json.loads((OUT / (name + '.json')).read_text(encoding='utf-8'))

def plants(name):
    return {p['id']:p for p in read(name)['plants']}

for before, after in [('eating_before','eating_after'),('eating_after','eating_later'),
                      ('pool_before','pool_after'),('pool_after','pool_restored'),
                      ('cob_before','cob_after'),('cob_after','cob_restored')]:
    a,b=plants(before),plants(after)
    assert a.keys()==b.keys(), (before, 'entity IDs changed')
    for key, old in a.items():
        new=b[key]
        for field in ('type','health','maxHealth','sleeping','unyieldingRootsSpent'):
            assert old[field]==new[field], (before,key,field,old[field],new[field])
        if old['type']=='PLANT_COBCANNON':
            for field in ('cobPhase','cobShotLaunched'):
                assert old[field]==new[field], (before,key,field)
            # Snapshot is after one screenshot barrier; a fixed-step advance is expected.
            elapsed = old['cobArmingTimeMs'] - new['cobArmingTimeMs']
            assert 0 <= elapsed <= (34 if after.endswith('restored') else 0), (before,elapsed)
        if old['type']=='PLANT_SUNFLOWER':
            for field in ('produceIntervalMs','produceSunCount'):
                assert old[field]==new[field], (before,key,field)
            elapsed = new['produceTimerMs'] - old['produceTimerMs']
            assert 0 <= elapsed <= (34 if after.endswith('restored') else 0), (before,elapsed)
    print(before,'->',after,': stable IDs, health and ability state passed')

cob=read('cob_after')['normalPlantsByCell']
assert cob['3_7']['id']==cob['3_8']['id']
print('multi-cell alias identity passed')
