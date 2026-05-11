#!/bin/bash
# Build script: wails handles frontend + bindings, go build applies ldflags for smaller binary
set -e
wails build -s
go build -ldflags="-s -w" -o build/bin/key-stats.exe .
echo "Built: $(ls -lh build/bin/key-stats.exe | awk '{print $5}')"
