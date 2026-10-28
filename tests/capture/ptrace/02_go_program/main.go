// Compiles a.c as `go build` does with cgo: Go starts the compiler itself,
// through its own system calls, without the C library.
package main

import (
	"os"
	"os/exec"
)

func main() {
	cc := exec.Command("cc", "-c", "a.c")
	cc.Stdout = os.Stdout
	cc.Stderr = os.Stderr
	if cc.Run() != nil {
		os.Exit(1)
	}
}
