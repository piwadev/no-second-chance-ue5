"""Build the SourceAssets release-asset zip that Tools/Setup.bat downloads (keeps the git repo / LFS quota small).

    python Tools/make_asset_pack.py [--version 1] [--out release/out]

Packs only what the editor bootstrap (Content/Python) imports: normalised weapon GLBs, baked props, characters, PBR
textures (no displacement maps), HDRIs, audio, FX, decals, fonts and the manifests. Raw downloads (Props/PolyHaven,
Props/3DAssets, weapon FBX/originals) stay out. Output: NSC-SourceAssets-v<N>.zip + .zip.sha256 (paths start with
SourceAssets/ so Setup.bat can extract it straight into the project folder).
Upload (data only, not a game release):
    gh release create assets-v1 release/out/NSC-SourceAssets-v1.zip* --prerelease --title "Source assets v1" --notes "..."
"""
import argparse, hashlib, os, sys, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SA = os.path.join(ROOT, 'SourceAssets')
STORE = {'.jpg', '.jpeg', '.png', '.ogg', '.zip'}


def wanted(rel):
    p = rel.replace('\\', '/'); parts = p.split('/'); low = p.lower()
    if parts[0] in ('_raw', 'tmp'): return False
    if parts[0] == 'Props': return len(parts) > 1 and parts[1] == 'Baked'
    if parts[0] == 'Weapons':
        name = parts[-1]
        return name.startswith('SM_') and name.endswith('.glb') or name.startswith('preview_') or name == 'material.json'
    if parts[0] == 'Textures' and '_disp_' in low: return False
    return True


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--version', default='1'); ap.add_argument('--out', default=os.path.join(ROOT, 'release', 'out'))
    a = ap.parse_args()
    if not os.path.isdir(SA): sys.exit('SourceAssets/ not found')
    os.makedirs(a.out, exist_ok=True)
    out = os.path.join(a.out, 'NSC-SourceAssets-v%s.zip' % a.version)
    files = []
    for dp, dn, fn in os.walk(SA):
        dn.sort()
        for f in sorted(fn):
            full = os.path.join(dp, f); rel = os.path.relpath(full, SA)
            if wanted(rel): files.append((full, rel))
    raw = sum(os.path.getsize(f) for f, _ in files)
    print('packing %d files, %.0f MB' % (len(files), raw / 1048576))
    with zipfile.ZipFile(out, 'w', allowZip64=True) as z:
        for full, rel in files:
            ext = os.path.splitext(full)[1].lower()
            z.write(full, 'SourceAssets/' + rel.replace('\\', '/'), compress_type=zipfile.ZIP_STORED if ext in STORE else zipfile.ZIP_DEFLATED, compresslevel=6)
    h = hashlib.sha256()
    with open(out, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    with open(out + '.sha256', 'w', newline='\n') as f: f.write('%s  %s\n' % (h.hexdigest(), os.path.basename(out)))
    size = os.path.getsize(out)
    print('%s  %.0f MB  sha256 %s' % (out, size / 1048576, h.hexdigest()))
    if size > 2 * 1024 ** 3 - 1: print('WARNING: over GitHub\'s 2 GB per-asset limit')


if __name__ == '__main__':
    main()
