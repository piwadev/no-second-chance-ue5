// Bridge exposed to the game page inside the launcher: file-based saves instead of localStorage.
const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('nscNative', {
  loadSave: () => ipcRenderer.sendSync('nsc:loadSave'),
  saveSync: (text, reason) => ipcRenderer.sendSync('nsc:saveSync', text, reason),
  resetSave: () => ipcRenderer.sendSync('nsc:resetSave'),
  info: () => ipcRenderer.sendSync('nsc:info'),
});
