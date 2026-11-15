#ifndef KS_UI_VIEW_KEYBOARD_H
#define KS_UI_VIEW_KEYBOARD_H

#include "core/stats.h"
#include "ui/d2d_renderer.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "ui/i18n.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *label;
    const char *primary_name;
    const char *alias_name;
    float units;
} KsKeyCapDef;

typedef enum {
    KS_HEAT_NONE = 0,
    KS_HEAT_LOW = 1,
    KS_HEAT_MID = 2,
    KS_HEAT_HIGH = 3
} KsHeatTier;

int ks_keyboard_row_count(void);
int ks_keyboard_row_len(int row);
const KsKeyCapDef *ks_keyboard_row_keys(int row);

int64_t ks_keyboard_lookup_count(const KsTodaySummary *summary, const KsKeyCapDef *def);
bool ks_keyboard_is_key_active(const KsKeyCapDef *def, char (*active_names)[KS_KEY_NAME_CAP], int active_count);
KsHeatTier ks_keyboard_heat_tier(int64_t count, int64_t max_count);

/* Computes card bounds & draws the full 6-row responsive keyboard heatmap.
 * Returns hovered key index or -1 if none. */
int ks_view_keyboard_draw(KsD2DRenderer *r, const KsThemePalette *pal, KsLang lang,
                          KsRect card_rect, const KsTodaySummary *summary,
                          char (*active_names)[KS_KEY_NAME_CAP], int active_count,
                          float mouse_x, float mouse_y, bool allow_hover,
                          char *out_hover_tooltip, int tooltip_cap);

#endif
