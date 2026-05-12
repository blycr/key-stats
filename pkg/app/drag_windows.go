//go:build windows

package app

import (
	"syscall"
	"unsafe"
)

var (
	user32DLL          = syscall.NewLazyDLL("user32.dll")
	procReleaseCapture = user32DLL.NewProc("ReleaseCapture")
	procSendMessageW   = user32DLL.NewProc("SendMessageW")
	procFindWindowW    = user32DLL.NewProc("FindWindowW")
)

const (
	wmNCLButtonDown = 0x00A1
	htCaption       = 2
)

// StartDrag triggers native Windows window drag for frameless mode.
// It is bound to the frontend and called on mousedown over the title bar area.
func (a *App) StartDrag() {
	className, _ := syscall.UTF16PtrFromString("wailsWindow")
	hwnd, _, _ := procFindWindowW.Call(
		uintptr(unsafe.Pointer(className)),
		0,
	)
	if hwnd == 0 {
		return
	}
	procReleaseCapture.Call()
	procSendMessageW.Call(hwnd, uintptr(wmNCLButtonDown), uintptr(htCaption), 0)
}
