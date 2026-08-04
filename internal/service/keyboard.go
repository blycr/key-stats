package service

import (
	"log"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
	"unsafe"

	"key-stats/internal/db"
	"key-stats/internal/models"
	"key-stats/internal/win32"
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
	// Global instance to be used by the Win32 callback
	globalService *KeyboardService

	// Reusable UTF-16 buffer pool for getActiveWindowTitle to reduce GC pressure.
	titleBufPool = sync.Pool{
		New: func() interface{} {
			b := make([]uint16, 256)
			return &b
		},
	}
)

// getActiveWindowTitle returns the title of the foreground window.
//
// GetWindowTextW sends a synchronous WM_GETTEXT across processes, which can
// block on a busy target window. We cache the title keyed by the foreground
// hwnd and only issue the syscall when the foreground window actually changes,
// keeping the hook hot path free of cross-process IPC on every keystroke.
func getActiveWindowTitle() string {
	hwnd, _, _ := win32.GetForegroundWindow.Call()
	if hwnd == 0 {
		lastHwnd = 0
		return "Unknown"
	}
	if hwnd == lastHwnd {
		return lastTitle
	}

	lastHwnd = hwnd
	lastTitle = "Unknown"

	defer func() {
		if r := recover(); r != nil {
			log.Printf("getActiveWindowTitle recovered: %v", r)
			lastTitle = "Unknown"
		}
	}()

	bufPtr := titleBufPool.Get().(*[]uint16)
	buf := *bufPtr
	ret, _, _ := win32.GetWindowTextW.Call(hwnd, uintptr(unsafe.Pointer(&buf[0])), uintptr(len(buf)))
	if ret > 0 {
		// UTF16ToString scans for the null terminator, so the full backing slice is safe.
		lastTitle = syscall.UTF16ToString(buf)
	}
	titleBufPool.Put(bufPtr)
	return lastTitle
}

// Cache of (foreground hwnd, title). hookProc runs on a single hook thread, so
// these are only ever touched from that one goroutine.
var lastHwnd uintptr
var lastTitle string

type KeyboardService struct {
	db     *db.DB
	hook   uintptr
	active bool
	// paused stops new keystrokes from being recorded when true.
	paused atomic.Bool
}

func NewKeyboardService(database *db.DB) *KeyboardService {
	return &KeyboardService{
		db: database,
	}
}

// SetPaused controls whether keystrokes are recorded. The frontend "Paused"
// toggle drives this so the recorder actually stops, not just the UI poll.
func (s *KeyboardService) SetPaused(p bool) {
	s.paused.Store(p)
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
		hook, _, err := win32.SetWindowsHookExW.Call(
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

		// Standard Windows message pump. Only GetMessageW is needed to keep the
		// hook callback firing; no TranslateMessage/DispatchMessage required.
		var msg win32.MSG
		for {
			ret, _, _ := win32.GetMessageW.Call(uintptr(unsafe.Pointer(&msg)), 0, 0, 0)
			if ret == 0 || ret == ^uintptr(0) || !s.active {
				break
			}
		}

		win32.UnhookWindowsHookEx.Call(s.hook)
	}()
}

func (s *KeyboardService) Stop() {
	if !s.active {
		return
	}
	s.active = false
	if s.hook != 0 {
		win32.UnhookWindowsHookEx.Call(s.hook)
		s.hook = 0
	}
}

func hookProc(nCode int32, wParam uintptr, lParam uintptr) uintptr {
	if nCode >= 0 {
		if wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN {
			// Bypass "possible misuse of unsafe.Pointer" go vet warning
			lParamPtr := *(*unsafe.Pointer)(unsafe.Pointer(&lParam))
			kbd := (*KBDLLHOOKSTRUCT)(lParamPtr)

			vk := int(kbd.VkCode)

			// Push to DB safely (skipped while paused)
			if globalService != nil && globalService.db != nil && !globalService.paused.Load() {
				globalService.db.PushEvent(models.KeyEvent{
					KeyCode:   vk,
					AppName:   getActiveWindowTitle(),
					Timestamp: time.Now().UnixNano() / int64(time.Millisecond),
				})
			}
		}
	}
	ret, _, _ := win32.CallNextHookEx.Call(0, uintptr(nCode), wParam, lParam)
	return ret
}
