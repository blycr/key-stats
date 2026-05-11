package models

type KeyEvent struct {
	ID        int64  `json:"id"`
	KeyCode   int    `json:"keyCode"`
	AppName   string `json:"appName"`
	Timestamp int64  `json:"timestamp"` // Unix ms
}

type DailyStat struct {
	Date       string `json:"date"`
	KeyCode    int    `json:"keyCode"`
	TotalCount int    `json:"totalCount"`
}

type TodaySummary struct {
	TotalKeys    int          `json:"totalKeys"`
	TopKeys      []KeyCount   `json:"topKeys"`
	AppBreakdown []AppCount   `json:"appBreakdown"`
}

type KeyCount struct {
	KeyCode int    `json:"keyCode"`
	KeyName string `json:"keyName"`
	Count   int    `json:"count"`
}

type AppCount struct {
	AppName string `json:"appName"`
	Count   int    `json:"count"`
}
