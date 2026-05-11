package db

import (
	"context"
	"database/sql"
	"log"
	"os"
	"path/filepath"
	"time"

	"key-stats/internal/models"

	_ "modernc.org/sqlite"
)

type DB struct {
	conn       *sql.DB
	eventChan  chan models.KeyEvent
	ctx        context.Context
	cancelFunc context.CancelFunc
}

func InitDB(dataDir string) (*DB, error) {
	if dataDir == "" {
		var err error
		dataDir, err = os.UserConfigDir()
		if err != nil {
			return nil, err
		}
		dataDir = filepath.Join(dataDir, "key-stats")
	}

	if err := os.MkdirAll(dataDir, 0755); err != nil {
		return nil, err
	}

	dbPath := filepath.Join(dataDir, "data.db")
	conn, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return nil, err
	}

	// PRAGMA for SQLite WAL mode and performance
	_, _ = conn.Exec("PRAGMA journal_mode=WAL;")
	_, _ = conn.Exec("PRAGMA synchronous=NORMAL;")

	query := `
	CREATE TABLE IF NOT EXISTS key_events (
		id          INTEGER PRIMARY KEY AUTOINCREMENT,
		key_code    INTEGER NOT NULL,
		app_name    TEXT    NOT NULL,
		timestamp   INTEGER NOT NULL
	);
	CREATE INDEX IF NOT EXISTS idx_key_events_timestamp ON key_events (timestamp);
	CREATE INDEX IF NOT EXISTS idx_key_events_key_code ON key_events (key_code);
	`
	if _, err := conn.Exec(query); err != nil {
		return nil, err
	}

	ctx, cancel := context.WithCancel(context.Background())
	d := &DB{
		conn:       conn,
		eventChan:  make(chan models.KeyEvent, 4096),
		ctx:        ctx,
		cancelFunc: cancel,
	}

	go d.batchWriter()
	return d, nil
}

func (d *DB) PushEvent(e models.KeyEvent) {
	select {
	case d.eventChan <- e:
	default:
		// Drop event safely if buffer is full, prevents memory leak & hook blocking
		log.Println("[Warning] Event buffer full, dropping key event")
	}
}

func (d *DB) GetConn() *sql.DB {
	return d.conn
}

// Reset deletes all recorded key events from the database.
func (d *DB) Reset() error {
	_, err := d.conn.Exec("DELETE FROM key_events")
	return err
}

func (d *DB) Close() {
	d.cancelFunc()
	close(d.eventChan)
	// Ensure the remaining events in channel are flushed before close
	var remaining []models.KeyEvent
	for e := range d.eventChan {
		remaining = append(remaining, e)
	}
	d.flush(remaining)
	d.conn.Close()
}

func (d *DB) batchWriter() {
	// Recover to prevent background worker crash bringing down the whole application
	defer func() {
		if r := recover(); r != nil {
			log.Printf("batchWriter recovered from panic: %v", r)
		}
	}()

	ticker := time.NewTicker(500 * time.Millisecond)
	defer ticker.Stop()

	var batch []models.KeyEvent

	for {
		select {
		case <-d.ctx.Done():
			return
		case e, ok := <-d.eventChan:
			if !ok {
				return
			}
			batch = append(batch, e)
			if len(batch) >= 256 {
				d.flush(batch)
				batch = nil
			}
		case <-ticker.C:
			if len(batch) > 0 {
				d.flush(batch)
				batch = nil
			}
		}
	}
}

func (d *DB) flush(batch []models.KeyEvent) {
	if len(batch) == 0 {
		return
	}

	tx, err := d.conn.Begin()
	if err != nil {
		log.Printf("failed to begin tx: %v", err)
		return
	}

	stmt, err := tx.Prepare("INSERT INTO key_events (key_code, app_name, timestamp) VALUES (?, ?, ?)")
	if err != nil {
		log.Printf("failed to prepare stmt: %v", err)
		tx.Rollback()
		return
	}
	defer stmt.Close()

	for _, e := range batch {
		if _, err := stmt.Exec(e.KeyCode, e.AppName, e.Timestamp); err != nil {
			log.Printf("failed to insert event: %v", err)
		}
	}

	if err := tx.Commit(); err != nil {
		log.Printf("failed to commit tx: %v", err)
	}
}
