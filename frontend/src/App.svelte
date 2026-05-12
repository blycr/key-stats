<script>
    import { onMount } from 'svelte';
    import KeyboardMap from './components/KeyboardMap.svelte';
    import Modal from './components/Modal.svelte';
    import SettingsPanel from './components/SettingsPanel.svelte';
    import { WindowHide, Quit } from '../wailsjs/runtime/runtime.js';

    // Default empty data structure
    let statsData = {
        totalKeys: 0,
        topKeys: [],
        appBreakdown: []
    };
    
    let isLive = true;
    let showMenu = false;
    let menuPos = { x: 0, y: 0 };
    let menuMode = 'dropdown'; // 'dropdown' | 'context'

    // Modal state
    let modalShow = false;
    let modalTitle = '';
    let modalMessage = '';
    let modalMode = 'info';
    let modalConfirmText = 'OK';
    let modalCancelText = 'Cancel';
    let modalOnConfirm = () => {};

    let showSettingsPanel = false;
    let showDateDropdown = false;
    let dateRange = 'Today';
    let startDays = 0;
    let endDays = 0;

    function openModal({ title, message, mode = 'info', confirmText = 'OK', cancelText = 'Cancel', onConfirm = () => {} }) {
        modalTitle = title;
        modalMessage = message;
        modalMode = mode;
        modalConfirmText = confirmText;
        modalCancelText = cancelText;
        modalOnConfirm = onConfirm;
        modalShow = true;
    }

    // Debounce helper
    function debounce(fn, ms) {
        let timer;
        return (...args) => {
            clearTimeout(timer);
            timer = setTimeout(() => fn(...args), ms);
        };
    }

    async function fetchLiveStats() {
        if (!window.go?.app?.App?.GetTodayStats) {
            console.warn("Go backend not available — stats will be empty");
            statsData = { totalKeys: 0, topKeys: [], appBreakdown: [] };
            return;
        }
        try {
            let data;
            if (startDays === 0 && endDays === 0) {
                data = await window.go.app.App.GetTodayStats();
            } else {
                data = await window.go.app.App.GetDateRangeStats(startDays, endDays);
            }
            if (data && data.status !== 'not implemented') {
                 statsData = data;
            }
        } catch (e) {
            console.error("Failed to fetch stats:", e);
        }
    }

    const dateOptions = [
        { label: 'Today', startDays: 0, endDays: 0 },
        { label: 'Yesterday', startDays: 1, endDays: 1 },
        { label: 'Last 7 Days', startDays: 7, endDays: 0 },
        { label: 'Last 30 Days', startDays: 30, endDays: 0 },
    ];

    function selectDateRange(option) {
        dateRange = option.label;
        startDays = option.startDays;
        endDays = option.endDays;
        showDateDropdown = false;
        fetchLiveStats();
    }

    function resetStats() {
        closeMenu();
        openModal({
            title: 'Reset Records',
            message: 'Are you sure you want to reset all records?\nThis action cannot be undone.',
            mode: 'confirm',
            confirmText: 'Reset',
            cancelText: 'Cancel',
            onConfirm: async () => {
                if (window.go?.app?.App?.ResetStats) {
                    try {
                        await window.go.app.App.ResetStats();
                        await fetchLiveStats();
                    } catch (e) {
                        console.error("Failed to reset stats:", e);
                        openModal({
                            title: 'Error',
                            message: 'Failed to reset records: ' + (e.message || 'Unknown error'),
                            mode: 'info'
                        });
                    }
                }
            }
        });
    }

    function showStatus() {
        closeMenu();
        const total = statsData.totalKeys.toLocaleString();
        openModal({
            title: 'Status',
            message: `Today's Keystrokes: ${total}\nRecording: ${isLive ? 'Active' : 'Paused'}`,
            mode: 'info',
            confirmText: 'OK'
        });
    }

    async function minimizeApp() {
        // Save window size before hiding
        if (window.go?.app?.App?.SaveWindowSize) {
            const width = window.outerWidth;
            const height = window.outerHeight;
            try {
                await window.go.app.App.SaveWindowSize(width, height);
            } catch (e) {
                console.error("Failed to save window size:", e);
            }
        }
        WindowHide();
        closeMenu();
    }

    function quitApp() {
        closeMenu();
        Quit();
    }

    function openSettings() {
        closeMenu();
        showSettingsPanel = true;
    }

    function toggleMenu(mode, e) {
        if (mode === 'dropdown') {
            menuMode = 'dropdown';
            showMenu = !showMenu;
        } else {
            e.preventDefault();
            menuMode = 'context';
            menuPos = { x: e.clientX, y: e.clientY };
            showMenu = true;
        }
    }

    function closeMenu() {
        showMenu = false;
    }

    function handleMainClick(e) {
        // Close menu when clicking outside the menu area
        if (showMenu && !e.target.closest('[data-menu]')) {
            closeMenu();
        }
        if (showDateDropdown && !e.target.closest('.date-dropdown-wrapper')) {
            showDateDropdown = false;
        }
    }

    function handleMainKeydown(e) {
        if (e.key === 'Escape' && showMenu) {
            closeMenu();
        }
    }

    function handleMenuKeydown(e) {
        if (e.key === 'Escape') {
            closeMenu();
        }
    }

    onMount(() => {
        // Load and apply saved font
        if (window.go?.app?.App?.GetConfig) {
            window.go.app.App.GetConfig().then(cfg => {
                if (cfg?.fontFamily) {
                    document.documentElement.style.setProperty('--app-font', `"${cfg.fontFamily}", "JetBrains Mono", monospace`);
                }
            }).catch(() => {});
        }

        fetchLiveStats();
        const interval = setInterval(() => {
            if (isLive) fetchLiveStats();
        }, 500);

        // Debounced resize listener to persist window size
        const saveSize = debounce(async () => {
            if (window.go?.app?.App?.SaveWindowSize) {
                const w = window.outerWidth;
                const h = window.outerHeight;
                if (w > 0 && h > 0) {
                    try {
                        await window.go.app.App.SaveWindowSize(w, h);
                    } catch (e) {
                        console.error("SaveWindowSize failed:", e);
                    }
                }
            }
        }, 500);
        window.addEventListener('resize', saveSize);

        return () => {
            clearInterval(interval);
            window.removeEventListener('resize', saveSize);
        };
    });
</script>

<!-- svelte-ignore a11y-no-noninteractive-element-interactions -->
<main class="w-screen h-screen flex flex-col bg-surface text-text-primary overflow-hidden selection:bg-accent/30 font-mono relative" on:click={handleMainClick} on:keydown={handleMainKeydown} on:contextmenu|preventDefault role="application">
    
    <!-- svelte-ignore a11y-no-noninteractive-element-interactions -->
    <!-- Title bar — on mousedown calls Go StartDrag for frameless window drag -->
    <div class="h-[84px] flex items-center justify-between px-6 bg-surface-raised border-b border-surface-overlay/50 shadow-sm z-50 select-none cursor-grab"
         on:mousedown={(e) => { if (!e.target.closest('button')) window.go?.app?.App?.StartDrag?.(); }}
         on:contextmenu={(e) => toggleMenu('context', e)}
         role="banner"
    >
        <div class="flex items-center gap-3 tracking-wide pointer-events-none">
            <div class="w-8 h-8 rounded-lg bg-accent/20 flex items-center justify-center text-accent shadow-[0_0_10px_rgba(108,99,255,0.2)]">
                <svg class="w-4 h-4" fill="currentColor" viewBox="0 0 24 24"><path d="M4 3h16a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2zM6 7h2v2H6zm4 0h2v2h-2zm4 0h2v2h-2zm4 0h2v2h-2zM6 12h2v2H6zm4 0h2v2h-2zm4 0h2v2h-2zm4 0h2v2h-2zM6 17h12v2H6z"/></svg>
            </div>
            <span class="font-bold text-lg text-text-primary">KeyStats</span>
        </div>
        
        <div class="flex items-center gap-4">
            <div class="relative date-dropdown-wrapper">
                <button class="px-3 py-1.5 text-xs font-medium bg-surface-overlay/60 rounded-md hover:bg-surface-overlay transition-colors border border-surface-overlay flex items-center gap-1"
                    on:click|stopPropagation={() => showDateDropdown = !showDateDropdown}>
                    {dateRange}
                    <svg class="w-3 h-3 text-text-tertiary transition-transform duration-200 {showDateDropdown ? 'rotate-180' : ''}" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" d="M19 9l-7 7-7-7"/>
                    </svg>
                </button>
                {#if showDateDropdown}
                <div class="absolute right-0 top-full mt-1 w-36 bg-surface-raised/95 backdrop-blur-2xl border border-surface-overlay/50 rounded-xl shadow-[0_8px_30px_rgba(0,0,0,0.4)] overflow-hidden z-[300] animate-menu">
                    {#each dateOptions as opt}
                    <button class="w-full px-3 py-2 text-xs text-left transition-colors {dateRange === opt.label ? 'text-text-primary bg-surface-overlay/40' : 'text-text-secondary hover:text-text-primary hover:bg-surface-overlay/30'}"
                        on:click|stopPropagation={() => selectDateRange(opt)}>
                        {opt.label}
                    </button>
                    {/each}
                </div>
                {/if}
            </div>
            <button 
                class="flex items-center gap-2 px-3 py-1.5 text-xs font-medium rounded-md transition-all duration-300 border {isLive ? 'text-success bg-success/10 border-success/20' : 'text-text-secondary bg-surface-overlay/30 border-surface-overlay'}"
                on:click={() => isLive = !isLive}
            >
                <div class="w-2 h-2 rounded-full {isLive ? 'bg-success animate-pulse shadow-[0_0_8px_rgba(48,209,88,0.6)]' : 'bg-text-secondary'}"></div>
                {isLive ? 'Live' : 'Paused'}
            </button>
            
            <!-- Menu button -->
            <button 
                class="w-8 h-8 flex items-center justify-center rounded-lg text-text-tertiary hover:text-text-primary hover:bg-surface-overlay/50 transition-all duration-200"
                on:click|stopPropagation={(e) => toggleMenu('dropdown', e)}
                aria-label="Menu"
            >
                <svg class="w-5 h-5" fill="currentColor" viewBox="0 0 24 24"><path d="M12 8c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2zm0 2c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm0 6c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2z"/></svg>
            </button>
        </div>
    </div>

    <!-- Dropdown / context menu -->
    {#if showMenu}
    <!-- svelte-ignore a11y-no-static-element-interactions -->
    <div class="fixed z-[100] w-48 py-1.5 bg-surface-raised/95 backdrop-blur-2xl border border-surface-overlay/40 rounded-xl shadow-[0_8px_30px_rgba(0,0,0,0.4)] overflow-hidden origin-top-right animate-menu"
         style={menuMode === 'context' ? `left: ${menuPos.x}px; top: ${menuPos.y}px;` : 'right: 24px; top: 72px;'}
         data-menu
         role="menu"
         tabindex="-1"
         on:click|stopPropagation
         on:keydown={handleMenuKeydown}
    >
        <button class="w-full px-4 py-2.5 text-xs text-text-secondary hover:text-text-primary hover:bg-surface-overlay/60 transition-colors flex items-center gap-3" on:click={resetStats}>
            <svg class="w-3.5 h-3.5 opacity-70" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24"><path d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"/></svg>
            Reset Records
        </button>

        <div class="h-px bg-surface-overlay/40 mx-2 my-1"></div>

        <button class="w-full px-4 py-2.5 text-xs text-text-secondary hover:text-text-primary hover:bg-surface-overlay/60 transition-colors flex items-center gap-3" on:click={showStatus}>
            <svg class="w-3.5 h-3.5 opacity-70" fill="currentColor" viewBox="0 0 24 24"><path d="M11 7h2v2h-2zm0 4h2v6h-2zm1-9C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2z"/></svg>
            Status
        </button>
        <button class="w-full px-4 py-2.5 text-xs text-text-secondary hover:text-text-primary hover:bg-surface-overlay/60 transition-colors flex items-center gap-3" on:click={openSettings}>
            <svg class="w-3.5 h-3.5 opacity-70" fill="currentColor" viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58a.49.49 0 00.12-.61l-1.92-3.32a.488.488 0 00-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 00-.48-.41h-3.84a.484.484 0 00-.48.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96a.488.488 0 00-.59.22L2.74 8.87a.49.49 0 00.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58a.49.49 0 00-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.27.41.48.41h3.84c.24 0 .44-.17.48-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
            Settings
        </button>

        <div class="h-px bg-surface-overlay/40 mx-2 my-1"></div>

        <button class="w-full px-4 py-2.5 text-xs text-text-secondary hover:text-text-primary hover:bg-surface-overlay/60 transition-colors flex items-center gap-3" on:click={minimizeApp}>
            <svg class="w-3.5 h-3.5 opacity-70" fill="currentColor" viewBox="0 0 24 24"><path d="M19 13H5v-2h14v2z"/></svg>
            Minimize to Tray
        </button>
        <button class="w-full px-4 py-2.5 text-xs text-danger/80 hover:text-danger hover:bg-danger/10 transition-colors flex items-center gap-3" on:click={quitApp}>
            <svg class="w-3.5 h-3.5 opacity-70" fill="currentColor" viewBox="0 0 24 24"><path d="M10.09 15.59L11.5 17l5-5-5-5-1.41 1.41L12.67 11H3v2h9.67l-2.58 2.59zM19 3H5c-1.11 0-2 .9-2 2v4h2V5h14v14H5v-4H3v4c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2z"/></svg>
            Quit
        </button>
    </div>
    {/if}

    <!-- Main content area -->
    <div class="flex-1 flex p-6 gap-6 overflow-hidden relative">
        <!-- Sidebar: today's summary & ranking -->
        <div class="w-[320px] flex flex-col gap-6 z-10">
            <!-- Total count card -->
            <div class="bg-surface-raised/80 backdrop-blur-lg rounded-2xl p-5 border border-surface-overlay/50 shadow-card flex flex-col gap-1 transition-transform hover:-translate-y-0.5 duration-300">
                <h2 class="text-[10px] font-bold text-text-tertiary tracking-widest uppercase mb-1">Today's Keystrokes</h2>
                <div class="text-4xl font-mono text-white font-light flex items-baseline gap-2">
                    {statsData.totalKeys.toLocaleString()}
                    <span class="text-xs font-mono text-accent font-medium tracking-wide">KEYS</span>
                </div>
            </div>

            <!-- App Breakdown card -->
            <div class="bg-surface-raised/80 backdrop-blur-lg rounded-2xl p-5 border border-surface-overlay/50 shadow-card max-h-[200px] flex flex-col">
                <h2 class="text-[10px] font-bold text-text-tertiary tracking-widest uppercase mb-4">Top Apps</h2>
                <div class="flex flex-col gap-3 overflow-y-auto pr-3 custom-scrollbar h-full">
                    {#each statsData.appBreakdown as app, i}
                        <div class="flex items-center gap-3 group relative">
                            <span class="text-text-tertiary font-mono text-[10px] w-3 text-right">{i + 1}</span>
                            <span class="font-mono text-text-primary text-[11px] flex-1 truncate" title={app.appName}>
                                {app.appName}
                            </span>
                            <span class="text-text-secondary font-mono text-xs w-10 text-right">{app.count}</span>
                        </div>
                    {/each}
                    {#if statsData.appBreakdown.length === 0}
                        <div class="text-text-tertiary text-xs flex items-center justify-center h-full opacity-50">
                            No data yet
                        </div>
                    {/if}
                </div>
            </div>

            <!-- Ranking card -->
            <div class="bg-surface-raised/80 backdrop-blur-lg rounded-2xl p-5 border border-surface-overlay/50 shadow-card flex-1 overflow-hidden flex flex-col">
                <h2 class="text-[10px] font-bold text-text-tertiary tracking-widest uppercase mb-4">Top Keys</h2>
                
                <div class="flex flex-col gap-4 overflow-y-auto pr-3 custom-scrollbar h-full">
                    {#each statsData.topKeys as key, i (key.keyName)}
                        <div class="flex items-center gap-3 group relative">
                            <!-- Rank number -->
                            <span class="text-text-tertiary font-mono text-[10px] w-3 text-right">{i + 1}</span>
                            
                            <!-- Key name -->
                            <span class="font-mono bg-surface-overlay/50 border border-surface-overlay text-text-primary px-2 py-0.5 rounded text-[11px] w-14 text-center shadow-sm">
                                {key.keyName}
                            </span>
                            
                            <!-- Progress bar -->
                            <div class="flex-1 h-1.5 bg-surface-overlay/30 rounded-full overflow-hidden relative">
                                <div class="absolute left-0 top-0 h-full bg-accent transition-all duration-1000 ease-out shadow-[0_0_10px_rgba(108,99,255,0.5)]" 
                                     style="width: {(key.count / Math.max(...statsData.topKeys.map(k => k.count), 1)) * 100}%">
                                </div>
                            </div>
                            
                            <!-- Count -->
                            <span class="text-text-secondary font-mono text-xs w-10 text-right">{key.count}</span>
                        </div>
                    {/each}
                    
                    {#if statsData.topKeys.length === 0}
                        <div class="text-text-tertiary text-xs flex items-center justify-center h-full opacity-50">
                            Awaiting keystrokes...
                        </div>
                    {/if}
                </div>
            </div>
        </div>

        <!-- Right side: keyboard heatmap -->
        <div class="flex-1 flex flex-col gap-6 z-10 relative">
            <div class="flex-1 bg-surface-raised/80 backdrop-blur-lg rounded-2xl p-6 border border-surface-overlay/50 shadow-card flex flex-col">
                <h2 class="text-[10px] font-bold text-text-tertiary tracking-widest uppercase mb-6 flex items-center justify-between">
                    <span>Keyboard Heatmap</span>
                    <span class="text-[10px] font-normal lowercase text-text-tertiary bg-surface-overlay/30 px-2 py-0.5 rounded">Intensity by relative frequency</span>
                </h2>
                
                <div class="flex-1 flex items-center justify-center">
                    <KeyboardMap data={statsData.topKeys} />
                </div>
            </div>
        </div>
        
    <!-- Background decorative glow -->
    <div class="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[800px] h-[800px] bg-accent/5 rounded-full blur-[120px] pointer-events-none z-0"></div>
</main>

<!-- Global modal -->
<Modal
    bind:show={modalShow}
    title={modalTitle}
    message={modalMessage}
    mode={modalMode}
    confirmText={modalConfirmText}
    cancelText={modalCancelText}
    on:confirm={modalOnConfirm}
    on:cancel={() => modalShow = false}
/>

<!-- Settings panel -->
<SettingsPanel bind:show={showSettingsPanel} />

<style>
    /* Hide scrollbar globally, keep scroll functionality */
    :global(::-webkit-scrollbar) { width: 0px; background: transparent; }
    
    /* Menu pop-in animation */
    @keyframes menuIn {
        from { opacity: 0; transform: scale(0.95); }
        to   { opacity: 1; transform: scale(1); }
    }
    .animate-menu {
        animation: menuIn 0.15s cubic-bezier(0.16, 1, 0.3, 1) forwards;
    }
</style>
