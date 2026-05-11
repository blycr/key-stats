<script>
    export let show = false;
    export let title = '';
    export let message = '';
    export let mode = 'info'; // 'info' | 'confirm'
    export let confirmText = 'OK';
    export let cancelText = 'Cancel';

    import { createEventDispatcher } from 'svelte';
    const dispatch = createEventDispatcher();

    function onConfirm() {
        dispatch('confirm');
        show = false;
    }

    function onCancel() {
        dispatch('cancel');
        show = false;
    }

    function onBackdropClick() {
        if (mode === 'info') {
            onCancel();
        }
    }

    function onKeydown(e) {
        if (!show) return;
        if (e.key === 'Escape') {
            onCancel();
        } else if (e.key === 'Enter' && mode === 'confirm') {
            onConfirm();
        }
    }
</script>

<svelte:window on:keydown={onKeydown}/>

{#if show}
<!-- svelte-ignore a11y-no-static-element-interactions -->
<div class="fixed inset-0 z-[200] flex items-center justify-center" on:click|self={onBackdropClick} on:keydown={onKeydown} role="presentation">
    <!-- Backdrop overlay -->
    <div class="absolute inset-0 bg-black/40 backdrop-blur-sm animate-fade-in"></div>
    
    <!-- Modal card -->
    <div class="relative w-[360px] bg-surface-raised/95 backdrop-blur-2xl border border-surface-overlay/50 rounded-2xl shadow-[0_20px_60px_rgba(0,0,0,0.5)] overflow-hidden animate-modal-in">
        <!-- Top accent line -->
        <div class="h-0.5 w-full bg-gradient-to-r from-transparent via-accent/50 to-transparent"></div>
        
        <div class="p-6 flex flex-col items-center text-center">
            <!-- Icon -->
            {#if mode === 'confirm'}
                <div class="w-12 h-12 rounded-full bg-danger/10 flex items-center justify-center mb-4">
                    <svg class="w-6 h-6 text-danger" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v3.75m-9.303 3.376c-.866 1.5.217 3.374 1.948 3.374h14.71c1.73 0 2.813-1.874 1.948-3.374L13.949 3.378c-.866-1.5-3.032-1.5-3.898 0L2.697 16.126zM12 15.75h.007v.008H12v-.008z"/>
                    </svg>
                </div>
            {:else}
                <div class="w-12 h-12 rounded-full bg-accent/10 flex items-center justify-center mb-4">
                    <svg class="w-6 h-6 text-accent" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" d="M11.25 11.25l.041-.02a.75.75 0 011.063.852l-.708 2.836a.75.75 0 001.063.853l.041-.021M21 12a9 9 0 11-18 0 9 9 0 0118 0zm-9-3.75h.008v.008H12V8.25z"/>
                    </svg>
                </div>
            {/if}
            
            <!-- Title -->
            <h3 class="text-sm font-semibold text-text-primary mb-2">{title}</h3>
            
            <!-- Content -->
            <p class="text-xs text-text-secondary leading-relaxed whitespace-pre-line">{message}</p>
        </div>
        
        <!-- Button row -->
        <div class="px-6 pb-6 flex gap-3 justify-center">
            {#if mode === 'confirm'}
                <button 
                    class="flex-1 px-4 py-2 text-xs font-medium text-text-secondary bg-surface-overlay/50 hover:bg-surface-overlay rounded-lg transition-colors border border-surface-overlay"
                    on:click={onCancel}
                >
                    {cancelText}
                </button>
            {/if}
            <button 
                class="flex-1 px-4 py-2 text-xs font-medium text-white rounded-lg transition-colors shadow-lg {mode === 'confirm' ? 'bg-danger hover:bg-danger/80 shadow-danger/20' : 'bg-accent hover:bg-accent/80 shadow-accent/20'}"
                on:click={onConfirm}
            >
                {confirmText}
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
    .animate-fade-in {
        animation: fadeIn 0.2s ease-out forwards;
    }
    .animate-modal-in {
        animation: modalIn 0.25s cubic-bezier(0.16, 1, 0.3, 1) forwards;
    }
</style>
