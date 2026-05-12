//go:build windows

package tray

import (
	"context"
	"os"
	"path/filepath"
	"runtime"
	"syscall"
	"unsafe"

	wailsRuntime "github.com/wailsapp/wails/v2/pkg/runtime"
)

var (
	user32                  = syscall.NewLazyDLL("user32.dll")
	procCreateWindowExW     = user32.NewProc("CreateWindowExW")
	procRegisterClassExW    = user32.NewProc("RegisterClassExW")
	procDefWindowProcW      = user32.NewProc("DefWindowProcW")
	procGetMessageW         = user32.NewProc("GetMessageW")
	procTranslateMessage    = user32.NewProc("TranslateMessage")
	procDispatchMessageW    = user32.NewProc("DispatchMessageW")
	procCreatePopupMenu     = user32.NewProc("CreatePopupMenu")
	procAppendMenuW         = user32.NewProc("AppendMenuW")
	procTrackPopupMenu      = user32.NewProc("TrackPopupMenu")
	procDestroyMenu         = user32.NewProc("DestroyMenu")
	procPostQuitMessage     = user32.NewProc("PostQuitMessage")
	procPostMessageW        = user32.NewProc("PostMessageW")
	procSetForegroundWindow = user32.NewProc("SetForegroundWindow")
	procGetCursorPos        = user32.NewProc("GetCursorPos")
	procDestroyWindow       = user32.NewProc("DestroyWindow")
	procUnregisterClassW    = user32.NewProc("UnregisterClassW")
	procDestroyIcon         = user32.NewProc("DestroyIcon")

	kernel32             = syscall.NewLazyDLL("kernel32.dll")
	procGetModuleHandleW = kernel32.NewProc("GetModuleHandleW")

	shell32            = syscall.NewLazyDLL("shell32.dll")
	procShellNotifyIconW = shell32.NewProc("Shell_NotifyIconW")
	procExtractIconExW   = shell32.NewProc("ExtractIconExW")
)

const (
	NIM_ADD         = 0x00000000
	NIM_DELETE      = 0x00000001
	NIF_MESSAGE     = 0x00000001
	NIF_ICON        = 0x00000002
	NIF_TIP         = 0x00000004
	WM_USER         = 0x0400
	TRAY_MSG_ID     = WM_USER + 1
	WM_LBUTTONUP    = 0x0202
	WM_RBUTTONUP    = 0x0205
	WM_COMMAND      = 0x0111
	WM_APP_QUIT     = WM_USER + 2
	WM_DESTROY      = 0x0002
	ID_SHOW         = 1001
	ID_QUIT         = 1002
	MF_STRING       = 0x00000000
	TPM_RIGHTALIGN  = 0x0008
	TPM_BOTTOMALIGN = 0x0020
	CW_USEDEFAULT   = 0x80000000
	WS_OVERLAPPED   = 0x00000000
)

type wndClassEx struct {
	cbSize        uint32
	style         uint32
	lpfnWndProc   uintptr
	cbClsExtra    int32
	cbWndExtra    int32
	hInstance     uintptr
	hIcon         uintptr
	hCursor       uintptr
	hbrBackground uintptr
	lpszMenuName  *uint16
	lpszClassName *uint16
	hIconSm       uintptr
}

type point struct {
	X int32
	Y int32
}

type msg struct {
	Hwnd    uintptr
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	Pt      point
}

type notifyIconData struct {
	cbSize           uint32
	hWnd             uintptr
	uID              uint32
	uFlags           uint32
	uCallbackMessage uint32
	hIcon            uintptr
	szTip            [128]uint16
	dwState          uint32
	dwStateMask      uint32
	szInfo           [256]uint16
	uVersion         uint32
	szInfoTitle      [64]uint16
	dwInfoFlags      uint32
	guidItem         [16]byte
	hBalloonIcon     uintptr
}

// Tray manages the system tray icon and menu.
type Tray struct {
	icon   []byte
	ctx    context.Context
	onShow func()
	onQuit func()

	hwnd  uintptr
	hIcon uintptr
}

// New creates a new Tray manager with the given icon bytes (ICO).
func New(icon []byte) *Tray {
	return &Tray{icon: icon}
}

// Run starts the system tray in a dedicated locked OS thread.
func (t *Tray) Run(ctx context.Context, onShow, onQuit func()) {
	t.ctx = ctx
	t.onShow = onShow
	t.onQuit = onQuit

	go func() {
		runtime.LockOSThread()
		t.runMessageLoop()
	}()
}

// Quit removes the tray icon and stops the message loop.
func (t *Tray) Quit() {
	if t.hwnd != 0 {
		procPostMessageW.Call(t.hwnd, WM_APP_QUIT, 0, 0)
	}
}

func (t *Tray) runMessageLoop() {
	hInstance, _, _ := procGetModuleHandleW.Call(0)

	className, _ := syscall.UTF16PtrFromString("KeyStatsTrayWindow")

	wndProcCallback := syscall.NewCallback(t.wndProc)

	wcex := &wndClassEx{
		cbSize:        uint32(unsafe.Sizeof(wndClassEx{})),
		lpfnWndProc:   wndProcCallback,
		hInstance:     hInstance,
		lpszClassName: className,
	}

	ret, _, _ := procRegisterClassExW.Call(uintptr(unsafe.Pointer(wcex)))
	if ret == 0 {
		return
	}

	hwnd, _, _ := procCreateWindowExW.Call(
		0,
		uintptr(unsafe.Pointer(className)),
		uintptr(unsafe.Pointer(syscall.StringToUTF16Ptr("KeyStatsTray"))),
		WS_OVERLAPPED,
		CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0,
		0, 0,
		hInstance, 0,
	)
	if hwnd == 0 {
		return
	}
	t.hwnd = hwnd

	// Load icon from embedded ICO bytes
	t.hIcon = t.loadIconFromBytes()

	// Add tray icon
	t.addTrayIcon()

	// Message loop
	var m msg
	for {
		ret, _, _ := procGetMessageW.Call(uintptr(unsafe.Pointer(&m)), 0, 0, 0)
		if ret == 0 {
			break
		}
		if int32(ret) == -1 {
			break
		}
		procTranslateMessage.Call(uintptr(unsafe.Pointer(&m)))
		procDispatchMessageW.Call(uintptr(unsafe.Pointer(&m)))
	}

	// Cleanup
	t.removeTrayIcon()
	if t.hIcon != 0 {
		procDestroyIcon.Call(t.hIcon)
	}
	procDestroyWindow.Call(t.hwnd)
	procUnregisterClassW.Call(uintptr(unsafe.Pointer(className)), hInstance)
}

func (t *Tray) wndProc(hwnd uintptr, uMsg uint32, wParam, lParam uintptr) uintptr {
	switch uMsg {
	case TRAY_MSG_ID:
		switch lParam {
		case WM_LBUTTONUP:
			// Left-click: show main window immediately
			if t.onShow != nil {
				t.onShow()
			}
			return 0
		case WM_RBUTTONUP:
			// Right-click: show context menu
			t.showContextMenu()
			return 0
		}
	case WM_COMMAND:
		switch wParam {
		case ID_SHOW:
			if t.onShow != nil {
				t.onShow()
			}
			return 0
		case ID_QUIT:
			if t.onQuit != nil {
				t.onQuit()
			}
			return 0
		}
	case WM_APP_QUIT:
		procPostQuitMessage.Call(0)
		return 0
	case WM_DESTROY:
		procPostQuitMessage.Call(0)
		return 0
	}
	ret, _, _ := procDefWindowProcW.Call(hwnd, uintptr(uMsg), wParam, lParam)
	return ret
}

func (t *Tray) addTrayIcon() {
	nid := &notifyIconData{
		cbSize:           uint32(unsafe.Sizeof(notifyIconData{})),
		hWnd:             t.hwnd,
		uID:              1,
		uFlags:           NIF_MESSAGE | NIF_ICON | NIF_TIP,
		uCallbackMessage: TRAY_MSG_ID,
		hIcon:            t.hIcon,
	}
	tip := syscall.StringToUTF16("KeyStats — Keyboard Statistics")
	copy(nid.szTip[:], tip)
	procShellNotifyIconW.Call(NIM_ADD, uintptr(unsafe.Pointer(nid)))
}

func (t *Tray) removeTrayIcon() {
	nid := &notifyIconData{
		cbSize: uint32(unsafe.Sizeof(notifyIconData{})),
		hWnd:   t.hwnd,
		uID:    1,
	}
	procShellNotifyIconW.Call(NIM_DELETE, uintptr(unsafe.Pointer(nid)))
}

func (t *Tray) showContextMenu() {
	hMenu, _, _ := procCreatePopupMenu.Call()

	mShow, _ := syscall.UTF16PtrFromString("Show Window")
	mQuit, _ := syscall.UTF16PtrFromString("Quit")

	procAppendMenuW.Call(hMenu, MF_STRING, ID_SHOW, uintptr(unsafe.Pointer(mShow)))
	procAppendMenuW.Call(hMenu, MF_STRING, ID_QUIT, uintptr(unsafe.Pointer(mQuit)))

	var pt point
	procGetCursorPos.Call(uintptr(unsafe.Pointer(&pt)))

	procSetForegroundWindow.Call(t.hwnd)
	procTrackPopupMenu.Call(
		hMenu,
		TPM_RIGHTALIGN|TPM_BOTTOMALIGN,
		uintptr(pt.X),
		uintptr(pt.Y),
		0,
		t.hwnd,
		0,
	)
	procDestroyMenu.Call(hMenu)
}

func (t *Tray) loadIconFromBytes() uintptr {
	// ExtractIconExW can load icons from .ico files.
	// We write the embedded bytes to a temporary file, load the icon, then delete the file.
	tmpFile := filepath.Join(os.TempDir(), "keystats_tray_icon.ico")
	if err := os.WriteFile(tmpFile, t.icon, 0644); err != nil {
		return 0
	}
	defer os.Remove(tmpFile)

	tmpFilePtr, _ := syscall.UTF16PtrFromString(tmpFile)

	var hIcon uintptr
	ret, _, _ := procExtractIconExW.Call(
		uintptr(unsafe.Pointer(tmpFilePtr)),
		0,
		uintptr(unsafe.Pointer(&hIcon)),
		0,
		1,
	)
	if ret == 0 || hIcon == 0 {
		return 0
	}
	return hIcon
}

// ShowWindow is a helper that shows the Wails window via runtime.
func ShowWindow(ctx context.Context) {
	wailsRuntime.WindowShow(ctx)
}

// QuitApp is a helper that gracefully quits the Wails application.
func QuitApp(ctx context.Context) {
	wailsRuntime.Quit(ctx)
}
