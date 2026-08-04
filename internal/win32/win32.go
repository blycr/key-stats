// Package win32 centralizes the Win32 interop primitives shared across
// keyboard hooks, tray, and window manipulation so procs and struct layouts
// live in exactly one place.
package win32

import (
	"syscall"
	"unsafe"
)

// Shared lazy DLL handles and procs. Tray-only procs (CreateWindowExW, etc.)
// stay declared in pkg/tray to keep this package small.
var (
	user32   = syscall.NewLazyDLL("user32.dll")
	kernel32 = syscall.NewLazyDLL("kernel32.dll")

	FindWindowW         = user32.NewProc("FindWindowW")
	SendMessageW        = user32.NewProc("SendMessageW")
	ReleaseCapture      = user32.NewProc("ReleaseCapture")
	LoadImageW          = user32.NewProc("LoadImageW")
	GetMessageW         = user32.NewProc("GetMessageW")
	GetForegroundWindow = user32.NewProc("GetForegroundWindow")
	GetWindowTextW      = user32.NewProc("GetWindowTextW")

	// Keyboard hook
	SetWindowsHookExW   = user32.NewProc("SetWindowsHookExW")
	UnhookWindowsHookEx = user32.NewProc("UnhookWindowsHookEx")
	CallNextHookEx      = user32.NewProc("CallNextHookEx")

	GetModuleHandleW = kernel32.NewProc("GetModuleHandleW")
)

// WailsWindowClass is the window class name Wails creates for the main window.
const WailsWindowClass = "wailsWindow"

// FindMainWindow returns the handle of the Wails main window, or 0 if not found.
func FindMainWindow() uintptr {
	className, _ := syscall.UTF16PtrFromString(WailsWindowClass)
	hwnd, _, _ := FindWindowW.Call(uintptr(unsafe.Pointer(className)), 0)
	return hwnd
}

// MSG is the Windows MSG structure, kept in sync with the SDK layout.
type MSG struct {
	Hwnd    uintptr
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Pt      struct{ X, Y int32 }
}
