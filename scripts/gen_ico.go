// gen_ico.go - Generate multi-size Windows ICO from PNG (stdlib only)
package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"os"

	xdraw "golang.org/x/image/draw"
)

func main() {
	if len(os.Args) < 3 {
		fmt.Fprintln(os.Stderr, "Usage: go run gen_ico.go <input.png> <output.ico>")
		os.Exit(1)
	}

	srcPath := os.Args[1]
	outPath := os.Args[2]

	srcFile, err := os.Open(srcPath)
	if err != nil {
		fmt.Fprintln(os.Stderr, "open input:", err)
		os.Exit(1)
	}
	defer srcFile.Close()

	src, err := png.Decode(srcFile)
	if err != nil {
		fmt.Fprintln(os.Stderr, "decode png:", err)
		os.Exit(1)
	}

	sizes := []int{16, 24, 32, 48, 64, 128, 256}

	type frame struct {
		width  int
		height int
		data   []byte
	}
	frames := make([]frame, len(sizes))

	for i, size := range sizes {
		dst := image.NewRGBA(image.Rect(0, 0, size, size))
		xdraw.CatmullRom.Scale(dst, dst.Bounds(), src, src.Bounds(), draw.Over, nil)

		var buf bytes.Buffer
		if err := png.Encode(&buf, dst); err != nil {
			fmt.Fprintln(os.Stderr, "encode png:", err)
			os.Exit(1)
		}
		frames[i] = frame{size, size, buf.Bytes()}
	}

	var out bytes.Buffer

	// ICO header
	binary.Write(&out, binary.LittleEndian, uint16(0))     // Reserved
	binary.Write(&out, binary.LittleEndian, uint16(1))     // Type: ICO
	binary.Write(&out, binary.LittleEndian, uint16(len(frames))) // Count

	// Directory entries
	offset := 6 + len(frames)*16
	for _, f := range frames {
		w, h := f.width, f.height
		if w >= 256 {
			w = 0
		}
		if h >= 256 {
			h = 0
		}
		out.WriteByte(byte(w))
		out.WriteByte(byte(h))
		out.WriteByte(0) // Colors
		out.WriteByte(0) // Reserved
		binary.Write(&out, binary.LittleEndian, uint16(1)) // Planes
		binary.Write(&out, binary.LittleEndian, uint16(32)) // BPP
		binary.Write(&out, binary.LittleEndian, uint32(len(f.data)))
		binary.Write(&out, binary.LittleEndian, uint32(offset))
		offset += len(f.data)
	}

	// Image data
	for _, f := range frames {
		out.Write(f.data)
	}

	if err := os.WriteFile(outPath, out.Bytes(), 0644); err != nil {
		fmt.Fprintln(os.Stderr, "write ico:", err)
		os.Exit(1)
	}

	fmt.Printf("Generated %s with %d frames: ", outPath, len(frames))
	for i, f := range frames {
		if i > 0 {
			fmt.Print(", ")
		}
		fmt.Printf("%dx%d", f.width, f.height)
	}
	fmt.Println()
}
