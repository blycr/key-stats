package service

import (
	"context"
	"log"
	"syscall"
	"time"
	"unsafe"

	"key-stats/internal/db"
	"key-stats/internal/models"
	"key-stats/internal/stats"
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
	db       *db.DB
	hook     uintptr
	active   bool
	emit     func(eventName string, data ...interface{})
	rtChan   chan map[string]interface{}
	rtCtx    context.Context
	rtCancel context.CancelFunc
}

func NewKeyboardService(database *db.DB, emit func(string, ...interface{})) *KeyboardService {
	return &KeyboardService{
		db:     database,
		emit:   emit,
		rtChan: make(chan map[string]interface{}, 256),
	}
}

func (s *KeyboardService) Start() {
	if s.active {
		return
	}
	s.active = true
	globalService = s
	s.rtCtx, s.rtCancel = context.WithCancel(context.Background())

	// Start a dedicated goroutine to safely emit real-time events.
	// runtime.EventsEmit must NOT be called from the syscall callback (hookProc)
	// because the Go scheduler cannot safely handle blocking/channel operations
	// on the Windows callback thread.
	go s.rtEmitter()

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

// rtEmitter runs in its own goroutine and safely calls emit() for each
// real-time key event. This avoids calling runtime.EventsEmit from the
// Windows syscall callback thread.
func (s *KeyboardService) rtEmitter() {
	for {
		select {
		case <-s.rtCtx.Done():
			return
		case ev, ok := <-s.rtChan:
			if !ok {
				return
			}
			if s.emit != nil {
				s.emit("key-pressed", ev)
			}
		}
	}
}

func (s *KeyboardService) Stop() {
	if !s.active {
		return
	}
	s.active = false
	if s.rtCancel != nil {
		s.rtCancel()
	}
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

			vk := int(kbd.VkCode)

			// Push to DB safely
			if globalService != nil && globalService.db != nil {
				globalService.db.PushEvent(models.KeyEvent{
					KeyCode:   vk,
					AppName:   getActiveWindowTitle(),
					Timestamp: time.Now().UnixNano() / int64(time.Millisecond),
				})
			}

			// Queue real-time event via non-blocking channel send.
			// hookProc runs on a Windows callback thread; we must not call
			// runtime.EventsEmit here. The rtEmitter goroutine handles it.
			if globalService != nil && globalService.active {
				name := stats.VKToName(vk)
				select {
				case globalService.rtChan <- map[string]interface{}{
					"keyCode": vk,
					"keyName": name,
				}:
				default:
					// Channel full — drop real-time event to avoid blocking hook
				}
			}
		}
	}
	ret, _, _ := callNextHookEx.Call(0, uintptr(nCode), wParam, lParam)
	return ret
}
