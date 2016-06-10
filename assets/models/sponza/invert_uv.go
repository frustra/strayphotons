package main

import (
	"io/ioutil"
	"math"
	"encoding/binary"
)

func main() {
	buf, err := ioutil.ReadFile("sponza.bin")
	if err != nil {
		panic(err)
	}

	for i := 24; i < 5900992; i += 32 {
		u := math.Float32frombits(binary.LittleEndian.Uint32(buf[i:]))
		v := 1.0 - math.Float32frombits(binary.LittleEndian.Uint32(buf[i+4:]))
		binary.LittleEndian.PutUint32(buf[i:], math.Float32bits(u))
		binary.LittleEndian.PutUint32(buf[i+4:], math.Float32bits(v))
	}

	err = ioutil.WriteFile("sponza.bin", buf, 0644)
	if err != nil {
		panic(err)
	}
}

