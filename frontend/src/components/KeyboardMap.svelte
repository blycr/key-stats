<script>
    export let data = [];
    export let flashKey = { name: '', ts: 0 };

    const keyboardLayout = [
        ['Esc', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 'Backspace'],
        ['Tab', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\'],
        ['Caps', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", 'Enter'],
        ['Shift', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 'Shift'],
        ['Ctrl', 'Win', 'Alt', 'Space', 'Alt', 'Fn', 'Ctrl']
    ];

    $: maxCount = Math.max(...data.map(d => d.count), 1);

    // Build a Map for O(1) key lookups instead of O(N) linear search per key cap.
    $: countMap = (() => {
        const m = new Map();
        for (const d of data) {
            if (d.keyName) {
                m.set(d.keyName.toUpperCase(), d.count);
            }
        }
        return m;
    })();

    function getCount(keyLabel) {
        return countMap.get(keyLabel.toUpperCase()) || 0;
    }

    function getHeatColor(count) {
        if (count === 0) return 'bg-surface-overlay text-text-secondary';
        const ratio = count / maxCount;
        if (ratio < 0.2) return 'bg-heatmap-low text-text-primary border-transparent';
        if (ratio < 0.6) return 'bg-heatmap-mid text-white border-accent/30';
        return 'bg-heatmap-high text-white shadow-[0_0_20px_rgba(108,99,255,0.6)] border-accent font-bold scale-105 z-10';
    }

    function getKeyWidthClass(keyLabel) {
        switch (keyLabel) {
            case 'Space': return 'w-56';
            case 'Backspace':
            case 'Enter':
            case 'Shift': return 'px-5';
            case 'Tab':
            case 'Caps': return 'px-4';
            case 'Ctrl':
            case 'Alt':
            case 'Win': return 'px-3';
            default: return 'w-9';
        }
    }

    let wrapperEl;
    let scale = 1;
    const BASE_WIDTH = 720; // approx natural width of the keyboard layout

    function updateScale() {
        if (!wrapperEl) return;
        const parentWidth = wrapperEl.parentElement.clientWidth;
        scale = Math.min(1, parentWidth / BASE_WIDTH);
    }

    // Map backend key names to keyboard layout labels
    const keyNameToLabel = {
        'Back': 'Backspace',
        'Caps': 'Caps',
        'Esc': 'Esc',
        'Enter': 'Enter',
        'Tab': 'Tab',
        'Space': 'Space',
        'Shift': 'Shift',
        'Ctrl': 'Ctrl',
        'Alt': 'Alt',
        'Win': 'Win',
        'Fn': 'Fn',
    };

    function normalizeKeyName(name) {
        return keyNameToLabel[name] || name;
    }

    let currentFlash = '';
    let flashTimer;

    $: if (flashKey.ts && flashKey.name) {
        currentFlash = normalizeKeyName(flashKey.name);
        clearTimeout(flashTimer);
        flashTimer = setTimeout(() => { currentFlash = ''; }, 180);
    }

    import { onMount } from 'svelte';
    onMount(() => {
        updateScale();
        const ro = new ResizeObserver(updateScale);
        if (wrapperEl && wrapperEl.parentElement) {
            ro.observe(wrapperEl.parentElement);
        }
        return () => {
            ro.disconnect();
            clearTimeout(flashTimer);
        };
    });
</script>

<div bind:this={wrapperEl} class="flex items-center justify-center w-full h-full">
    <div style="transform: scale({scale}); transform-origin: center center;" class="will-change-transform">
        <div class="flex flex-col gap-1.5 p-5 bg-surface-overlay/20 rounded-2xl backdrop-blur-xl border border-surface-overlay/50 shadow-card">
            {#each keyboardLayout as row}
                <div class="flex justify-center gap-1.5">
                    {#each row as key}
                        {@const count = getCount(key)}
                        <div class="
                            relative group flex items-center justify-center rounded-lg border border-surface-overlay/40
                            transition-all duration-500 ease-out cursor-default shrink-0
                            {getKeyWidthClass(key)} h-9
                            {getHeatColor(count)}
                            {currentFlash && currentFlash.toUpperCase() === key.toUpperCase() ? 'key-flash' : ''}
                            hover:-translate-y-1 hover:shadow-[0_4px_20px_rgba(108,99,255,0.4)] hover:border-accent
                        ">
                            <span class="text-[11px] font-mono select-none">{key}</span>
                            
                            {#if count > 0}
                            <div class="absolute -top-10 left-1/2 -translate-x-1/2 px-2.5 py-1 bg-[#000000cc] backdrop-blur-sm text-white text-[10px] rounded-md opacity-0 group-hover:opacity-100 transition-opacity duration-200 whitespace-nowrap pointer-events-none shadow-lg border border-surface-overlay">
                                <span class="text-accent font-bold">{count}</span> presses
                                <div class="absolute -bottom-1 left-1/2 -translate-x-1/2 w-0 h-0 border-l-[4px] border-r-[4px] border-t-[4px] border-l-transparent border-r-transparent border-t-[#000000cc]"></div>
                            </div>
                            {/if}
                        </div>
                    {/each}
                </div>
            {/each}
        </div>
    </div>
</div>

<style>
    /* Real-time key press flash — short bright pulse on the key cap */
    :global(.key-flash) {
        box-shadow: 0 0 20px rgba(108, 99, 255, 0.7), 0 0 8px rgba(108, 99, 255, 0.5) !important;
        border-color: rgba(108, 99, 255, 0.8) !important;
        transform: scale(1.12) !important;
        z-index: 20 !important;
        transition: all 0.04s ease-out !important;
    }
</style>
