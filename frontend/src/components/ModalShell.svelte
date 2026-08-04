<script>
    export let onBackdrop = () => {};
    export let onEscape = () => {};
    export let width = '360px';

    // Escape is owned here; Enter-to-confirm (Modal) stays in the calling component.
    function onKeydown(e) {
        if (e.key === 'Escape') onEscape();
    }
</script>

<svelte:window on:keydown={onKeydown}/>

<!-- svelte-ignore a11y-no-static-element-interactions -->
<div class="fixed inset-0 z-[200] flex items-center justify-center" on:click|self={onBackdrop} role="presentation">
    <div class="absolute inset-0 bg-black/40 backdrop-blur-sm animate-fade-in"></div>

    <div class="relative bg-surface-raised/95 backdrop-blur-2xl border border-surface-overlay/50 rounded-2xl shadow-[var(--shadow-modal)] overflow-hidden animate-modal-in"
         style="width: {width}">
        <div class="h-0.5 w-full bg-gradient-to-r from-transparent via-accent/50 to-transparent"></div>
        <slot/>
    </div>
</div>
