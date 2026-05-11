<script>
    import { onMount } from 'svelte';
    import KeyboardMap from './components/KeyboardMap.svelte';

    // 默认空数据结构
    let statsData = {
        totalKeys: 0,
        topKeys: [],
        appBreakdown: []
    };
    
    let isLive = true;

    // 假设 Wails 已经绑定了后端的方法 window.go.main.App.GetTodayStats
    async function fetchLiveStats() {
        if (!window.go?.main?.App?.GetTodayStats) {
            // Mock 数据，方便你在没有后端启动时在浏览器预览丝滑动画
            statsData = {
                totalKeys: 12847,
                topKeys: [
                    { keyName: 'Space', count: 2103 },
                    { keyName: 'E', count: 1024 },
                    { keyName: 'A', count: 891 },
                    { keyName: 'Backspace', count: 756 },
                    { keyName: 'Enter', count: 654 }
                ],
                appBreakdown: []
            };
            return;
        }

        try {
            const data = await window.go.main.App.GetTodayStats();
            // 防止未实现的 stub 覆盖界面
            if (data && data.status !== 'not implemented') {
                 statsData = data;
            }
        } catch (e) {
            console.error("Failed to fetch stats from Wails backend:", e);
        }
    }

    onMount(() => {
        fetchLiveStats();
        // 配合后端 500ms 批量写入，前端 500ms 轮询以降低延迟
        const interval = setInterval(() => {
            if (isLive) fetchLiveStats();
        }, 500);

        return () => clearInterval(interval);
    });
</script>

<main class="w-screen h-screen flex flex-col bg-surface text-text-primary overflow-hidden selection:bg-accent/30 font-sans">
    
    <!-- 顶部状态栏，使用拖拽区域属性以支持 Wails 无边框拖动 -->
    <div class="h-14 flex items-center justify-between px-6 bg-surface-raised border-b border-surface-overlay/50 shadow-sm z-50 select-none" style="-webkit-app-region: drag">
        <div class="flex items-center gap-3 tracking-wide" style="-webkit-app-region: no-drag">
            <div class="w-8 h-8 rounded-lg bg-accent/20 flex items-center justify-center text-accent shadow-[0_0_10px_rgba(108,99,255,0.2)]">
                <svg class="w-4 h-4" fill="currentColor" viewBox="0 0 24 24"><path d="M4 3h16a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2zM6 7h2v2H6zm4 0h2v2h-2zm4 0h2v2h-2zm4 0h2v2h-2zM6 12h2v2H6zm4 0h2v2h-2zm4 0h2v2h-2zm4 0h2v2h-2zM6 17h12v2H6z"/></svg>
            </div>
            <span class="font-bold text-lg text-text-primary">KeyStats</span>
        </div>
        
        <div class="flex items-center gap-4" style="-webkit-app-region: no-drag">
            <button class="px-3 py-1.5 text-xs font-medium bg-surface-overlay/60 rounded-md hover:bg-surface-overlay transition-colors border border-surface-overlay">
                Today ▾
            </button>
            <button 
                class="flex items-center gap-2 px-3 py-1.5 text-xs font-medium rounded-md transition-all duration-300 border {isLive ? 'text-success bg-success/10 border-success/20' : 'text-text-secondary bg-surface-overlay/30 border-surface-overlay'}"
                on:click={() => isLive = !isLive}
            >
                <div class="w-2 h-2 rounded-full {isLive ? 'bg-success animate-pulse shadow-[0_0_8px_rgba(48,209,88,0.6)]' : 'bg-text-secondary'}"></div>
                {isLive ? 'Live' : 'Paused'}
            </button>
        </div>
    </div>

    <!-- 主展示区 -->
    <div class="flex-1 flex p-6 gap-6 overflow-hidden relative">
        <!-- 侧边栏：今日汇总 & 排行榜 -->
        <div class="w-[320px] flex flex-col gap-6 z-10">
            <!-- 总数卡片 -->
            <div class="bg-surface-raised/80 backdrop-blur-lg rounded-2xl p-5 border border-surface-overlay/50 shadow-card flex flex-col gap-1 transition-transform hover:-translate-y-0.5 duration-300">
                <h2 class="text-[10px] font-bold text-text-tertiary tracking-widest uppercase mb-1">Today's Keystrokes</h2>
                <div class="text-4xl font-mono text-white font-light flex items-baseline gap-2">
                    {statsData.totalKeys.toLocaleString()}
                    <span class="text-xs font-sans text-accent font-medium tracking-wide">KEYS</span>
                </div>
            </div>

            <!-- 排行榜卡片 -->
            <div class="bg-surface-raised/80 backdrop-blur-lg rounded-2xl p-5 border border-surface-overlay/50 shadow-card flex-1 overflow-hidden flex flex-col">
                <h2 class="text-[10px] font-bold text-text-tertiary tracking-widest uppercase mb-4">Top Keys</h2>
                
                <div class="flex flex-col gap-4 overflow-y-auto pr-3 custom-scrollbar h-full">
                    {#each statsData.topKeys as key, i (key.keyName)}
                        <div class="flex items-center gap-3 group relative">
                            <!-- 序号 -->
                            <span class="text-text-tertiary font-mono text-[10px] w-3 text-right">{i + 1}</span>
                            
                            <!-- 按键名 -->
                            <span class="font-mono bg-surface-overlay/50 border border-surface-overlay text-text-primary px-2 py-0.5 rounded text-[11px] w-14 text-center shadow-sm">
                                {key.keyName}
                            </span>
                            
                            <!-- 进度条 -->
                            <div class="flex-1 h-1.5 bg-surface-overlay/30 rounded-full overflow-hidden relative">
                                <div class="absolute left-0 top-0 h-full bg-accent transition-all duration-1000 ease-out shadow-[0_0_10px_rgba(108,99,255,0.5)]" 
                                     style="width: {(key.count / Math.max(...statsData.topKeys.map(k => k.count), 1)) * 100}%">
                                </div>
                            </div>
                            
                            <!-- 数量 -->
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

        <!-- 右侧：键盘热力图 -->
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
        
        <!-- 背景装饰光晕 -->
        <div class="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[800px] h-[800px] bg-accent/5 rounded-full blur-[120px] pointer-events-none z-0"></div>
    </div>
</main>

<style>
    /* 全局滚动条美化 */
    :global(::-webkit-scrollbar) { width: 6px; }
    :global(::-webkit-scrollbar-track) { background: transparent; }
    :global(::-webkit-scrollbar-thumb) { background: #3A3A3C; border-radius: 6px; }
    :global(::-webkit-scrollbar-thumb:hover) { background: #4A4A4C; }
</style>
