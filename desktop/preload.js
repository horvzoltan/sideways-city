const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('desktop', { quit: () => ipcRenderer.send('quit') });
