package app

import (
	"context"
	"fmt"
	"key-stats/internal/config"
	"key-stats/internal/db"
	"key-stats/internal/models"
	"key-stats/internal/service"
	"key-stats/internal/stats"
	"key-stats/pkg/tray"
)

// App struct
type App struct {
	ctx      context.Context
	database *db.DB
	keyboard *service.KeyboardService
	trayMgr  *tray.Tray
}

// NewApp creates a new App application struct
func NewApp(icon []byte) *App {
	return &App{
		trayMgr: tray.New(icon),
	}
}

// Startup is called when the app starts. Opens DB, starts logger, starts tray.
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

	// 3. Start system tray
	a.trayMgr.Run(ctx,
		func() { tray.ShowWindow(ctx) },
		func() { tray.QuitApp(ctx) },
	)
}

// Shutdown is called when the app is closing.
func (a *App) Shutdown(ctx context.Context) {
	fmt.Println("App is shutting down...")

	if a.trayMgr != nil {
		a.trayMgr.Quit()
	}
	if a.keyboard != nil {
		a.keyboard.Stop()
	}
	if a.database != nil {
		a.database.Close()
	}
}

// GetWindowSize returns the saved window dimensions.
func (a *App) GetWindowSize() (config.WindowState, error) {
	cfg, err := config.Load()
	if err != nil {
		return config.WindowState{}, err
	}
	return cfg.Window, nil
}

// SaveWindowSize persists the current window dimensions.
func (a *App) SaveWindowSize(width, height int) error {
	return config.Save(&config.Config{
		Window: config.WindowState{Width: width, Height: height},
	})
}

// -- API Contract --

// GetTodayStats returns aggregate stats for the current day.
func (a *App) GetTodayStats() (models.TodaySummary, error) {
	if a.database == nil {
		return models.TodaySummary{}, fmt.Errorf("database not initialized")
	}
	return stats.GetTodaySummary(a.database.GetConn())
}

// ResetStats clears all recorded keystroke statistics from the database.
func (a *App) ResetStats() error {
	if a.database == nil {
		return fmt.Errorf("database not initialized")
	}
	return a.database.Reset()
}

// ToggleLogger enables or disables the keyboard hook. Returns new state.
func (a *App) ToggleLogger() (bool, error) {
	return true, nil
}

// Ctx returns the Wails context (used by runtime calls from other packages).
func (a *App) Ctx() context.Context {
	return a.ctx
}
