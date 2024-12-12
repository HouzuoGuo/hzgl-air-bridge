package util

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/sha256"
	"encoding/binary"
)

func IsValidPubkey(pubKeyCompressed []byte) bool {
	withSignByte := append([]byte{0x02}, pubKeyCompressed...)
	curve := elliptic.P224()
	x, y := elliptic.UnmarshalCompressed(curve, withSignByte)
	if x == nil || y == nil {
		return false
	}
	if !curve.IsOnCurve(x, y) {
		return false
	}
	return true
}

func Ecdh(ephemeralPublicKey *ecdsa.PublicKey, privateKey *ecdsa.PrivateKey) []byte {
	x, _ := ephemeralPublicKey.Curve.ScalarMult(ephemeralPublicKey.X, ephemeralPublicKey.Y, privateKey.D.Bytes())
	return x.Bytes()
}

func Decrypt(cipherText, symmetricKey, tag []byte) ([]byte, error) {
	decryptionKey := symmetricKey[:16]
	iv := symmetricKey[16:]
	block, err := aes.NewCipher(decryptionKey)
	if err != nil {
		return nil, err
	}
	aesGcm, err := cipher.NewGCMWithNonceSize(block, len(iv))
	if err != nil {
		return nil, err
	}
	plainText, err := aesGcm.Open(nil, iv, append(cipherText, tag...), nil)
	if err != nil {
		return nil, err
	}
	return plainText, nil
}

func Kdf(secret, ephemeralKey []byte) []byte {
	hash := sha256.New()
	hash.Write(secret)
	counter := make([]byte, 4)
	binary.BigEndian.PutUint32(counter, 1)
	hash.Write(counter)
	hash.Write(ephemeralKey)
	return hash.Sum(nil)
}
