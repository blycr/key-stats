#include "ui/i18n.h"

#include <windows.h>
#include <string.h>

static const char *const kStringsEn[KS_STR_COUNT] = {
    [KS_STR_APP_TITLE] = "KeyStats",
    [KS_STR_LIVE] = "Live",
    [KS_STR_PAUSED] = "Paused",
    [KS_STR_HERO_TOTAL] = "TOTAL KEYSTROKES",
    [KS_STR_UNIT_KEYS] = "KEYS",
    [KS_STR_TOP_APPS] = "TOP APPS",
    [KS_STR_TOP_KEYS] = "TOP KEYS",
    [KS_STR_KEYBOARD_HEATMAP] = "KEYBOARD HEATMAP",
    [KS_STR_HEAT_SUBTITLE] = "Scale",
    [KS_STR_AWAITING_KEYS] = "Awaiting input",
    [KS_STR_NO_DATA] = "No data",
    [KS_STR_UNKNOWN_APP] = "Unknown Window",
    [KS_STR_MENU_STATUS] = "Status",
    [KS_STR_MENU_SETTINGS] = "Settings",
    [KS_STR_MENU_MINIMIZE_TRAY] = "Minimize to Tray",
    [KS_STR_MENU_QUIT] = "Quit",
    [KS_STR_STATUS_TITLE] = "Status",
    [KS_STR_STATUS_ACTIVE] = "Recording Active",
    [KS_STR_STATUS_PAUSED] = "Recording Paused",
    [KS_STR_STATUS_TOTAL_LABEL] = "Total Keystrokes",
    [KS_STR_STATUS_MODE_LABEL] = "Storage Mode",
    [KS_STR_STATUS_DESC] = "Counts keystrokes continuously in local SQLite via a non-blocking hook without capturing typed content.",
    [KS_STR_RESET_TITLE] = "Confirm Clear Statistics",
    [KS_STR_RESET_CONFIRM_MSG] = "Permanently delete all recorded keystrokes and app history?\nThis action cannot be undone.",
    [KS_STR_BTN_CANCEL] = "Cancel",
    [KS_STR_BTN_CONFIRM_RESET] = "Delete Forever",
    [KS_STR_BTN_OK] = "OK",
    [KS_STR_BTN_SAVE] = "Save",
    [KS_STR_SETTINGS_TITLE] = "Settings",
    [KS_STR_SETTINGS_SUBTITLE] = "Persisted to local config",
    [KS_STR_SETTINGS_LANGUAGE] = "Language",
    [KS_STR_SETTINGS_LANGUAGE_DESC] = "App & tray language",
    [KS_STR_LANG_AUTO] = "Auto",
    [KS_STR_SETTINGS_THEME] = "Theme",
    [KS_STR_SETTINGS_THEME_DESC] = "Appearance mode",
    [KS_STR_THEME_AUTO] = "Auto",
    [KS_STR_THEME_DARK] = "Dark",
    [KS_STR_THEME_LIGHT] = "Light",
    [KS_STR_SETTINGS_SCHEME] = "Color Scheme",
    [KS_STR_SETTINGS_SCHEME_DESC] = "Surface & accent palette",
    [KS_STR_SCHEME_INDIGO] = "Midnight Indigo",
    [KS_STR_SCHEME_OCEAN] = "Ocean Cyan",
    [KS_STR_SCHEME_EMERALD] = "Emerald Mint",
    [KS_STR_SCHEME_AMBER] = "Warm Amber",
    [KS_STR_SCHEME_ROSE] = "Crimson Rose",
    [KS_STR_SETTINGS_FONT] = "Font",
    [KS_STR_SETTINGS_FONT_DESC] = "UI & keycap font",
    [KS_STR_SETTINGS_FONT_SEARCH] = "Search fonts",
    [KS_STR_SETTINGS_FONT_DEFAULT] = "System Default",
    [KS_STR_SETTINGS_FONT_NO_MATCH] = "No match",
    [KS_STR_SETTINGS_START_MINIMIZED] = "Start Minimized",
    [KS_STR_SETTINGS_START_MINIMIZED_DESC] = "Hide to tray on launch",
    [KS_STR_SETTINGS_AUTO_START] = "Auto Start",
    [KS_STR_SETTINGS_AUTO_START_DESC] = "Launch on Windows login",
    [KS_STR_SETTINGS_DATA_DIR] = "Data Directory",
    [KS_STR_SETTINGS_OPEN_FOLDER] = "Open",
    [KS_STR_SETTINGS_PORTABLE_TAG] = "Portable",
    [KS_STR_SETTINGS_CLEAR_STATS] = "Statistics Data",
    [KS_STR_SETTINGS_CLEAR_STATS_DESC] = "Clear all cumulative records",
    [KS_STR_SETTINGS_CLEAR_BTN] = "Clear Stats",
    [KS_STR_TOOLTIP_PRESSES] = "presses",
    [KS_STR_TRAY_SHOW] = "Show Window",
    [KS_STR_TRAY_QUIT] = "Quit"
};

static const char *const kStringsZh[KS_STR_COUNT] = {
    [KS_STR_APP_TITLE] = "KeyStats",
    [KS_STR_LIVE] = "监听中",
    [KS_STR_PAUSED] = "已暂停",
    [KS_STR_HERO_TOTAL] = "累计按键",
    [KS_STR_UNIT_KEYS] = "次",
    [KS_STR_TOP_APPS] = "常用应用",
    [KS_STR_TOP_KEYS] = "高频按键",
    [KS_STR_KEYBOARD_HEATMAP] = "键盘热力图",
    [KS_STR_HEAT_SUBTITLE] = "热力",
    [KS_STR_AWAITING_KEYS] = "等待按键",
    [KS_STR_NO_DATA] = "暂无数据",
    [KS_STR_UNKNOWN_APP] = "未知窗口",
    [KS_STR_MENU_STATUS] = "运行状态",
    [KS_STR_MENU_SETTINGS] = "设置",
    [KS_STR_MENU_MINIMIZE_TRAY] = "最小化到托盘",
    [KS_STR_MENU_QUIT] = "退出程序",
    [KS_STR_STATUS_TITLE] = "运行状态",
    [KS_STR_STATUS_ACTIVE] = "后台全局监听中",
    [KS_STR_STATUS_PAUSED] = "按键记录已暂停",
    [KS_STR_STATUS_TOTAL_LABEL] = "累计按键总数",
    [KS_STR_STATUS_MODE_LABEL] = "配置与存储",
    [KS_STR_STATUS_DESC] = "软件运行期间持续在本地 SQLite 中累计按键次数，不记录任何输入内容。",
    [KS_STR_RESET_TITLE] = "二次确认：清空统计数据",
    [KS_STR_RESET_CONFIRM_MSG] = "此操作将永久删除本地所有累计按键与应用统计记录，清空后无法恢复。\n请确认是否继续？",
    [KS_STR_BTN_CANCEL] = "取消",
    [KS_STR_BTN_CONFIRM_RESET] = "确认永久清空",
    [KS_STR_BTN_OK] = "确定",
    [KS_STR_BTN_SAVE] = "保存",
    [KS_STR_SETTINGS_TITLE] = "设置",
    [KS_STR_SETTINGS_SUBTITLE] = "调整后点击保存即可持久化",
    [KS_STR_SETTINGS_LANGUAGE] = "语言",
    [KS_STR_SETTINGS_LANGUAGE_DESC] = "界面与托盘语言",
    [KS_STR_LANG_AUTO] = "跟随系统",
    [KS_STR_SETTINGS_THEME] = "主题",
    [KS_STR_SETTINGS_THEME_DESC] = "深色、浅色或自动",
    [KS_STR_THEME_AUTO] = "跟随系统",
    [KS_STR_THEME_DARK] = "深色",
    [KS_STR_THEME_LIGHT] = "浅色",
    [KS_STR_SETTINGS_SCHEME] = "配色方案",
    [KS_STR_SETTINGS_SCHEME_DESC] = "主题色调与点缀色",
    [KS_STR_SCHEME_INDIGO] = "星夜靛紫",
    [KS_STR_SCHEME_OCEAN] = "深海青蓝",
    [KS_STR_SCHEME_EMERALD] = "苍翠森绿",
    [KS_STR_SCHEME_AMBER] = "琥珀暖金",
    [KS_STR_SCHEME_ROSE] = "绯樱玫瑰",
    [KS_STR_SETTINGS_FONT] = "字体",
    [KS_STR_SETTINGS_FONT_DESC] = "全局与键帽字体",
    [KS_STR_SETTINGS_FONT_SEARCH] = "搜索字体",
    [KS_STR_SETTINGS_FONT_DEFAULT] = "系统默认",
    [KS_STR_SETTINGS_FONT_NO_MATCH] = "无匹配字体",
    [KS_STR_SETTINGS_START_MINIMIZED] = "启动最小化",
    [KS_STR_SETTINGS_START_MINIMIZED_DESC] = "启动后直接进入托盘",
    [KS_STR_SETTINGS_AUTO_START] = "开机自启",
    [KS_STR_SETTINGS_AUTO_START_DESC] = "随系统登录自动运行",
    [KS_STR_SETTINGS_DATA_DIR] = "数据目录",
    [KS_STR_SETTINGS_OPEN_FOLDER] = "打开",
    [KS_STR_SETTINGS_PORTABLE_TAG] = "便携",
    [KS_STR_SETTINGS_CLEAR_STATS] = "统计数据",
    [KS_STR_SETTINGS_CLEAR_STATS_DESC] = "永久清空累计的按键与统计",
    [KS_STR_SETTINGS_CLEAR_BTN] = "清空统计",
    [KS_STR_TOOLTIP_PRESSES] = "次",
    [KS_STR_TRAY_SHOW] = "显示主界面",
    [KS_STR_TRAY_QUIT] = "退出"
};

static KsLang detect_system_lang(void) {
    LANGID lid = GetUserDefaultUILanguage();
    return (PRIMARYLANGID(lid) == LANG_CHINESE) ? KS_LANG_ZH : KS_LANG_EN;
}

KsLang ks_lang_from_code(const char *lang_code) {
    if (!lang_code || lang_code[0] == '\0' || _stricmp(lang_code, "auto") == 0) {
        return detect_system_lang();
    }
    if (_strnicmp(lang_code, "en", 2) == 0) {
        return KS_LANG_EN;
    }
    if (_strnicmp(lang_code, "zh", 2) == 0) {
        return KS_LANG_ZH;
    }
    return detect_system_lang();
}

const char *ks_lang_to_code(KsLang lang) {
    return (lang == KS_LANG_EN) ? "en" : "zh";
}

const char *ks_tr(KsLang lang, KsStringId id) {
    if ((int)id < 0 || id >= KS_STR_COUNT) return "";
    const char *s = (lang == KS_LANG_ZH) ? kStringsZh[id] : kStringsEn[id];
    return s ? s : "";
}
