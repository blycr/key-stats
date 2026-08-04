package db

import (
	"context"
	"database/sql"
	"log"
	"os"
	"path/filepath"
	"sync"
	"sync/atomic"
	"time"

	"key-stats/internal/config"
	"key-stats/internal/models"

	_ "modernc.org/sqlite"
)

type DB struct {
	conn       *sql.DB
	eventChan  chan models.KeyEvent
	ctx        context.Context
	cancelFunc context.CancelFunc
	wg         sync.WaitGroup

	// dropCount counts events dropped when eventChan is full, aggregated so the
	// hook callback thread never performs I/O (log writes) under buffer pressure.
	dropCount atomic.Int64
}

func InitDB(dataDir string) (*DB, error) {
	if dataDir == "" {
		var err error
		dataDir, err = os.UserConfigDir()
		if err != nil {
			return nil, err
		}
		dataDir = filepath.Join(dataDir, config.AppDirName)
	}

	if err := os.MkdirAll(dataDir, 0755); err != nil {
		return nil, err
	}

	dbPath := filepath.Join(dataDir, "data.db")
	conn, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return nil, err
	}
	// Single connection: PRAGMA settings are per-connection, so limiting the pool
	// guarantees synchronous=NORMAL (and WAL) apply to every query. This app has
	// no need for parallel connections.
	conn.SetMaxOpenConns(1)
	conn.SetMaxIdleConns(1)

	// PRAGMA for SQLite WAL mode and performance
	if _, err := conn.Exec("PRAGMA journal_mode=WAL;"); err != nil {
		log.Printf("WAL mode not available: %v", err)
	}
	if _, err := conn.Exec("PRAGMA synchronous=NORMAL;"); err != nil {
		log.Printf("synchronous NORMAL not available: %v", err)
	}

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

	d.wg.Add(1)
	go d.batchWriter()
	return d, nil
}

func (d *DB) PushEvent(e models.KeyEvent) {
	select {
	case d.eventChan <- e:
	default:
		// Drop event safely if buffer is full, prevents memory leak & hook blocking.
		// Only bump a counter here — logging on the hook callback thread would block.
		d.dropCount.Add(1)
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
	// Stop the writer first so it flushes its in-flight batch, then drain what's
	// still in the channel — no collected keystroke is dropped on shutdown.
	d.cancelFunc()
	d.wg.Wait()
	close(d.eventChan)
	var remaining []models.KeyEvent
	for e := range d.eventChan {
		remaining = append(remaining, e)
	}
	d.flush(remaining)
	d.conn.Close()
}

func (d *DB) batchWriter() {
	defer func() {
		if r := recover(); r != nil {
			log.Printf("batchWriter recovered from panic: %v", r)
		}
		d.wg.Done()
	}()

	ticker := time.NewTicker(500 * time.Millisecond)
	defer ticker.Stop()

	var batch []models.KeyEvent

	for {
		select {
		case <-d.ctx.Done():
			if len(batch) > 0 {
				d.flush(batch)
			}
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
			if n := d.dropCount.Swap(0); n > 0 {
				log.Printf("[Warning] Event buffer full, dropped %d key events", n)
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
