package config

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

// AppConfig holds all application settings.
// Add new fields here to expand the configuration surface.
type AppConfig struct {
	// Window
	WindowWidth  int
	WindowHeight int

	// Appearance
	Theme string

	// Behavior
	StartMinimized bool
	AutoStart      bool

	// Data storage (read-only from .env — switch by copying .env next to exe)
	PortableMode bool
}

// defaults returns a new config with factory defaults.
func defaults() *AppConfig {
	return &AppConfig{
		WindowWidth:    1280,
		WindowHeight:   800,
		Theme:          "dark",
		StartMinimized: false,
		AutoStart:      false,
		PortableMode:   false,
	}
}

// envPath returns the absolute path to the .env file.
// Portable mode is auto-detected: if .env exists next to the executable, use it.
func envPath() string {
	exe, err := os.Executable()
	if err == nil {
		portable := filepath.Join(filepath.Dir(exe), ".env")
		if _, err := os.Stat(portable); err == nil {
			return portable
		}
	}
	dir, _ := os.UserConfigDir()
	return filepath.Join(dir, "key-stats", ".env")
}

// DataDir returns the directory for SQLite and other data files.
// If portable mode is active, it returns the executable's directory.
func DataDir() (string, error) {
	cfg, err := Load()
	if err != nil {
		return "", err
	}
	if cfg.PortableMode {
		exe, err := os.Executable()
		if err == nil {
			return filepath.Dir(exe), nil
		}
	}
	dir, _ := os.UserConfigDir()
	return filepath.Join(dir, "key-stats"), nil
}

// Load reads configuration from the .env file.
func Load() (*AppConfig, error) {
	data, err := os.ReadFile(envPath())
	if err != nil {
		if os.IsNotExist(err) {
			return defaults(), nil
		}
		return nil, err
	}

	cfg := defaults()
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
				cfg.WindowWidth = v
			}
		case "WINDOW_HEIGHT":
			if v, err := strconv.Atoi(val); err == nil && v > 0 {
				cfg.WindowHeight = v
			}
		case "THEME":
			cfg.Theme = val
		case "START_MINIMIZED":
			cfg.StartMinimized = parseBool(val)
		case "AUTO_START":
			cfg.AutoStart = parseBool(val)
		case "PORTABLE_MODE":
			cfg.PortableMode = parseBool(val)
		}
	}
	return cfg, nil
}

// Save persists the configuration to the .env file with helpful comments.
// This also serves as a self-documenting template for users.
func Save(cfg *AppConfig) error {
	path := envPath()
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return err
	}

	content := fmt.Sprintf(`# KeyStats Configuration
# ======================
# Edit values below or uncomment lines to customize.
# Changes take effect on next launch (some may require restart).

# Window Size (last known dimensions)
WINDOW_WIDTH=%d
WINDOW_HEIGHT=%d

# Appearance
# THEME=%s         # dark | light | auto

# Behavior
# START_MINIMIZED=false    # Start minimized to system tray
# AUTO_START=false         # Launch on system startup

# Data Storage
# PORTABLE_MODE=false      # If true, .env and data.db live next to the executable.
#                          # To enable portable mode: copy this .env file next to key-stats.exe
`, cfg.WindowWidth, cfg.WindowHeight, cfg.Theme)

	return os.WriteFile(path, []byte(content), 0644)
}

// ToMap converts AppConfig to a flat map for JSON/API transport.
// Frontend can iterate this to build a dynamic settings UI.
func (c *AppConfig) ToMap() map[string]interface{} {
	return map[string]interface{}{
		"windowWidth":    c.WindowWidth,
		"windowHeight":   c.WindowHeight,
		"theme":          c.Theme,
		"startMinimized": c.StartMinimized,
		"autoStart":      c.AutoStart,
		"portableMode":   c.PortableMode,
	}
}

// UpdateFromMap updates AppConfig fields from a flat map.
// Returns a list of keys that were actually changed.
func (c *AppConfig) UpdateFromMap(m map[string]interface{}) []string {
	var changed []string
	if v, ok := m["windowWidth"]; ok {
		if f, ok := toInt(v); ok && f > 0 {
			c.WindowWidth = f
			changed = append(changed, "windowWidth")
		}
	}
	if v, ok := m["windowHeight"]; ok {
		if f, ok := toInt(v); ok && f > 0 {
			c.WindowHeight = f
			changed = append(changed, "windowHeight")
		}
	}
	if v, ok := m["theme"]; ok {
		if s, ok := v.(string); ok {
			c.Theme = s
			changed = append(changed, "theme")
		}
	}
	if v, ok := m["startMinimized"]; ok {
		c.StartMinimized = toBool(v)
		changed = append(changed, "startMinimized")
	}
	if v, ok := m["autoStart"]; ok {
		c.AutoStart = toBool(v)
		changed = append(changed, "autoStart")
	}
	return changed
}

func parseBool(s string) bool {
	s = strings.ToLower(strings.TrimSpace(s))
	return s == "true" || s == "1" || s == "yes"
}

func toInt(v interface{}) (int, bool) {
	switch val := v.(type) {
	case int:
		return val, true
	case float64:
		return int(val), true
	case string:
		if i, err := strconv.Atoi(val); err == nil {
			return i, true
		}
	}
	return 0, false
}

func toBool(v interface{}) bool {
	switch val := v.(type) {
	case bool:
		return val
	case string:
		return parseBool(val)
	case float64:
		return val != 0
	case int:
		return val != 0
	}
	return false
}
