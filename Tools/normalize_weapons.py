"""Runs Tools/blender/normalize_weapon.py for every weapon in Tools/blender/weapon_fit.json and writes
SourceAssets/Weapons/<Id>/SM_<Id>.glb + preview_<Id>.png.  Usage: python Tools/normalize_weapons.py [blender exe]"""
import json, os, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
blender = sys.argv[1] if len(sys.argv) > 1 else 'blender'
fit = json.load(open(os.path.join(ROOT, 'Tools', 'blender', 'weapon_fit.json')))
for wid, cfg in fit.items():
    if wid.startswith('_'): continue
    wdir = os.path.join(ROOT, 'SourceAssets', 'Weapons', wid)
    ov = {k: v for k, v in cfg.items() if k != 'length'}
    r = subprocess.run([blender, '-b', '-P', os.path.join(ROOT, 'Tools', 'blender', 'normalize_weapon.py'), '--', wdir, wid, str(cfg['length']), json.dumps(ov)], capture_output=True, text=True)
    print(next((l for l in r.stdout.splitlines() if l.startswith('NSC_')), wid + ' FAILED\n' + r.stdout[-800:] + r.stderr[-800:]))
