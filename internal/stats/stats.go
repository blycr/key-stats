package stats

import (
	"database/sql"
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
	// Common keys
	switch vk {
	case 32:  return "Space"
	case 13:  return "Enter"
	case 8:   return "Back"
	case 16:  return "Shift"
	case 17:  return "Ctrl"
	case 18:  return "Alt"
	case 9:   return "Tab"
	case 27:  return "Esc"
	case 20:  return "Caps"
	case 91:  return "Win"
	case 92:  return "Win"
	case 106: return "*"
	case 107: return "+"
	case 109: return "-"
	case 110: return "."
	case 111: return "/"
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
	}
	return "Key"
}
