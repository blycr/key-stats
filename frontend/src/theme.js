/**
 * Theme manager — applies CSS data-theme attribute and Wails native window theme.
 */
export function applyTheme(theme) {
    const resolved = resolveTheme(theme);
    document.documentElement.setAttribute('data-theme', resolved);
    setNativeTheme(resolved);
}

export function setupAutoTheme(theme) {
    if (theme !== 'auto') return () => {};
    const mq = window.matchMedia('(prefers-color-scheme: light)');
    const handler = () => applyTheme('auto');
    mq.addEventListener('change', handler);
    return () => mq.removeEventListener('change', handler);
}

function resolveTheme(theme) {
    if (theme === 'auto') {
        return window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark';
    }
    return theme === 'light' ? 'light' : 'dark';
}

function setNativeTheme(resolved) {
    try {
        if (resolved === 'light') {
            window.runtime?.WindowSetLightTheme?.();
        } else {
            window.runtime?.WindowSetDarkTheme?.();
        }
    } catch (_) {
        // Wails runtime not ready yet — harmless
    }
}
