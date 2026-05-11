//go:build windows

package tray

import (
	"context"

	"github.com/getlantern/systray"
	"github.com/wailsapp/wails/v2/pkg/runtime"
)

// Tray manages the system tray icon and menu.
type Tray struct {
	icon   []byte
	ctx    context.Context
	onShow func()
	onQuit func()
}

// New creates a new Tray manager with the given icon bytes (PNG).
func New(icon []byte) *Tray {
	return &Tray{icon: icon}
}

// Run starts the system tray in a background goroutine.
// ctx is the Wails runtime context (needed for runtime.WindowShow / runtime.Quit).
func (t *Tray) Run(ctx context.Context, onShow, onQuit func()) {
	t.ctx = ctx
	t.onShow = onShow
	t.onQuit = onQuit

	go func() {
		systray.Run(t.ready, t.exit)
	}()
}

// Quit removes the tray icon.
func (t *Tray) Quit() {
	systray.Quit()
}

func (t *Tray) ready() {
	systray.SetIcon(t.icon)
	systray.SetTitle("KeyStats")
	systray.SetTooltip("KeyStats — Keyboard Statistics")

	mShow := systray.AddMenuItem("显示主页面", "显示 KeyStats 主窗口")
	mQuit := systray.AddMenuItem("退出", "退出 KeyStats")

	go func() {
		for {
			select {
			case <-mShow.ClickedCh:
				if t.onShow != nil {
					t.onShow()
				}
			case <-mQuit.ClickedCh:
				if t.onQuit != nil {
					t.onQuit()
				}
			}
		}
	}()
}

func (t *Tray) exit() {}

// ShowWindow is a helper that shows the Wails window via runtime.
func ShowWindow(ctx context.Context) {
	runtime.WindowShow(ctx)
}

// QuitApp is a helper that gracefully quits the Wails application.
func QuitApp(ctx context.Context) {
	runtime.Quit(ctx)
}
