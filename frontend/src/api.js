/**
 * Thin wrapper around the Wails Go bindings. All call sites go through here so
 * the backend-availability guard and error normalization live in one place
 * instead of being repeated with window.go?.app?.App?.X optional chaining.
 */

function bind(name) {
    return (...args) => {
        const go = window.go?.app?.App;
        if (!go || typeof go[name] !== 'function') {
            return Promise.reject(new Error(`Go backend unavailable: ${name}`));
        }
        return Promise.resolve(go[name](...args));
    };
}

export const api = {
    getTodayStats: bind('GetTodayStats'),
    getDateRangeStats: bind('GetDateRangeStats'),
    resetStats: bind('ResetStats'),
    getConfig: bind('GetConfig'),
    setConfig: bind('SetConfig'),
    saveWindowSize: bind('SaveWindowSize'),
    getSystemFonts: bind('GetSystemFonts'),
    setPaused: bind('SetPaused'),
    startDrag: bind('StartDrag'),
};

export default api;
