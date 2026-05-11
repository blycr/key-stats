<!-- SettingsPanel.svelte — A dedicated settings modal for KeyStats -->
<script>
    export let show = false;

    import { createEventDispatcher } from 'svelte';
    const dispatch = createEventDispatcher();

    let settings = {
        theme: 'dark',
        startMinimized: false,
        autoStart: false,
        portableMode: false,
        dataDir: ''
    };
    let saving = false;
    let showThemeDropdown = false;

    const themeOptions = [
        { value: 'dark', label: 'Dark' },
        { value: 'light', label: 'Light' },
        { value: 'auto', label: 'Auto' }
    ];

    async function loadSettings() {
        if (!window.go?.app?.App?.GetConfig) return;
        try {
            const cfg = await window.go.app.App.GetConfig();
            settings = {
                theme: cfg.theme || 'dark',
                startMinimized: cfg.startMinimized || false,
                autoStart: cfg.autoStart || false,
                portableMode: cfg.portableMode || false,
                dataDir: cfg.dataDir || ''
            };
        } catch (e) {
            console.error('Failed to load config:', e);
        }
    }

    async function saveSettings() {
        if (!window.go?.app?.App?.SetConfig) return;
        saving = true;
        try {
            const changed = await window.go.app.App.SetConfig({
                theme: settings.theme,
                startMinimized: settings.startMinimized,
                autoStart: settings.autoStart
            });
            console.log('Config updated:', changed);
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
        dispatch('close');
    }

    function onKeydown(e) {
        if (show && e.key === 'Escape') closePanel();
    }

    function selectTheme(value) {
        settings.theme = value;
        showThemeDropdown = false;
    }

    // Close dropdown when clicking outside the panel or pressing Escape
    function onWindowClick(e) {
        if (showThemeDropdown) {
            const dropdown = e.target.closest('.theme-dropdown-wrapper');
            if (!dropdown) showThemeDropdown = false;
        }
    }

    $: if (show) loadSettings();
</script>

<svelte:window on:keydown={onKeydown} on:click={onWindowClick}/>

{#if show}
<div class="fixed inset-0 z-[200] flex items-center justify-center" on:click|self={closePanel}>
    <div class="absolute inset-0 bg-black/40 backdrop-blur-sm animate-fade-in"></div>

    <div class="relative w-[420px] bg-surface-raised/95 backdrop-blur-2xl border border-surface-overlay/50 rounded-2xl shadow-[0_20px_60px_rgba(0,0,0,0.5)] overflow-hidden animate-modal-in">
        <div class="h-0.5 w-full bg-gradient-to-r from-transparent via-accent/50 to-transparent"></div>

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
                    <div class="relative theme-dropdown-wrapper">
                        <button
                            class="min-w-[100px] flex items-center justify-between gap-2 bg-surface-overlay/60 border border-surface-overlay text-text-primary text-xs rounded-lg px-3 py-1.5 outline-none focus:border-accent transition-colors cursor-pointer"
                            on:click|stopPropagation={() => showThemeDropdown = !showThemeDropdown}
                        >
                            <span>{themeOptions.find(o => o.value === settings.theme)?.label || settings.theme}</span>
                            <svg class="w-3.5 h-3.5 text-text-tertiary transition-transform duration-200 {showThemeDropdown ? 'rotate-180' : ''}" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                <path stroke-linecap="round" stroke-linejoin="round" d="M19 9l-7 7-7-7"/>
                            </svg>
                        </button>

                        {#if showThemeDropdown}
                        <div class="absolute right-0 top-full mt-1 w-full min-w-[100px] bg-surface-raised/95 backdrop-blur-2xl border border-surface-overlay/50 rounded-xl shadow-[0_8px_30px_rgba(0,0,0,0.4)] overflow-hidden z-[300] animate-dropdown-in">
                            {#each themeOptions as opt}
                            <button
                                class="w-full px-3 py-2 text-xs text-left transition-colors {settings.theme === opt.value ? 'text-text-primary bg-surface-overlay/40' : 'text-text-secondary hover:text-text-primary hover:bg-surface-overlay/30'}"
                                on:click|stopPropagation={() => selectTheme(opt.value)}
                            >
                                {opt.label}
                            </button>
                            {/each}
                        </div>
                        {/if}
                    </div>
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
    </div>
</div>
{/if}

<style>
    @keyframes fadeIn {
        from { opacity: 0; }
        to { opacity: 1; }
    }
    @keyframes modalIn {
        from { opacity: 0; transform: scale(0.92) translateY(8px); }
        to { opacity: 1; transform: scale(1) translateY(0); }
    }
    @keyframes dropdownIn {
        from { opacity: 0; transform: scale(0.95) translateY(-4px); }
        to { opacity: 1; transform: scale(1) translateY(0); }
    }
    .animate-fade-in {
        animation: fadeIn 0.2s ease-out forwards;
    }
    .animate-modal-in {
        animation: modalIn 0.25s cubic-bezier(0.16, 1, 0.3, 1) forwards;
    }
    .animate-dropdown-in {
        animation: dropdownIn 0.15s cubic-bezier(0.16, 1, 0.3, 1) forwards;
    }
</style>
