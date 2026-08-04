package stats

import (
	"database/sql"
	"fmt"
	"sort"
	"time"

	"key-stats/internal/models"
)

const (
	dayMillis  = int64(24 * 60 * 60 * 1000)
	maxTopKeys = 10
)

// GetDateRangeSummary returns aggregate stats for a date range (inclusive).
// startDaysAgo is the older date, endDaysAgo is the newer date.
// Example: GetDateRangeSummary(db, 7, 0) returns stats for the last 7 days including today.
func GetDateRangeSummary(db *sql.DB, startDaysAgo, endDaysAgo int) (models.TodaySummary, error) {
	var summary models.TodaySummary
	summary.TopKeys = []models.KeyCount{}
	summary.AppBreakdown = []models.AppCount{}

	// Range in epoch-milliseconds for UTC calendar days. Computing concrete
	// boundaries keeps the timestamp column SARGable (the idx_key_events_timestamp
	// index is used) instead of wrapping it in date() which forces a full scan.
	now := time.Now().UnixMilli()
	base := (now / dayMillis) * dayMillis // UTC midnight today
	lo := base - int64(startDaysAgo)*dayMillis
	hi := base - int64(endDaysAgo)*dayMillis + dayMillis

	// Single scan over the range: GROUP BY key_code covers every event, so the
	// total is the sum of the grouped counts. Distinct VK codes are bounded
	// (~256), so no LIMIT is needed before merging display names.
	rows, err := db.Query(`
		SELECT key_code, COUNT(*) as count
		FROM key_events
		WHERE timestamp >= ? AND timestamp < ?
		GROUP BY key_code
	`, lo, hi)
	if err != nil {
		return summary, err
	}
	defer rows.Close()

	nameCount := make(map[string]int)
	for rows.Next() {
		var kc models.KeyCount
		if err := rows.Scan(&kc.KeyCode, &kc.Count); err != nil {
			continue
		}
		summary.TotalKeys += kc.Count
		name := VKToName(kc.KeyCode)
		nameCount[name] += kc.Count
	}

	keys := make([]models.KeyCount, 0, len(nameCount))
	for name, count := range nameCount {
		keys = append(keys, models.KeyCount{KeyName: name, Count: count})
	}
	sort.Slice(keys, func(i, j int) bool {
		if keys[i].Count == keys[j].Count {
			return keys[i].KeyName < keys[j].KeyName
		}
		return keys[i].Count > keys[j].Count
	})
	for i := 0; i < len(keys) && i < maxTopKeys; i++ {
		summary.TopKeys = append(summary.TopKeys, keys[i])
	}

	// App Breakdown
	appRows, err := db.Query(`
		SELECT app_name, COUNT(*) as count
		FROM key_events
		WHERE timestamp >= ? AND timestamp < ?
		GROUP BY app_name
		ORDER BY count DESC
		LIMIT 10`, lo, hi)
	if err != nil {
		return summary, err
	}
	defer appRows.Close()
	for appRows.Next() {
		var ac models.AppCount
		if err := appRows.Scan(&ac.AppName, &ac.Count); err != nil {
			continue
		}
		summary.AppBreakdown = append(summary.AppBreakdown, ac)
	}

	return summary, nil
}

func VKToName(vk int) string {
	// A-Z
	if vk >= 65 && vk <= 90 {
		return string(rune(vk))
	}
	// 0-9 (main keyboard)
	if vk >= 48 && vk <= 57 {
		return string(rune(vk))
	}
	// Numpad 0-9 — map to main keyboard digits for unified heatmap
	if vk >= 96 && vk <= 105 {
		return string(rune('0' + (vk - 96)))
	}
	// F1-F24
	if vk >= 112 && vk <= 135 {
		return fmt.Sprintf("F%d", vk-111)
	}
	// Common keys
	switch vk {
	case 32:
		return "Space"
	case 13:
		return "Enter"
	case 8:
		return "Back"
	case 9:
		return "Tab"
	case 27:
		return "Esc"
	case 20:
		return "Caps"
	case 91:
		return "LWin"
	case 92:
		return "RWin"
	case 93:
		return "Menu"
	case 95:
		return "Sleep"
	// Modifiers — left/right variants
	case 160:
		return "LShift"
	case 161:
		return "RShift"
	case 162:
		return "LCtrl"
	case 163:
		return "RCtrl"
	case 164:
		return "LAlt"
	case 165:
		return "RAlt"
	case 16:
		return "Shift"
	case 17:
		return "Ctrl"
	case 18:
		return "Alt"
	// Navigation
	case 33:
		return "PgUp"
	case 34:
		return "PgDn"
	case 35:
		return "End"
	case 36:
		return "Home"
	case 37:
		return "Left"
	case 38:
		return "Up"
	case 39:
		return "Right"
	case 40:
		return "Down"
	case 45:
		return "Insert"
	case 46:
		return "Delete"
	case 44:
		return "PrtSc"
	case 19:
		return "Pause"
	case 145:
		return "ScrLk"
	case 144:
		return "NumLk"
	// Numpad operators
	case 106:
		return "*"
	case 107:
		return "+"
	case 109:
		return "-"
	case 110:
		return "."
	case 111:
		return "/"
	case 12:
		return "Clear"
	// Main keyboard symbols
	case 187:
		return "="
	case 189:
		return "-"
	case 190:
		return "."
	case 188:
		return ","
	case 191:
		return "/"
	case 186:
		return ";"
	case 192:
		return "`"
	case 219:
		return "["
	case 220:
		return "\\"
	case 221:
		return "]"
	case 222:
		return "'"
	case 223:
		return "~"
	// Media / Browser
	case 166:
		return "BrwBack"
	case 167:
		return "BrwFwd"
	case 168:
		return "BrwRef"
	case 169:
		return "BrwStop"
	case 170:
		return "BrwSrch"
	case 171:
		return "BrwFav"
	case 172:
		return "BrwHome"
	case 173:
		return "Mute"
	case 174:
		return "VolDn"
	case 175:
		return "VolUp"
	case 176:
		return "Next"
	case 177:
		return "Prev"
	case 178:
		return "Stop"
	case 179:
		return "Play"
	case 180:
		return "Mail"
	case 181:
		return "Media"
	case 182:
		return "Launch1"
	case 183:
		return "Launch2"
	// Fn / Special — some keyboard firmwares emit these for the Fn key
	case 255:
		return "Fn"
	case 232:
		return "Fn"
	case 230:
		return "Fn"
	}
	return fmt.Sprintf("VK_%d", vk)
}
