// All on-disk locations. The Unreal game keeps its SaveGame slots in %LOCALAPPDATA%\NoSecondChance\Saved\SaveGames
// (packaged-build default), never inside the versioned game folders, so updating / rolling back can never touch them.
// The launcher keeps rotating backups of those slots under %APPDATA%\NoSecondChance\saves\backups.
const path = require('path');
const os = require('os');
const { app } = require('electron');

const roaming = process.env.NSC_ROAMING || path.join(app.getPath('appData'), 'NoSecondChance');
const local = process.env.NSC_LOCAL || (process.platform === 'win32'
  ? path.join(process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local'), 'NoSecondChance')
  : path.join(process.env.XDG_DATA_HOME || path.join(os.homedir(), '.local', 'share'), 'NoSecondChance'));

module.exports = {
  roaming, local,
  userData: path.join(roaming, 'launcher'),
  savesDir: path.join(roaming, 'saves'),
  ueSaveDir: process.env.NSC_UE_SAVES || path.join(local, 'Saved', 'SaveGames'),
  saveFile: path.join(process.env.NSC_UE_SAVES || path.join(local, 'Saved', 'SaveGames'), 'NSCSettings.sav'),
  backupsDir: path.join(roaming, 'saves', 'backups'),
  settingsFile: path.join(roaming, 'launcher-settings.json'),
  gameDir: path.join(local, 'game'),
  stateFile: path.join(local, 'state.json'),
  downloadsDir: path.join(local, 'downloads'),
  logFile: path.join(roaming, 'launcher', 'launcher.log'),
};
