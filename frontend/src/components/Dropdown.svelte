<script>
    export let open = false;
    export let onToggle = () => {};
    export let display = '';
    export let items = [];
    export let itemLabel = (item) => item;
    export let isActive = () => false;
    export let onSelect = () => {};
    export let buttonClass = '';
    export let buttonStyle = '';
    export let menuClass = '';
    export let itemStyle = null; // (item) => style string | null
    export let wrapperClass = '';
</script>

<div class="relative {wrapperClass}">
    <button
        class="flex items-center justify-between gap-2 bg-surface-overlay/60 border border-surface-overlay text-text-primary text-xs rounded-lg px-3 py-1.5 outline-none focus:border-accent transition-colors cursor-pointer {buttonClass}"
        style={buttonStyle || undefined}
        on:click|stopPropagation={onToggle}
    >
        <span>{display}</span>
        <svg class="w-3.5 h-3.5 text-text-tertiary transition-transform duration-200 {open ? 'rotate-180' : ''}" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" d="M19 9l-7 7-7-7"/>
        </svg>
    </button>

    {#if open}
    <div class="absolute right-0 top-full mt-1 bg-surface-raised/95 backdrop-blur-2xl border border-surface-overlay/50 rounded-xl shadow-[var(--shadow-dropdown)] overflow-hidden z-[300] animate-dropdown-in {menuClass}">
        {#each items as item (item)}
        <button
            class="w-full px-3 py-2 text-xs text-left transition-colors {isActive(item) ? 'text-text-primary bg-surface-overlay/40' : 'text-text-secondary hover:text-text-primary hover:bg-surface-overlay/30'}"
            style={itemStyle ? itemStyle(item) : undefined}
            on:click|stopPropagation={() => onSelect(item)}
        >
            {itemLabel(item)}
        </button>
        {/each}
    </div>
    {/if}
</div>
