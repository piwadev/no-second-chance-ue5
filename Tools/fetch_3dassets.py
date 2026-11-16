"""Fetch CC0 gap-filler props from 3dassets.dev (gear, medical items, objective items, dockyard props).
Writes SourceAssets/Props/3DAssets/<Key>.glb and SourceAssets/_manifests/3dassets.json.
Usage: python Tools/fetch_3dassets.py   (re-runnable; skips files that exist)"""
import json, os, sys, urllib.request, urllib.parse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'SourceAssets', 'Props', '3DAssets')
MAN = os.path.join(ROOT, 'SourceAssets', '_manifests', '3dassets.json')
UA = {'User-Agent': 'NSC-asset-fetch/1.0'}

# key: (search query, words that must appear in the title (any))
WANT = {
    'Helmet': ('combat helmet', ['combat helmet', 'military helmet', 'tactical helmet', 'army helmet', 'ballistic helmet']),
    'PlateCarrier': ('plate carrier', ['plate carrier', 'vest']),
    'Backpack': ('military backpack', ['backpack', 'rucksack']),
    'Tourniquet': ('tourniquet', ['tourniquet']),
    'Bandage': ('bandage roll', ['bandage']),
    'Medkit': ('first aid kit', ['first aid', 'medkit', 'medical kit', 'trauma']),
    'DefuseKit': ('wire cutters tool kit', ['cutter', 'tool kit', 'toolkit', 'pliers']),
    'BreachingCharge': ('breaching charge', ['breach', 'charge']),
    'SecurityCamera': ('security camera', ['camera']),
    'Radio': ('military radio', ['radio']),
    'AmmoCrate': ('ammo crate', ['ammo', 'ammunition']),
    'Magazine': ('rifle magazine', ['magazine']),
    'RedDot': ('red dot sight', ['red dot', 'sight', 'optic']),
    'Scope': ('rifle scope', ['scope']),
    'Suppressor': ('suppressor silencer', ['suppressor', 'silencer']),
    'Flashlight': ('tactical flashlight', ['flashlight', 'torch']),
    'ShippingContainer': ('shipping container', ['container']),
    'Forklift': ('forklift', ['forklift']),
    'Pallet': ('wooden pallet', ['pallet']),
    'PalletJack': ('pallet jack', ['pallet jack', 'pallet truck']),
    'Bollard': ('mooring bollard', ['bollard']),
    'DockCrane': ('dock crane', ['crane']),
    'Ladder': ('steel ladder', ['ladder']),
    'Catwalk': ('steel catwalk', ['catwalk', 'walkway']),
    'Foregrip': ('vertical foregrip', ['foregrip', 'grip']),
    'RollerDoor': ('roller shutter door', ['roller', 'shutter']),
    'SandbagWall': ('sandbag wall', ['sandbag']),
    'ConcreteBarrier': ('concrete barrier', ['barrier', 'jersey']),
    'IBCTank': ('ibc tote container', ['ibc']),
    'Workbench': ('workbench', ['workbench', 'work bench']),
    'Locker': ('metal locker', ['locker']),
    'OfficeDesk': ('office desk', ['desk']),
    'Generator': ('diesel generator', ['generator']),
    'FloodLight': ('flood light', ['flood', 'floodlight', 'work light']),
    'CableReel': ('cable reel', ['reel', 'spool']),
}

def get_json(url):
    return json.loads(urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60).read())

def main():
    os.makedirs(OUT, exist_ok=True)
    manifest = []
    for key, (query, words) in WANT.items():
        dest = os.path.join(OUT, key + '.glb')
        try:
            res = get_json('https://3dassets.dev/api/v1/assets?' + urllib.parse.urlencode({'q': query, 'limit': 20}))['data']
        except Exception as e:  # noqa
            print('search failed', key, e); continue
        pick = None
        for a in res:
            lic = (a.get('license') or {}).get('slug', '')
            if lic != 'cc0-1.0':
                continue
            if any(w in a['title'].lower() for w in words):
                pick = a; break
        if not pick:
            print('no match', key); continue
        if not os.path.exists(dest):
            data = urllib.request.urlopen(urllib.request.Request(pick['cdnUrl'], headers=UA), timeout=120).read()
            open(dest, 'wb').write(data)
        manifest.append({'name': key, 'title': pick['title'], 'file': os.path.relpath(dest, ROOT).replace('\\', '/'),
                         'source': pick['url'], 'author': pick['contributor']['name'] + ' (3dassets.dev)',
                         'license': 'CC0-1.0', 'aiGenerated': pick.get('aiGenerated', False)})
        print(key, '->', pick['title'])
    os.makedirs(os.path.dirname(MAN), exist_ok=True)
    json.dump(manifest, open(MAN, 'w'), indent=1)
    print(len(manifest), 'assets')

if __name__ == '__main__':
    main()
