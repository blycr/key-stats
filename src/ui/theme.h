#ifndef KS_UI_THEME_H
#define KS_UI_THEME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KS_SCHEME_INDIGO = 0,
    KS_SCHEME_OCEAN = 1,
    KS_SCHEME_EMERALD = 2,
    KS_SCHEME_AMBER = 3,
    KS_SCHEME_ROSE = 4,
    KS_SCHEME_COUNT
} KsColorScheme;

typedef struct {
    float r;
    float g;
    float b;
    float a;
} KsColor;

typedef struct {
    bool dark_mode;
    KsColor bg_backdrop;
    KsColor surface;
    KsColor surface_raised;
    KsColor surface_overlay;
    KsColor chassis_bg;
    KsColor text_primary;
    KsColor text_secondary;
    KsColor text_tertiary;
    KsColor accent;
    KsColor accent_hover;
    KsColor heatmap;
    KsColor success;
    KsColor danger;
    KsColor border_subtle;
    KsColor border_strong;
    KsColor border_key;
    KsColor card_highlight;
    KsColor glass_tint;
    KsColor glass_border;
    KsColor shadow_color;
    KsColor glow_ambient;
} KsThemePalette;

typedef struct {
    KsThemePalette from;
    KsThemePalette to;
    KsThemePalette current;
    bool target_dark;
    KsColorScheme target_scheme;
    bool animating;
    uint64_t anim_start_ms;
    uint32_t anim_duration_ms;
} KsThemeAnimator;

KsColor ks_rgba(uint8_t r, uint8_t g, uint8_t b, float a);
KsColor ks_color_lerp(KsColor a, KsColor b, float t);
static inline KsColor ks_color_alpha(KsColor c, float a) {
    c.a = a;
    return c;
}

KsColorScheme ks_scheme_from_code(const char *code);
const char *ks_scheme_to_code(KsColorScheme scheme);
KsColor ks_scheme_swatch(KsColorScheme scheme, bool dark_mode);

KsThemePalette ks_theme_palette(bool dark_mode, KsColorScheme scheme, bool mica_active);
KsThemePalette ks_theme_dark_palette(bool mica_active);
KsThemePalette ks_theme_light_palette(bool mica_active);
bool ks_system_prefers_dark(void);
bool ks_resolve_dark_mode(const char *theme_pref);

void ks_theme_init(KsThemeAnimator *anim, bool dark_mode, KsColorScheme scheme, bool mica_active);
void ks_theme_set_target(KsThemeAnimator *anim, bool dark_mode, KsColorScheme scheme,
                         bool mica_active, uint64_t now_ms, bool immediate);
bool ks_theme_tick(KsThemeAnimator *anim, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
