package util

import (
	"bytes"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"fmt"
)

func HexString(in []byte) string {
	var ret bytes.Buffer
	for _, b := range in {
		ret.WriteString(fmt.Sprintf("%.2x ", b))
	}
	return ret.String()
}

func HashedBeacon(in []byte) (string, error) {
	hasher := sha256.New()
	hasher.Write(in)
	hashedBytes := hasher.Sum(nil)
	encodedHash := base64.StdEncoding.EncodeToString(hashedBytes)
	return encodedHash, nil
}

func ByteFromBits(bits []byte) byte {
	var ret byte
	for i, bit := range bits {
		if bit == 1 {
			ret |= 1 << (7 - i)
		}
	}
	return ret
}

func Base64Decode(in string) []byte {
	decodedBytes, err := base64.StdEncoding.DecodeString(in)
	if err != nil {
		panic(err)
	}
	return decodedBytes
}

func TwoBeBytes(in []byte) int {
	return int(binary.BigEndian.Uint16(in))
}
