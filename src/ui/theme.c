#include "ui/theme.h"

#include <windows.h>
#include <string.h>

typedef struct {
    const char *code;
    uint8_t dark_bg[3];
    uint8_t dark_surface[3];
    uint8_t dark_raised[3];
    uint8_t dark_overlay[3];
    uint8_t dark_accent[3];
    uint8_t dark_accent_hover[3];
    uint8_t light_bg[3];
    uint8_t light_surface[3];
    uint8_t light_raised[3];
    uint8_t light_overlay[3];
    uint8_t light_accent[3];
    uint8_t light_accent_hover[3];
} KsSchemeSpec;

static const KsSchemeSpec kSchemes[KS_SCHEME_COUNT] = {
    [KS_SCHEME_INDIGO] = {
        .code = "indigo",
        .dark_bg = { 18, 19, 24 },
        .dark_surface = { 28, 29, 36 },
        .dark_raised = { 42, 44, 53 },
        .dark_overlay = { 52, 54, 64 },
        .dark_accent = { 108, 99, 255 },
        .dark_accent_hover = { 130, 122, 255 },
        .light_bg = { 241, 240, 236 },
        .light_surface = { 250, 249, 246 },
        .light_raised = { 255, 255, 255 },
        .light_overlay = { 235, 234, 229 },
        .light_accent = { 88, 81, 230 },
        .light_accent_hover = { 108, 100, 255 },
    },
    [KS_SCHEME_OCEAN] = {
        .code = "ocean",
        .dark_bg = { 13, 19, 27 },
        .dark_surface = { 21, 30, 41 },
        .dark_raised = { 33, 45, 60 },
        .dark_overlay = { 44, 58, 76 },
        .dark_accent = { 56, 189, 248 },
        .dark_accent_hover = { 103, 210, 252 },
        .light_bg = { 236, 242, 248 },
        .light_surface = { 246, 250, 254 },
        .light_raised = { 255, 255, 255 },
        .light_overlay = { 226, 235, 244 },
        .light_accent = { 2, 132, 199 },
        .light_accent_hover = { 14, 154, 224 },
    },
    [KS_SCHEME_EMERALD] = {
        .code = "emerald",
        .dark_bg = { 14, 21, 19 },
        .dark_surface = { 22, 33, 30 },
        .dark_raised = { 34, 49, 44 },
        .dark_overlay = { 45, 63, 57 },
        .dark_accent = { 52, 211, 153 },
        .dark_accent_hover = { 90, 225, 175 },
        .light_bg = { 237, 243, 240 },
        .light_surface = { 247, 251, 249 },
        .light_raised = { 255, 255, 255 },
        .light_overlay = { 226, 236, 231 },
        .light_accent = { 5, 150, 105 },
        .light_accent_hover = { 16, 172, 122 },
    },
    [KS_SCHEME_AMBER] = {
        .code = "amber",
        .dark_bg = { 22, 19, 16 },
        .dark_surface = { 33, 29, 24 },
        .dark_raised = { 49, 43, 36 },
        .dark_overlay = { 62, 54, 45 },
        .dark_accent = { 245, 158, 11 },
        .dark_accent_hover = { 251, 178, 48 },
        .light_bg = { 245, 241, 234 },
        .light_surface = { 252, 250, 246 },
        .light_raised = { 255, 255, 255 },
        .light_overlay = { 237, 231, 221 },
        .light_accent = { 217, 119, 6 },
        .light_accent_hover = { 234, 136, 18 },
    },
    [KS_SCHEME_ROSE] = {
        .code = "rose",
        .dark_bg = { 23, 17, 21 },
        .dark_surface = { 35, 26, 32 },
        .dark_raised = { 51, 38, 46 },
        .dark_overlay = { 64, 48, 58 },
        .dark_accent = { 251, 113, 133 },
        .dark_accent_hover = { 253, 142, 159 },
        .light_bg = { 246, 239, 242 },
        .light_surface = { 253, 248, 250 },
        .light_raised = { 255, 255, 255 },
        .light_overlay = { 238, 227, 232 },
        .light_accent = { 225, 29, 72 },
        .light_accent_hover = { 240, 52, 92 },
    },
};

KsColor ks_rgba(uint8_t r, uint8_t g, uint8_t b, float a) {
    KsColor c;
    c.r = (float)r / 255.0f;
    c.g = (float)g / 255.0f;
    c.b = (float)b / 255.0f;
    c.a = a;
    return c;
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float ease_out_cubic(float t) {
    t = clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

KsColor ks_color_lerp(KsColor a, KsColor b, float t) {
    t = clamp01(t);
    KsColor out;
    out.r = a.r + (b.r - a.r) * t;
    out.g = a.g + (b.g - a.g) * t;
    out.b = a.b + (b.b - a.b) * t;
    out.a = a.a + (b.a - a.a) * t;
    return out;
}

KsColorScheme ks_scheme_from_code(const char *code) {
    if (!code || code[0] == '\0') return KS_SCHEME_INDIGO;
    for (int i = 0; i < KS_SCHEME_COUNT; ++i) {
        if (_stricmp(code, kSchemes[i].code) == 0) {
            return (KsColorScheme)i;
        }
    }
    return KS_SCHEME_INDIGO;
}

const char *ks_scheme_to_code(KsColorScheme scheme) {
    int idx = (int)scheme;
    if (idx < 0 || idx >= KS_SCHEME_COUNT) idx = 0;
    return kSchemes[idx].code;
}

KsColor ks_scheme_swatch(KsColorScheme scheme, bool dark_mode) {
    int idx = (int)scheme;
    if (idx < 0 || idx >= KS_SCHEME_COUNT) idx = 0;
    const uint8_t *rgb = dark_mode ? kSchemes[idx].dark_accent : kSchemes[idx].light_accent;
    return ks_rgba(rgb[0], rgb[1], rgb[2], 1.0f);
}

KsThemePalette ks_theme_palette(bool dark_mode, KsColorScheme scheme, bool mica_active) {
    (void)mica_active;
    int idx = (int)scheme;
    if (idx < 0 || idx >= KS_SCHEME_COUNT) idx = 0;
    const KsSchemeSpec *s = &kSchemes[idx];

    KsThemePalette p;
    p.dark_mode = dark_mode;
    if (dark_mode) {
        p.bg_backdrop = ks_rgba(s->dark_bg[0], s->dark_bg[1], s->dark_bg[2], 1.0f);
        p.surface = ks_rgba(s->dark_surface[0], s->dark_surface[1], s->dark_surface[2], 0.96f);
        p.surface_raised = ks_rgba(s->dark_raised[0], s->dark_raised[1], s->dark_raised[2], 0.98f);
        p.surface_overlay = ks_rgba(s->dark_overlay[0], s->dark_overlay[1], s->dark_overlay[2], 0.96f);
        p.chassis_bg = ks_rgba(s->dark_bg[0], s->dark_bg[1], s->dark_bg[2], 0.72f);
        p.text_primary = ks_rgba(244, 244, 246, 1.0f);
        p.text_secondary = ks_rgba(166, 168, 178, 1.0f);
        p.text_tertiary = ks_rgba(118, 120, 130, 1.0f);
        p.accent = ks_rgba(s->dark_accent[0], s->dark_accent[1], s->dark_accent[2], 1.0f);
        p.accent_hover = ks_rgba(s->dark_accent_hover[0], s->dark_accent_hover[1], s->dark_accent_hover[2], 1.0f);
        p.heatmap = p.accent;
        p.success = ks_rgba(52, 211, 153, 1.0f);
        p.danger = ks_rgba(239, 68, 68, 1.0f);
        p.border_subtle = ks_rgba(255, 255, 255, 0.08f);
        p.border_strong = ks_rgba(255, 255, 255, 0.14f);
        p.border_key = ks_rgba(255, 255, 255, 0.09f);
        p.card_highlight = ks_rgba(255, 255, 255, 0.04f);
        p.glass_tint = ks_rgba(s->dark_surface[0], s->dark_surface[1], s->dark_surface[2], 0.88f);
        p.glass_border = ks_rgba(255, 255, 255, 0.0f);
        p.shadow_color = ks_rgba(0, 0, 0, 0.24f);
        p.glow_ambient = ks_color_alpha(p.accent, 0.075f);
    } else {
        p.bg_backdrop = ks_rgba(s->light_bg[0], s->light_bg[1], s->light_bg[2], 1.0f);
        p.surface = ks_rgba(s->light_surface[0], s->light_surface[1], s->light_surface[2], 0.96f);
        p.surface_raised = ks_rgba(s->light_raised[0], s->light_raised[1], s->light_raised[2], 0.98f);
        p.surface_overlay = ks_rgba(s->light_overlay[0], s->light_overlay[1], s->light_overlay[2], 0.96f);
        p.chassis_bg = ks_rgba(s->light_bg[0], s->light_bg[1], s->light_bg[2], 0.76f);
        p.text_primary = ks_rgba(24, 24, 27, 1.0f);
        p.text_secondary = ks_rgba(82, 82, 91, 1.0f);
        p.text_tertiary = ks_rgba(138, 138, 146, 1.0f);
        p.accent = ks_rgba(s->light_accent[0], s->light_accent[1], s->light_accent[2], 1.0f);
        p.accent_hover = ks_rgba(s->light_accent_hover[0], s->light_accent_hover[1], s->light_accent_hover[2], 1.0f);
        p.heatmap = p.accent;
        p.success = ks_rgba(16, 158, 106, 1.0f);
        p.danger = ks_rgba(220, 38, 38, 1.0f);
        p.border_subtle = ks_rgba(0, 0, 0, 0.07f);
        p.border_strong = ks_rgba(0, 0, 0, 0.12f);
        p.border_key = ks_rgba(0, 0, 0, 0.085f);
        p.card_highlight = ks_rgba(255, 255, 255, 0.75f);
        p.glass_tint = ks_rgba(s->light_surface[0], s->light_surface[1], s->light_surface[2], 0.90f);
        p.glass_border = ks_rgba(0, 0, 0, 0.0f);
        p.shadow_color = ks_rgba(24, 24, 27, 0.08f);
        p.glow_ambient = ks_color_alpha(p.accent, 0.045f);
    }
    return p;
}

KsThemePalette ks_theme_dark_palette(bool mica_active) {
    return ks_theme_palette(true, KS_SCHEME_INDIGO, mica_active);
}

KsThemePalette ks_theme_light_palette(bool mica_active) {
    return ks_theme_palette(false, KS_SCHEME_INDIGO, mica_active);
}

bool ks_system_prefers_dark(void) {
    DWORD val = 1;
    DWORD sz = sizeof(val);
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD type = 0;
        if (RegQueryValueExW(key, L"AppsUseLightTheme", NULL, &type, (BYTE *)&val, &sz) != ERROR_SUCCESS ||
            type != REG_DWORD) {
            val = 1;
        }
        RegCloseKey(key);
    }
    return val == 0;
}

bool ks_resolve_dark_mode(const char *theme_pref) {
    if (!theme_pref || _stricmp(theme_pref, "auto") == 0 || theme_pref[0] == '\0') {
        return ks_system_prefers_dark();
    }
    if (_stricmp(theme_pref, "light") == 0) {
        return false;
    }
    return true;
}

void ks_theme_init(KsThemeAnimator *anim, bool dark_mode, KsColorScheme scheme, bool mica_active) {
    memset(anim, 0, sizeof(*anim));
    KsThemePalette p = ks_theme_palette(dark_mode, scheme, mica_active);
    anim->from = p;
    anim->to = p;
    anim->current = p;
    anim->target_dark = dark_mode;
    anim->target_scheme = scheme;
    anim->animating = false;
    anim->anim_duration_ms = 350;
}

void ks_theme_set_target(KsThemeAnimator *anim, bool dark_mode, KsColorScheme scheme,
                         bool mica_active, uint64_t now_ms, bool immediate) {
    KsThemePalette next = ks_theme_palette(dark_mode, scheme, mica_active);
    if (immediate) {
        anim->from = next;
        anim->to = next;
        anim->current = next;
        anim->target_dark = dark_mode;
        anim->target_scheme = scheme;
        anim->animating = false;
        return;
    }
    anim->from = anim->current;
    anim->to = next;
    anim->target_dark = dark_mode;
    anim->target_scheme = scheme;
    anim->anim_start_ms = now_ms;
    anim->anim_duration_ms = 350;
    anim->animating = true;
}

bool ks_theme_tick(KsThemeAnimator *anim, uint64_t now_ms) {
    if (!anim->animating) return false;
    uint64_t elapsed = (now_ms >= anim->anim_start_ms) ? (now_ms - anim->anim_start_ms) : 0;
    float raw = (anim->anim_duration_ms > 0) ? ((float)elapsed / (float)anim->anim_duration_ms) : 1.0f;
    float t = ease_out_cubic(raw);

    anim->current.dark_mode = (t >= 0.5f) ? anim->to.dark_mode : anim->from.dark_mode;
    anim->current.bg_backdrop = ks_color_lerp(anim->from.bg_backdrop, anim->to.bg_backdrop, t);
    anim->current.surface = ks_color_lerp(anim->from.surface, anim->to.surface, t);
    anim->current.surface_raised = ks_color_lerp(anim->from.surface_raised, anim->to.surface_raised, t);
    anim->current.surface_overlay = ks_color_lerp(anim->from.surface_overlay, anim->to.surface_overlay, t);
    anim->current.chassis_bg = ks_color_lerp(anim->from.chassis_bg, anim->to.chassis_bg, t);
    anim->current.text_primary = ks_color_lerp(anim->from.text_primary, anim->to.text_primary, t);
    anim->current.text_secondary = ks_color_lerp(anim->from.text_secondary, anim->to.text_secondary, t);
    anim->current.text_tertiary = ks_color_lerp(anim->from.text_tertiary, anim->to.text_tertiary, t);
    anim->current.accent = ks_color_lerp(anim->from.accent, anim->to.accent, t);
    anim->current.accent_hover = ks_color_lerp(anim->from.accent_hover, anim->to.accent_hover, t);
    anim->current.heatmap = ks_color_lerp(anim->from.heatmap, anim->to.heatmap, t);
    anim->current.success = ks_color_lerp(anim->from.success, anim->to.success, t);
    anim->current.danger = ks_color_lerp(anim->from.danger, anim->to.danger, t);
    anim->current.border_subtle = ks_color_lerp(anim->from.border_subtle, anim->to.border_subtle, t);
    anim->current.border_strong = ks_color_lerp(anim->from.border_strong, anim->to.border_strong, t);
    anim->current.border_key = ks_color_lerp(anim->from.border_key, anim->to.border_key, t);
    anim->current.card_highlight = ks_color_lerp(anim->from.card_highlight, anim->to.card_highlight, t);
    anim->current.glass_tint = ks_color_lerp(anim->from.glass_tint, anim->to.glass_tint, t);
    anim->current.glass_border = ks_color_lerp(anim->from.glass_border, anim->to.glass_border, t);
    anim->current.shadow_color = ks_color_lerp(anim->from.shadow_color, anim->to.shadow_color, t);
    anim->current.glow_ambient = ks_color_lerp(anim->from.glow_ambient, anim->to.glow_ambient, t);

    if (raw >= 1.0f) {
        anim->current = anim->to;
        anim->animating = false;
        return false;
    }
    return true;
}
