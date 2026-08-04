//go:build windows

package app

import (
	"key-stats/internal/win32"
)

const (
	wmNCLButtonDown = 0x00A1
	htCaption       = 2
)

// StartDrag triggers native Windows window drag for frameless mode.
// It is bound to the frontend and called on mousedown over the title bar area.
func (a *App) StartDrag() {
	hwnd := win32.FindMainWindow()
	if hwnd == 0 {
		return
	}
	win32.ReleaseCapture.Call()
	win32.SendMessageW.Call(hwnd, uintptr(wmNCLButtonDown), uintptr(htCaption), 0)
}
