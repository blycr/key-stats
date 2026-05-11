//go:build windows

package main

import (
	"syscall"
)

var (
	user32DLL          = syscall.NewLazyDLL("user32.dll")
	procReleaseCapture = user32DLL.NewProc("ReleaseCapture")
	procSendMessageW   = user32DLL.NewProc("SendMessageW")
	procGetForegroundWindow = user32DLL.NewProc("GetForegroundWindow")
)

const (
	wmNCLButtonDown = 0x00A1
	htCaption       = 2
)

// StartDrag triggers native Windows window drag for frameless mode.
// It is bound to the frontend and called on mousedown over the title bar area.
func (a *App) StartDrag() {
	hwnd, _, _ := procGetForegroundWindow.Call()
	if hwnd == 0 {
		return
	}
	procReleaseCapture.Call()
	procSendMessageW.Call(hwnd, uintptr(wmNCLButtonDown), uintptr(htCaption), 0)
}
