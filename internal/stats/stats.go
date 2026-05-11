package stats

import (
	"database/sql"
	"fmt"
	"key-stats/internal/models"
	"sort"
)

func GetTodaySummary(db *sql.DB) (models.TodaySummary, error) {
	var summary models.TodaySummary
	summary.TopKeys = []models.KeyCount{}
	summary.AppBreakdown = []models.AppCount{}

	// Total Keys Today
	err := db.QueryRow("SELECT COUNT(*) FROM key_events WHERE date(timestamp/1000, 'unixepoch') = date('now')").Scan(&summary.TotalKeys)
	if err != nil {
		return summary, err
	}

	// Top 10 Keys Today — aggregate by key_name so numpad and main keys merge
	rows, err := db.Query(`
		SELECT key_code, COUNT(*) as count 
		FROM key_events 
		WHERE date(timestamp/1000, 'unixepoch') = date('now')
		GROUP BY key_code 
		ORDER BY count DESC 
		LIMIT 50
	`)
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
		name := VKToName(kc.KeyCode)
		nameCount[name] += kc.Count
	}

	// Convert map to slice and sort by count (desc), then name (asc) for stable ordering
	type pair struct {
		name  string
		count int
	}
	var pairs []pair
	for n, c := range nameCount {
		pairs = append(pairs, pair{n, c})
	}
	sort.SliceStable(pairs, func(i, j int) bool {
		if pairs[i].count == pairs[j].count {
			return pairs[i].name < pairs[j].name
		}
		return pairs[i].count > pairs[j].count
	})
	for i := 0; i < len(pairs) && i < 10; i++ {
		summary.TopKeys = append(summary.TopKeys, models.KeyCount{
			KeyName: pairs[i].name,
			Count:   pairs[i].count,
		})
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
	case 32:  return "Space"
	case 13:  return "Enter"
	case 8:   return "Back"
	case 9:   return "Tab"
	case 27:  return "Esc"
	case 20:  return "Caps"
	case 91:  return "Win"
	case 92:  return "Win"
	case 93:  return "Menu"
	case 95:  return "Sleep"
	// Modifiers — left/right variants
	case 160: return "LShift"
	case 161: return "RShift"
	case 162: return "LCtrl"
	case 163: return "RCtrl"
	case 164: return "LAlt"
	case 165: return "RAlt"
	case 16:  return "Shift"
	case 17:  return "Ctrl"
	case 18:  return "Alt"
	// Navigation
	case 33:  return "PgUp"
	case 34:  return "PgDn"
	case 35:  return "End"
	case 36:  return "Home"
	case 37:  return "Left"
	case 38:  return "Up"
	case 39:  return "Right"
	case 40:  return "Down"
	case 45:  return "Insert"
	case 46:  return "Delete"
	case 44:  return "PrtSc"
	case 19:  return "Pause"
	case 145: return "ScrLk"
	case 144: return "NumLk"
	// Numpad operators
	case 106: return "*"
	case 107: return "+"
	case 109: return "-"
	case 110: return "."
	case 111: return "/"
	case 12:  return "Clear"
	// Main keyboard symbols
	case 187: return "="
	case 189: return "-"
	case 190: return "."
	case 188: return ","
	case 191: return "/"
	case 186: return ";"
	case 192: return "`"
	case 219: return "["
	case 220: return "\\"
	case 221: return "]"
	case 222: return "'"
	case 223: return "`"
	// Media / Browser
	case 166: return "BrwBack"
	case 167: return "BrwFwd"
	case 168: return "BrwRef"
	case 169: return "BrwStop"
	case 170: return "BrwSrch"
	case 171: return "BrwFav"
	case 172: return "BrwHome"
	case 173: return "Mute"
	case 174: return "VolDn"
	case 175: return "VolUp"
	case 176: return "Next"
	case 177: return "Prev"
	case 178: return "Stop"
	case 179: return "Play"
	case 180: return "Mail"
	case 181: return "Media"
	case 182: return "Launch1"
	case 183: return "Launch2"
	}
	return fmt.Sprintf("VK_%d", vk)
}
