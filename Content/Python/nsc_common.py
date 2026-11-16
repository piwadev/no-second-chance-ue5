"""Shared helpers for the NO SECOND CHANCE editor bootstrap (UE 5.4 / 5.5 Python)."""
import os
import traceback
import unreal

PROJECT_DIR = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = os.path.join(PROJECT_DIR, 'SourceAssets')
ROOT = '/Game/NSC'

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

REPORT = {'ok': [], 'warn': []}


def log(msg):
    unreal.log('[NSC] ' + msg)


def warn(msg):
    REPORT['warn'].append(msg)
    unreal.log_warning('[NSC] ' + msg)


def step(fn):
    """Run one bootstrap step; a failure is logged and the rest of the setup continues."""
    def wrapper(*a, **kw):
        log('---- ' + fn.__name__)
        try:
            r = fn(*a, **kw)
            REPORT['ok'].append(fn.__name__)
            return r
        except Exception:
            warn(fn.__name__ + ' failed:\n' + traceback.format_exc())
    wrapper.__name__ = fn.__name__
    return wrapper


def src(*parts):
    return os.path.join(SOURCE, *parts)


def exists(path):
    return EAL.does_asset_exist(path)


def obj_path(package_path):
    name = package_path.rsplit('/', 1)[-1]
    return package_path + '.' + name


def load(package_path):
    return unreal.load_asset(obj_path(package_path)) if exists(package_path) else None


def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)


def import_files(files, dest, name=None, replace=True):
    """Synchronous import through AssetTools (Interchange where the engine uses it). Returns imported object paths."""
    ensure_dir(dest)
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property('filename', f)
        t.set_editor_property('destination_path', dest)
        if name and len(files) == 1:
            t.set_editor_property('destination_name', name)
        t.set_editor_property('automated', True)
        t.set_editor_property('replace_existing', replace)
        t.set_editor_property('save', False)
        try:
            t.set_editor_property('async_', False)
        except Exception:
            pass
        tasks.append(t)
    asset_tools.import_asset_tasks(tasks)
    out = []
    for t in tasks:
        try:
            out += [str(p) for p in t.get_editor_property('imported_object_paths')]
        except Exception:
            pass
    return out


def assets_in(folder, cls=None, recursive=True):
    """Loaded assets under a content folder, optionally filtered by unreal class."""
    res = []
    for p in EAL.list_assets(folder, recursive=recursive, include_folder=False):
        a = unreal.load_asset(p)
        if a is not None and (cls is None or isinstance(a, cls)):
            res.append(a)
    return res


def move(asset, dest_package_path):
    """Rename/move an asset to /Game/... path (package path, no object suffix). Replaces an existing one."""
    cur = asset.get_path_name().split('.')[0]
    if cur == dest_package_path:
        return asset
    if exists(dest_package_path):
        EAL.delete_asset(dest_package_path)
    ensure_dir(dest_package_path.rsplit('/', 1)[0])
    if not EAL.rename_asset(cur, dest_package_path):
        warn('rename failed %s -> %s' % (cur, dest_package_path))
        return asset
    return unreal.load_asset(obj_path(dest_package_path))


def create_asset(name, folder, cls, factory):
    path = folder + '/' + name
    if exists(path):
        EAL.delete_asset(path)
    ensure_dir(folder)
    return asset_tools.create_asset(name, folder, cls, factory)


def save_all():
    EAL.save_directory(ROOT, only_if_is_dirty=False, recursive=True)


def cm(v):
    return v * 100.0


def to_ue(bx, bz, by=0.0):
    """Browser (three.js metres: x right, y up, -z forward) -> Unreal cm (X forward, Y right, Z up)."""
    return unreal.Vector(-bz * 100.0, bx * 100.0, by * 100.0)
