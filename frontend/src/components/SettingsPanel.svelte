<!-- SettingsPanel.svelte — A dedicated settings modal for KeyStats -->
<script>
    export let show = false;

    import { createEventDispatcher } from 'svelte';
    import ModalShell from './ModalShell.svelte';
    import Dropdown from './Dropdown.svelte';
    import { applyAppearance, DEFAULT_FONT } from '../theme.js';
    import { api } from '../api.js';
    const dispatch = createEventDispatcher();

    let settings = {
        theme: 'dark',
        fontFamily: DEFAULT_FONT,
        startMinimized: false,
        autoStart: false,
        portableMode: false,
        dataDir: ''
    };
    let saving = false;
    let showThemeDropdown = false;
    let showFontDropdown = false;
    let systemFonts = [];

    const themeOptions = [
        { value: 'dark', label: 'Dark' },
        { value: 'light', label: 'Light' },
        { value: 'auto', label: 'Auto' }
    ];

    async function loadSettings() {
        try {
            const cfg = await api.getConfig();
            settings = {
                theme: cfg.theme || 'dark',
                fontFamily: cfg.fontFamily || DEFAULT_FONT,
                startMinimized: cfg.startMinimized || false,
                autoStart: cfg.autoStart || false,
                portableMode: cfg.portableMode || false,
                dataDir: cfg.dataDir || ''
            };
        } catch (e) {
            console.error('Failed to load config:', e);
        }
        await loadSystemFonts();
    }

    async function loadSystemFonts() {
        try {
            const fonts = await api.getSystemFonts();
            // GetSystemFonts already returns the preset list first; just dedupe.
            const seen = new Set();
            systemFonts = fonts.filter(f => {
                const lower = f.toLowerCase();
                if (seen.has(lower)) return false;
                seen.add(lower);
                return true;
            }).sort();
        } catch (e) {
            console.error('Failed to load system fonts:', e);
            systemFonts = [];
        }
    }

    async function saveSettings() {
        saving = true;
        try {
            await api.setConfig({
                theme: settings.theme,
                fontFamily: settings.fontFamily,
                startMinimized: settings.startMinimized,
                autoStart: settings.autoStart
            });
            applyAppearance(settings);
            closePanel();
        } catch (e) {
            console.error('Failed to save config:', e);
        } finally {
            saving = false;
        }
    }

    function closePanel() {
        show = false;
        showThemeDropdown = false;
        showFontDropdown = false;
        dispatch('close');
    }

    // Close a dropdown when clicking outside the panel
    function onWindowClick(e) {
        if (showThemeDropdown && !e.target.closest('.theme-dropdown-wrapper')) {
            showThemeDropdown = false;
        }
        if (showFontDropdown && !e.target.closest('.font-dropdown-wrapper')) {
            showFontDropdown = false;
        }
    }

    $: if (show) loadSettings();
</script>

<svelte:window on:click={onWindowClick}/>

{#if show}
<ModalShell onBackdrop={closePanel} onEscape={closePanel} width="420px">
    <div class="p-6">
        <h3 class="text-sm font-semibold text-text-primary mb-1">Settings</h3>
        <p class="text-[11px] text-text-tertiary mb-5">Changes are saved to .env and take effect on next launch.</p>

        <div class="space-y-4">
            <!-- Theme -->
            <div class="flex items-center justify-between">
                <div>
                    <div class="text-xs text-text-primary">Theme</div>
                    <div class="text-[10px] text-text-tertiary">Interface color scheme</div>
                </div>
                <Dropdown
                    open={showThemeDropdown}
                    onToggle={() => { showThemeDropdown = !showThemeDropdown; showFontDropdown = false; }}
                    items={themeOptions}
                    display={themeOptions.find(o => o.value === settings.theme)?.label || settings.theme}
                    itemLabel={(o) => o.label}
                    isActive={(o) => o.value === settings.theme}
                    onSelect={(o) => { settings.theme = o.value; showThemeDropdown = false; }}
                    buttonClass="min-w-[100px]"
                    wrapperClass="theme-dropdown-wrapper"
                />
            </div>

            <!-- Font Family -->
            <div class="flex items-center justify-between">
                <div>
                    <div class="text-xs text-text-primary">Font</div>
                    <div class="text-[10px] text-text-tertiary">Interface & code font</div>
                </div>
                <Dropdown
                    open={showFontDropdown}
                    onToggle={() => { showFontDropdown = !showFontDropdown; showThemeDropdown = false; }}
                    items={systemFonts}
                    display={settings.fontFamily || DEFAULT_FONT}
                    itemLabel={(f) => f}
                    isActive={(f) => settings.fontFamily === f}
                    onSelect={(f) => { settings.fontFamily = f; showFontDropdown = false; }}
                    buttonClass="min-w-[140px]"
                    buttonStyle={`font-family: '${settings.fontFamily || DEFAULT_FONT}', monospace;`}
                    menuClass="w-[200px] max-h-[240px] overflow-y-auto custom-scrollbar"
                    itemStyle={(f) => `font-family: '${f}', monospace;`}
                    wrapperClass="font-dropdown-wrapper"
                />
            </div>

            <!-- Start Minimized -->
            <div class="flex items-center justify-between">
                <div>
                    <div class="text-xs text-text-primary">Start Minimized</div>
                    <div class="text-[10px] text-text-tertiary">Launch directly to system tray</div>
                </div>
                <button class="w-9 h-5 rounded-full transition-colors duration-200 relative {settings.startMinimized ? 'bg-accent' : 'bg-surface-overlay'}" on:click={() => settings.startMinimized = !settings.startMinimized}>
                    <div class="w-3.5 h-3.5 rounded-full bg-white absolute top-0.5 transition-transform duration-200 {settings.startMinimized ? 'translate-x-4' : 'translate-x-0.5'}"></div>
                </button>
            </div>

            <!-- Auto Start -->
            <div class="flex items-center justify-between">
                <div>
                    <div class="text-xs text-text-primary">Launch on Startup</div>
                    <div class="text-[10px] text-text-tertiary">Start KeyStats when Windows boots</div>
                </div>
                <button class="w-9 h-5 rounded-full transition-colors duration-200 relative {settings.autoStart ? 'bg-accent' : 'bg-surface-overlay'}" on:click={() => settings.autoStart = !settings.autoStart}>
                    <div class="w-3.5 h-3.5 rounded-full bg-white absolute top-0.5 transition-transform duration-200 {settings.autoStart ? 'translate-x-4' : 'translate-x-0.5'}"></div>
                </button>
            </div>

            <div class="h-px bg-surface-overlay/40"></div>

            <!-- Data Directory (read-only) -->
            <div>
                <div class="text-xs text-text-primary mb-1">Data Directory</div>
                <div class="text-[10px] text-text-tertiary font-mono bg-surface-overlay/30 border border-surface-overlay/30 rounded-md px-2 py-1.5 break-all">{settings.dataDir || 'Loading...'}</div>
            </div>

            <!-- Portable Mode (read-only) -->
            <div class="flex items-center justify-between">
                <div>
                    <div class="text-xs text-text-primary">Portable Mode</div>
                    <div class="text-[10px] text-text-tertiary">Store data next to the executable</div>
                </div>
                <span class="text-[10px] font-medium px-2 py-0.5 rounded-md {settings.portableMode ? 'bg-accent/10 text-accent' : 'bg-surface-overlay/50 text-text-tertiary'}">
                    {settings.portableMode ? 'Active' : 'Inactive'}
                </span>
            </div>
        </div>
    </div>

    <div class="px-6 pb-6 flex gap-3 justify-end">
        <button class="px-4 py-2 text-xs font-medium text-text-secondary bg-surface-overlay/50 hover:bg-surface-overlay rounded-lg transition-colors border border-surface-overlay" on:click={closePanel}>
            Cancel
        </button>
        <button class="px-4 py-2 text-xs font-medium text-white bg-accent hover:bg-accent/80 rounded-lg transition-colors shadow-lg shadow-accent/20" on:click={saveSettings} disabled={saving}>
            {saving ? 'Saving...' : 'Save'}
        </button>
    </div>
</ModalShell>
{/if}
