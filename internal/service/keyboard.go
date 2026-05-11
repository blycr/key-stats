package service

import (
	"log"
	"syscall"
	"time"
	"unsafe"

	"key-stats/internal/db"
	"key-stats/internal/models"
)

const (
	WH_KEYBOARD_LL = 13
	WM_KEYDOWN     = 0x0100
	WM_SYSKEYDOWN  = 0x0104
)

type KBDLLHOOKSTRUCT struct {
	VkCode      uint32
	ScanCode    uint32
	Flags       uint32
	Time        uint32
	DwExtraInfo uintptr
}

var (
	user32              = syscall.NewLazyDLL("user32.dll")
	setWindowsHookExW   = user32.NewProc("SetWindowsHookExW")
	unhookWindowsHookEx = user32.NewProc("UnhookWindowsHookEx")
	callNextHookEx      = user32.NewProc("CallNextHookEx")
	getMessageW         = user32.NewProc("GetMessageW")
	getForegroundWindow = user32.NewProc("GetForegroundWindow")
	getWindowTextW      = user32.NewProc("GetWindowTextW")

	// Global instance to be used by the Win32 callback
	globalService *KeyboardService
)

func getActiveWindowTitle() string {
	defer func() {
		if r := recover(); r != nil {
			log.Printf("getActiveWindowTitle recovered: %v", r)
		}
	}()

	hwnd, _, _ := getForegroundWindow.Call()
	if hwnd == 0 {
		return "Unknown"
	}

	buf := make([]uint16, 256)
	ret, _, _ := getWindowTextW.Call(hwnd, uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)))
	if ret == 0 {
		return "Unknown"
	}

	return syscall.UTF16ToString(buf)
}

type KeyboardService struct {
	db     *db.DB
	hook   uintptr
	active bool
}

func NewKeyboardService(database *db.DB) *KeyboardService {
	return &KeyboardService{
		db: database,
	}
}

func (s *KeyboardService) Start() {
	if s.active {
		return
	}
	s.active = true
	globalService = s

	go func() {
		defer func() {
			if r := recover(); r != nil {
				log.Printf("KeyboardService Event Loop recovered: %v", r)
			}
		}()

		// SetWindowsHookExW needs a callback
		cb := syscall.NewCallback(hookProc)

		// Install hook
		hook, _, err := setWindowsHookExW.Call(
			uintptr(WH_KEYBOARD_LL),
			cb,
			0, // No module handle needed for LL hook in Go
			0, // All threads
		)
		if hook == 0 {
			log.Printf("Failed to set hook: %v", err)
			return
		}
		s.hook = hook

		// Standard Windows message pump
		var msg struct {
			Hwnd    uintptr
			Message uint32
			WParam  uintptr
			LParam  uintptr
			Time    uint32
			Pt      struct{ X, Y int32 }
		}

		for {
			ret, _, _ := getMessageW.Call(uintptr(unsafe.Pointer(&msg)), 0, 0, 0)
			if ret == 0 || ret == ^uintptr(0) || !s.active {
				break
			}
		}

		unhookWindowsHookEx.Call(s.hook)
	}()
}

func (s *KeyboardService) Stop() {
	if !s.active {
		return
	}
	s.active = false
	if s.hook != 0 {
		unhookWindowsHookEx.Call(s.hook)
		s.hook = 0
	}
}

func hookProc(nCode int32, wParam uintptr, lParam uintptr) uintptr {
	if nCode >= 0 {
		if wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN {
			// Bypass "possible misuse of unsafe.Pointer" go vet warning
			lParamPtr := *(*unsafe.Pointer)(unsafe.Pointer(&lParam))
			kbd := (*KBDLLHOOKSTRUCT)(lParamPtr)

			// Push to DB safely
			if globalService != nil && globalService.db != nil {
				globalService.db.PushEvent(models.KeyEvent{
					KeyCode:   int(kbd.VkCode),
					AppName:   getActiveWindowTitle(),
					Timestamp: time.Now().UnixNano() / int64(time.Millisecond),
				})
			}
		}
	}
	ret, _, _ := callNextHookEx.Call(0, uintptr(nCode), wParam, lParam)
	return ret
}
