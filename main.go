package main

import (
	"embed"
	"log"

	"key-stats/internal/config"
	"key-stats/pkg/app"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
	"github.com/wailsapp/wails/v2/pkg/options/windows"
)

//go:embed all:frontend/dist
var assets embed.FS

//go:embed build/windows/icon.ico
var appIcon []byte

func main() {
	application := app.NewApp(appIcon)

	// Restore saved window size (or use defaults)
	cfg, _ := config.Load()
	width, height := cfg.WindowWidth, cfg.WindowHeight
	if width == 0 {
		width = 1280
	}
	if height == 0 {
		height = 800
	}

	err := wails.Run(&options.App{
		Title:     "KeyStats",
		Width:     width,
		Height:    height,
		MinWidth:  1100,
		MinHeight: 650,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		BackgroundColour: &options.RGBA{R: 28, G: 28, B: 30, A: 1},
		OnStartup:        application.Startup,
		OnShutdown:       application.Shutdown,
		Frameless:        true,
		Windows: &windows.Options{
			WebviewIsTransparent:              true,
			WindowIsTranslucent:               true,
			BackdropType:                      windows.Mica,
			DisableWindowIcon:                 false,
			DisableFramelessWindowDecorations: false,
		},
		Bind: []interface{}{
			application,
		},
	})

	if err != nil {
		log.Fatal(err)
	}
}
