// Save manager for the Unreal build. The game writes USaveGame slots (*.sav, "GVAS" header) itself; the launcher only
// backs them up (rotating, last 5), exports/imports the main slot and copies it to an optional cloud-synced folder.
const fs = require('fs');
const path = require('path');
const P = require('./paths');
const log = require('./log');

const KEEP = 5, MIN_BACKUP_GAP = 10 * 60 * 1000;
let lastBackup = 0;
const ts = () => { const d = new Date(), p = n => String(n).padStart(2, '0'); return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`; };
const ensure = () => { fs.mkdirSync(P.backupsDir, { recursive: true }); };

// accepts a Buffer or a path; returns an error string or null
function validate(data) {
  let b = data; try { if (typeof data === 'string') b = fs.readFileSync(data); } catch { return 'not readable'; }
  if (!Buffer.isBuffer(b) || b.length < 16 || b.slice(0, 4).toString('latin1') !== 'GVAS') return 'not an Unreal save game (.sav)';
  return null;
}
function atomicCopy(src, dst) { fs.mkdirSync(path.dirname(dst), { recursive: true }); const tmp = dst + '.tmp'; fs.copyFileSync(src, tmp); fs.renameSync(tmp, dst); }
function prune(prefix, keep) {
  try {
    const fl = fs.readdirSync(P.backupsDir).filter(f => f.startsWith(prefix)).map(f => ({ f, t: fs.statSync(path.join(P.backupsDir, f)).mtimeMs })).sort((a, b) => b.t - a.t);
    for (const x of fl.slice(keep)) fs.rmSync(path.join(P.backupsDir, x.f), { recursive: true, force: true });
  } catch (e) { log.warn('prune failed', e); }
}
const slots = () => { try { return fs.readdirSync(P.ueSaveDir).filter(f => f.toLowerCase().endsWith('.sav')); } catch { return []; } };
// one backup = a folder with every .sav slot
function backupNow(prefix = 'save', keep = KEEP) {
  const s = slots(); if (!s.length) return null;
  ensure(); const dst = path.join(P.backupsDir, `${prefix}-${ts()}`);
  try { fs.mkdirSync(dst, { recursive: true }); for (const f of s) fs.copyFileSync(path.join(P.ueSaveDir, f), path.join(dst, f)); prune(prefix + '-', keep); lastBackup = Date.now(); return dst; }
  catch (e) { log.warn('backup failed', e); return null; }
}
function maybeBackup() { if (Date.now() - lastBackup > MIN_BACKUP_GAP) return backupNow('save'); return null; }
function listBackups() {
  try { return fs.readdirSync(P.backupsDir).map(f => ({ file: path.join(P.backupsDir, f), name: f, time: fs.statSync(path.join(P.backupsDir, f)).mtimeMs })).sort((a, b) => b.time - a.time); } catch { return []; }
}
function info() {
  let size = 0, mtime = 0;
  for (const f of slots()) { try { const s = fs.statSync(path.join(P.ueSaveDir, f)); size += s.size; mtime = Math.max(mtime, s.mtimeMs); } catch { /* gone */ } }
  return { saveFile: P.saveFile, savesDir: P.ueSaveDir, exists: size > 0, size, mtime, backups: listBackups().length };
}
// ---- export / import of the main slot (launcher UI)
function exportTo(file) { if (!fs.existsSync(P.saveFile)) throw new Error('There is no save to export yet — play the game first.'); atomicCopy(P.saveFile, file); return file; }
function importFrom(file) {
  const err = validate(file); if (err) throw new Error('That file is ' + err + '.');
  backupNow('restore', 5); atomicCopy(file, P.saveFile); return true;
}
// ---- cloud-folder backup (any folder: Google Drive / OneDrive / Dropbox synced directory)
function cloudBackup(dir) {
  if (!dir) throw new Error('Choose a cloud backup folder first (Settings).');
  if (!fs.existsSync(P.saveFile)) throw new Error('There is no save to back up yet.');
  fs.mkdirSync(dir, { recursive: true });
  const name = `NSC-save-${ts()}.sav`; atomicCopy(P.saveFile, path.join(dir, name)); atomicCopy(P.saveFile, path.join(dir, 'NSC-save-latest.sav'));
  try { const old = fs.readdirSync(dir).filter(f => /^NSC-save-\d{8}-\d{6}\.sav$/.test(f)).sort().reverse(); for (const f of old.slice(10)) fs.unlinkSync(path.join(dir, f)); } catch { /* ignore */ }
  return path.join(dir, name);
}
function cloudLatest(dir) { const f = path.join(dir || '', 'NSC-save-latest.sav'); return dir && fs.existsSync(f) ? f : null; }
module.exports = { info, exportTo, importFrom, cloudBackup, cloudLatest, validate, backupNow, maybeBackup, listBackups };
