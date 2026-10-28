package main

// #include "greet.h"
import "C"

func main() {
	C.greet()
}
