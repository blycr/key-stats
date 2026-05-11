<script>
    export let data = [];

    const keyboardLayout = [
        ['Esc', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 'Backspace'],
        ['Tab', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\'],
        ['Caps', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", 'Enter'],
        ['Shift', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 'Shift'],
        ['Ctrl', 'Win', 'Alt', 'Space', 'Alt', 'Fn', 'Ctrl']
    ];

    $: maxCount = Math.max(...data.map(d => d.count), 1);

    function getCount(keyLabel) {
        if (!data || data.length === 0) return 0;
        const d = data.find(item => item.keyName && item.keyName.toUpperCase() === keyLabel.toUpperCase());
        return d ? d.count : 0;
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

    import { onMount } from 'svelte';
    onMount(() => {
        updateScale();
        const ro = new ResizeObserver(updateScale);
        if (wrapperEl && wrapperEl.parentElement) {
            ro.observe(wrapperEl.parentElement);
        }
        return () => ro.disconnect();
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
