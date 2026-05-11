package stats

import (
	"database/sql"
	"key-stats/internal/models"
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

	// Top 10 Keys Today
	rows, err := db.Query(`
		SELECT key_code, COUNT(*) as count 
		FROM key_events 
		WHERE date(timestamp/1000, 'unixepoch') = date('now')
		GROUP BY key_code 
		ORDER BY count DESC 
		LIMIT 10
	`)
	if err != nil {
		return summary, err
	}
	defer rows.Close()

	for rows.Next() {
		var kc models.KeyCount
		if err := rows.Scan(&kc.KeyCode, &kc.Count); err != nil {
			continue
		}
		kc.KeyName = VKToName(kc.KeyCode)
		summary.TopKeys = append(summary.TopKeys, kc)
	}

	return summary, nil
}

func VKToName(vk int) string {
	// A-Z
	if vk >= 65 && vk <= 90 {
		return string(rune(vk))
	}
	// 0-9
	if vk >= 48 && vk <= 57 {
		return string(rune(vk))
	}
	// Common keys
	switch vk {
	case 32: return "Space"
	case 13: return "Enter"
	case 8:  return "Back"
	case 16: return "Shift"
	case 17: return "Ctrl"
	case 18: return "Alt"
	case 9:  return "Tab"
	case 27: return "Esc"
	case 20: return "Caps"
	case 91: return "Win"
	}
	return "Key"
}
