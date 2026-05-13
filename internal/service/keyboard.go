package service

import (
	"context"
	"log"
	"sync"
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
	db            *db.DB
	hook          uintptr
	active        bool
	rtChan        chan map[string]interface{}
	rtCtx         context.Context
	rtCancel      context.CancelFunc
	mu            sync.Mutex
	latestKeyName string
	latestKeyTs   int64
}

func NewKeyboardService(database *db.DB) *KeyboardService {
	return &KeyboardService{
		db:     database,
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

	// Start a dedicated goroutine to drain rtChan and prevent backpressure.
	// rtChan is written to from hookProc (non-blocking); rtEmitter drains it.
	// We intentionally do NOT emit events to JS — polling avoids Svelte 4 conflicts.
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

// rtEmitter drains rtChan to prevent the channel from filling up in hookProc.
// We intentionally do NOT call runtime.EventsEmit here — emitting custom events
// from Go interferes with Svelte 4's document-level event delegation, causing
// button unresponsiveness and broken reactivity. The frontend polls GetLatestKeyPress()
// at 100ms instead.
func (s *KeyboardService) rtEmitter() {
	for {
		select {
		case <-s.rtCtx.Done():
			return
		case _, ok := <-s.rtChan:
			if !ok {
				return
			}
		}
	}
}

// GetLatestKey returns the most recent key press name and timestamp (ms).
// Returns empty string and 0 if no key has been pressed yet.
func (s *KeyboardService) GetLatestKey() (string, int64) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.latestKeyName, s.latestKeyTs
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
			// hookProc runs on a Windows callback thread; keep this fast.
			// The rtEmitter goroutine drains rtChan. Frontend polls GetLatestKeyPress().
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
				globalService.mu.Lock()
				globalService.latestKeyName = name
				globalService.latestKeyTs = time.Now().UnixNano() / int64(time.Millisecond)
				globalService.mu.Unlock()
			}
		}
	}
	ret, _, _ := callNextHookEx.Call(0, uintptr(nCode), wParam, lParam)
	return ret
}
