/**
 * Theme manager — applies CSS data-theme attribute, the app font, and the Wails
 * native window theme. The "auto" matchMedia listener is torn down and re-bound
 * whenever the theme changes so a stale listener can never flip the UI back
 * after the user picks a fixed theme.
 */

export const DEFAULT_FONT = 'JetBrains Mono';

// The one active "auto" listener; null when not in auto mode.
let autoCleanup = null;

export function applyTheme(theme) {
    const resolved = resolveTheme(theme);
    document.documentElement.setAttribute('data-theme', resolved);
    setNativeTheme(resolved);
}

export function applyFont(fontFamily) {
    const font = fontFamily || DEFAULT_FONT;
    document.documentElement.style.setProperty('--app-font', `"${font}", "${DEFAULT_FONT}", monospace`);
}

/**
 * Single entry point for both components: applies theme + font and re-binds the
 * auto-theme listener (if any) so switching auto<->fixed always stays current.
 */
export function applyAppearance({ theme, fontFamily } = {}) {
    if (autoCleanup) {
        autoCleanup();
        autoCleanup = null;
    }
    if (fontFamily) applyFont(fontFamily);
    if (theme) {
        applyTheme(theme);
        if (theme === 'auto') {
            autoCleanup = setupAutoTheme();
        }
    }
}

/** Tears down the auto-theme listener (call on component destroy). */
export function cleanupAppearance() {
    if (autoCleanup) {
        autoCleanup();
        autoCleanup = null;
    }
}

function setupAutoTheme() {
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
