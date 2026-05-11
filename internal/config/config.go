package config

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

// WindowState stores the last known window dimensions.
type WindowState struct {
	Width  int
	Height int
}

func envPath() string {
	dir, _ := os.UserConfigDir()
	return filepath.Join(dir, "key-stats", ".env")
}

// Load reads window state from the .env file.
// Returns defaults (1280x800) if the file does not exist.
func Load() (*WindowState, error) {
	data, err := os.ReadFile(envPath())
	if err != nil {
		if os.IsNotExist(err) {
			return &WindowState{Width: 1280, Height: 800}, nil
		}
		return nil, err
	}

	ws := &WindowState{Width: 1280, Height: 800}
	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		parts := strings.SplitN(line, "=", 2)
		if len(parts) != 2 {
			continue
		}
		key := strings.TrimSpace(parts[0])
		val := strings.TrimSpace(parts[1])
		switch key {
		case "WINDOW_WIDTH":
			if v, err := strconv.Atoi(val); err == nil && v > 0 {
				ws.Width = v
			}
		case "WINDOW_HEIGHT":
			if v, err := strconv.Atoi(val); err == nil && v > 0 {
				ws.Height = v
			}
		}
	}
	return ws, nil
}

// Save writes window state to the .env file.
func Save(ws *WindowState) error {
	path := envPath()
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return err
	}
	content := fmt.Sprintf("# KeyStats Configuration\nWINDOW_WIDTH=%d\nWINDOW_HEIGHT=%d\n", ws.Width, ws.Height)
	return os.WriteFile(path, []byte(content), 0644)
}
