package config

import (
	"encoding/json"
	"os"
	"path/filepath"
)

type WindowState struct {
	Width  int `json:"width"`
	Height int `json:"height"`
}

type Config struct {
	Window WindowState `json:"window"`
}

var configPath string

func init() {
	configDir, _ := os.UserConfigDir()
	configPath = filepath.Join(configDir, "key-stats", "config.json")
}

func Load() (*Config, error) {
	data, err := os.ReadFile(configPath)
	if err != nil {
		if os.IsNotExist(err) {
			return &Config{Window: WindowState{Width: 1280, Height: 800}}, nil
		}
		return nil, err
	}
	var cfg Config
	if err := json.Unmarshal(data, &cfg); err != nil {
		return &Config{Window: WindowState{Width: 1280, Height: 800}}, nil
	}
	return &cfg, nil
}

func Save(cfg *Config) error {
	if err := os.MkdirAll(filepath.Dir(configPath), 0755); err != nil {
		return err
	}
	data, err := json.MarshalIndent(cfg, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(configPath, data, 0644)
}
