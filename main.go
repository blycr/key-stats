package main

import (
	"embed"
	"log"

	"key-stats/pkg/app"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
	"github.com/wailsapp/wails/v2/pkg/options/windows"
)

//go:embed all:frontend/dist
var assets embed.FS

//go:embed build/appicon.png
var appIcon []byte

func main() {
	application := app.NewApp(appIcon)

	err := wails.Run(&options.App{
		Title:     "KeyStats",
		Width:     1280,
		Height:    800,
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
