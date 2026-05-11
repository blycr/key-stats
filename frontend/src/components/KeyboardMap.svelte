<script>
    // 接收从外层传入的热力图数据 [{ keyName: "A", count: 120 }, ...]
    export let data = [];

    // 标准 QWERTY 键盘布局
    const keyboardLayout = [
        ['Esc', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 'Backspace'],
        ['Tab', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\'],
        ['Caps', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", 'Enter'],
        ['Shift', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 'Shift'],
        ['Ctrl', 'Win', 'Alt', 'Space', 'Alt', 'Fn', 'Ctrl']
    ];

    // 计算最高频次，用于热力图缩放比例
    $: maxCount = Math.max(...data.map(d => d.count), 1);

    function getCount(keyLabel) {
        if (!data || data.length === 0) return 0;
        const d = data.find(item => item.keyName && item.keyName.toUpperCase() === keyLabel.toUpperCase());
        return d ? d.count : 0;
    }

    // 动态返回背景色，根据占比呈现毛玻璃发光效果
    function getHeatColor(count) {
        if (count === 0) return 'bg-surface-overlay text-text-secondary';
        const ratio = count / maxCount;
        
        if (ratio < 0.2) return 'bg-heatmap-low text-text-primary border-transparent';
        if (ratio < 0.6) return 'bg-heatmap-mid text-white border-accent/30';
        // 最高频的按键，加上发光阴影
        return 'bg-heatmap-high text-white shadow-[0_0_20px_rgba(108,99,255,0.6)] border-accent font-bold scale-105 z-10';
    }

    // 设置特俗按键的宽度
    function getKeyWidthClass(keyLabel) {
        switch (keyLabel) {
            case 'Space': return 'w-72';
            case 'Backspace':
            case 'Enter':
            case 'Shift': return 'px-6';
            case 'Tab':
            case 'Caps': return 'px-5';
            case 'Ctrl':
            case 'Alt':
            case 'Win': return 'px-4';
            default: return 'w-10';
        }
    }
</script>

<div class="flex flex-col gap-2 p-6 bg-surface-overlay/20 rounded-2xl backdrop-blur-xl border border-surface-overlay/50 shadow-card">
    {#each keyboardLayout as row}
        <div class="flex justify-center gap-2">
            {#each row as key}
                {@const count = getCount(key)}
                <div class="
                    relative group flex items-center justify-center rounded-lg border border-surface-overlay/40
                    transition-all duration-500 ease-out cursor-default
                    {getKeyWidthClass(key)} h-10
                    {getHeatColor(count)}
                    hover:-translate-y-1 hover:shadow-[0_4px_20px_rgba(108,99,255,0.4)] hover:border-accent
                ">
                    <span class="text-[11px] font-mono select-none">{key}</span>
                    
                    <!-- 悬浮时的数字提示卡片 (Tooltip) -->
                    {#if count > 0}
                    <div class="absolute -top-10 left-1/2 -translate-x-1/2 px-2.5 py-1 bg-[#000000cc] backdrop-blur-sm text-white text-[10px] rounded-md opacity-0 group-hover:opacity-100 transition-opacity duration-200 whitespace-nowrap pointer-events-none shadow-lg border border-surface-overlay">
                        <span class="text-accent font-bold">{count}</span> presses
                        <!-- 小三角形 -->
                        <div class="absolute -bottom-1 left-1/2 -translate-x-1/2 w-0 h-0 border-l-[4px] border-r-[4px] border-t-[4px] border-l-transparent border-r-transparent border-t-[#000000cc]"></div>
                    </div>
                    {/if}
                </div>
            {/each}
        </div>
    {/each}
</div>
