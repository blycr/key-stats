#include "tests/test_runner.h"
#include "ui/i18n.h"
#include "ui/theme.h"
#include "ui/view_keyboard.h"
#include "ui/view_settings.h"
#include "ui/widgets.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool test_keyboard_layout_and_heat(void) {
    KS_ASSERT_EQ_INT(ks_keyboard_row_count(), 6);

    int total_keys = 0;
    for (int r = 0; r < 6; ++r) {
        int len = ks_keyboard_row_len(r);
        KS_ASSERT_TRUE(len > 0);
        const KsKeyCapDef *keys = ks_keyboard_row_keys(r);
        KS_ASSERT_TRUE(keys != NULL);
        float sum_u = 0.0f;
        for (int i = 0; i < len; ++i) {
            KS_ASSERT_TRUE(keys[i].label != NULL && keys[i].label[0] != '\0');
            KS_ASSERT_TRUE(keys[i].primary_name != NULL && keys[i].primary_name[0] != '\0');
            KS_ASSERT_TRUE(keys[i].units > 0.5f);
            sum_u += keys[i].units;
        }
        KS_ASSERT_TRUE(sum_u >= 13.0f && sum_u <= 16.0f);
        total_keys += len;
    }
    KS_ASSERT_TRUE(total_keys >= 70);

    KS_ASSERT_EQ_INT(ks_keyboard_heat_tier(0, 100), KS_HEAT_NONE);
    KS_ASSERT_EQ_INT(ks_keyboard_heat_tier(10, 100), KS_HEAT_LOW);
    KS_ASSERT_EQ_INT(ks_keyboard_heat_tier(20, 100), KS_HEAT_MID);
    KS_ASSERT_EQ_INT(ks_keyboard_heat_tier(59, 100), KS_HEAT_MID);
    KS_ASSERT_EQ_INT(ks_keyboard_heat_tier(60, 100), KS_HEAT_HIGH);

    KsKeyCount items[3] = {
        { "LShift", 42 },
        { "Shift", 8 },
        { "E", 100 }
    };
    KsTodaySummary sum;
    memset(&sum, 0, sizeof(sum));
    memcpy(sum.all_keys, items, sizeof(items));
    sum.all_keys_len = 3;

    const KsKeyCapDef *row4 = ks_keyboard_row_keys(4);
    KS_ASSERT_EQ_STR(row4[0].primary_name, "LShift");
    KS_ASSERT_EQ_INT(ks_keyboard_lookup_count(&sum, &row4[0]), 50);
    KS_ASSERT_EQ_INT(ks_keyboard_lookup_count(&sum, &row4[11]), 0); /* RShift stays distinct */

    char active[2][KS_KEY_NAME_CAP];
    snprintf(active[0], KS_KEY_NAME_CAP, "LShift");
    KS_ASSERT_TRUE(ks_keyboard_is_key_active(&row4[0], active, 1));
    KS_ASSERT_TRUE(!ks_keyboard_is_key_active(&row4[11], active, 1));
    return true;
}

static bool test_virtual_list_and_font_filter(void) {
    int first = 0, last = 0;
    ks_settings_visible_range(500, 0.0f, 200.0f, 34.0f, &first, &last);
    KS_ASSERT_EQ_INT(first, 0);
    KS_ASSERT_TRUE(last >= 5 && last <= 10);

    ks_settings_visible_range(500, 3400.0f, 200.0f, 34.0f, &first, &last);
    KS_ASSERT_EQ_INT(first, 100);
    KS_ASSERT_TRUE(last >= 105 && last <= 110);

    ks_settings_visible_range(500, 17000.0f, 200.0f, 34.0f, &first, &last);
    KS_ASSERT_EQ_INT(last, 499);

    ks_settings_visible_range(0, 0.0f, 200.0f, 34.0f, &first, &last);
    KS_ASSERT_TRUE(last < first);

    char *sample_names[] = { "Segoe UI", "Consolas", "Microsoft YaHei UI", "JetBrains Mono" };
    KsFontList list = { sample_names, 4, 4 };
    KsSettingsModal modal;
    ks_settings_init(&modal);

    ks_settings_filter_fonts(&modal, &list, "");
    KS_ASSERT_EQ_INT(modal.filtered_count, 5); /* System Default + 4 fonts */
    KS_ASSERT_EQ_INT(modal.filtered_idx[0], -1);

    ks_settings_filter_fonts(&modal, &list, "mono");
    KS_ASSERT_EQ_INT(modal.filtered_count, 1);
    KS_ASSERT_EQ_INT(modal.filtered_idx[0], 3);

    ks_settings_filter_fonts(&modal, &list, "no_such_font_xyz");
    KS_ASSERT_EQ_INT(modal.filtered_count, 0);

    ks_settings_destroy(&modal);
    return true;
}

static bool test_theme_transition_and_i18n(void) {
    KsThemeAnimator anim;
    ks_theme_init(&anim, true, KS_SCHEME_INDIGO, false);
    KS_ASSERT_TRUE(anim.current.dark_mode == true);
    KS_ASSERT_TRUE(!anim.animating);

    ks_theme_set_target(&anim, false, KS_SCHEME_OCEAN, false, 1000, false);
    KS_ASSERT_TRUE(anim.animating);
    KS_ASSERT_TRUE(ks_theme_tick(&anim, 1175));
    KS_ASSERT_TRUE(anim.current.surface.r > 0.2f && anim.current.surface.r < 0.92f);

    KS_ASSERT_TRUE(!ks_theme_tick(&anim, 1400));
    KS_ASSERT_TRUE(!anim.animating);
    KS_ASSERT_TRUE(anim.current.dark_mode == false);
    KsThemePalette ocean_light = ks_theme_palette(false, KS_SCHEME_OCEAN, false);
    KS_ASSERT_TRUE(fabsf(anim.current.accent.r - ocean_light.accent.r) < 0.01f);
    KS_ASSERT_TRUE(fabsf(anim.current.text_primary.r - (24.0f / 255.0f)) < 0.01f);

    KS_ASSERT_EQ_INT(ks_scheme_from_code("emerald"), KS_SCHEME_EMERALD);
    KS_ASSERT_EQ_INT(ks_scheme_from_code("unknown"), KS_SCHEME_INDIGO);
    KS_ASSERT_EQ_STR(ks_scheme_to_code(KS_SCHEME_ROSE), "rose");
    KsColor sw = ks_scheme_swatch(KS_SCHEME_AMBER, true);
    KS_ASSERT_TRUE(sw.r > 0.8f);

    KS_ASSERT_EQ_INT(ks_lang_from_code("en"), KS_LANG_EN);
    KS_ASSERT_EQ_INT(ks_lang_from_code("zh-CN"), KS_LANG_ZH);
    KsLang auto_lang = ks_lang_from_code("auto");
    KS_ASSERT_TRUE(auto_lang == KS_LANG_EN || auto_lang == KS_LANG_ZH);

    for (int id = 0; id < KS_STR_COUNT; ++id) {
        const char *en = ks_tr(KS_LANG_EN, (KsStringId)id);
        const char *zh = ks_tr(KS_LANG_ZH, (KsStringId)id);
        KS_ASSERT_TRUE(en != NULL && en[0] != '\0');
        KS_ASSERT_TRUE(zh != NULL && zh[0] != '\0');
    }

    char num[32];
    ks_format_count(1234567, num, sizeof(num));
    KS_ASSERT_EQ_STR(num, "1,234,567");
    return true;
}

int main(void) {
    KS_RUN_TEST(test_keyboard_layout_and_heat);
    KS_RUN_TEST(test_virtual_list_and_font_filter);
    KS_RUN_TEST(test_theme_transition_and_i18n);
    printf("\nUI tests: %d run, %d failed\n", g_ks_tests_run, g_ks_tests_failed);
    return g_ks_tests_failed == 0 ? 0 : 1;
}
