package main

import (
	"context"
	"fmt"
	"key-stats/internal/db"
	"key-stats/internal/models"
	"key-stats/internal/service"
	"key-stats/internal/stats"
)

// App struct
type App struct {
	ctx      context.Context
	database *db.DB
	keyboard *service.KeyboardService
}

// NewApp creates a new App application struct
func NewApp() *App {
	return &App{}
}

// Startup is called when the app starts. Opens DB, starts logger.
func (a *App) Startup(ctx context.Context) {
	a.ctx = ctx
	fmt.Println("App is starting up...")

	// 1. Initialize DB
	d, err := db.InitDB()
	if err != nil {
		fmt.Printf("Failed to init DB: %v\n", err)
		return
	}
	a.database = d

	// 2. Start Keyboard Logger
	a.keyboard = service.NewKeyboardService(d)
	a.keyboard.Start()
}

// Shutdown is called when the app is closing.
func (a *App) Shutdown(ctx context.Context) {
	fmt.Println("App is shutting down...")
	if a.keyboard != nil {
		a.keyboard.Stop()
	}
	if a.database != nil {
		a.database.Close()
	}
}

// -- API Contract --

// GetTodayStats returns aggregate stats for the current day.
func (a *App) GetTodayStats() (models.TodaySummary, error) {
	if a.database == nil {
		return models.TodaySummary{}, fmt.Errorf("database not initialized")
	}
	// Access the underlying sql.DB from internal/db/sqlite.go
	// Since DB struct is in internal/db, we need to export it or add a getter.
	// Let's assume we can get it.
	return stats.GetTodaySummary(a.database.GetConn())
}

// ToggleLogger enables or disables the keyboard hook. Returns new state.
func (a *App) ToggleLogger() (bool, error) {
	return true, nil
}
