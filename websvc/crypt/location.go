package crypt

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/sha256"
	"encoding/binary"
	"math/big"
	"time"
)

// With kudos to github.com/dchristl, this is largely a translation of https://github.com/dchristl/macless-haystack/blob/main/macless_haystack/lib/findMy/decrypt_reports.dart (GPLv3) into go.

type FindMyReport struct {
	Payload    []byte
	Timestamp  time.Time
	Confidence uint8
}

type FindMyLocationReport struct {
	Latitude   float64
	Longitude  float64
	Accuracy   uint8
	Timestamp  time.Time
	Confidence uint8
}

func DecryptReport(report FindMyReport, locationPrivateKey []byte) (*FindMyLocationReport, error) {
	curve := elliptic.P224()
	payloadData := report.Payload
	ephemeralKeyBytes := payloadData[len(payloadData)-16-10-57 : len(payloadData)-16-10]
	encData := payloadData[len(payloadData)-16-10 : len(payloadData)-16]
	tag := payloadData[len(payloadData)-16:]
	decodeTimeAndConfidence(payloadData, &report)
	privateKey := new(ecdsa.PrivateKey)
	privateKey.D = new(big.Int).SetBytes(locationPrivateKey)
	privateKey.PublicKey.Curve = curve
	x, y := elliptic.Unmarshal(curve, ephemeralKeyBytes)
	ephemeralPublicKey := &ecdsa.PublicKey{Curve: curve, X: x, Y: y}
	sharedKeyBytes := ecdh(ephemeralPublicKey, privateKey)
	derivedKey := kdf(sharedKeyBytes, ephemeralKeyBytes)
	decryptedPayload, err := decryptPayload(encData, derivedKey, tag)
	if err != nil {
		return nil, err
	}
	locationReport := decodePayload(decryptedPayload, report)
	return locationReport, nil
}

func decodeTimeAndConfidence(payloadData []byte, report *FindMyReport) {
	seenTimeStamp := binary.BigEndian.Uint32(payloadData[:4])
	report.Timestamp = time.Date(2001, 1, 1, 0, 0, 0, 0, time.UTC).Add(time.Duration(seenTimeStamp) * time.Second).Local()
	report.Confidence = payloadData[4]
}

func ecdh(ephemeralPublicKey *ecdsa.PublicKey, privateKey *ecdsa.PrivateKey) []byte {
	x, _ := ephemeralPublicKey.Curve.ScalarMult(ephemeralPublicKey.X, ephemeralPublicKey.Y, privateKey.D.Bytes())
	return x.Bytes()
}

func decodePayload(payload []byte, report FindMyReport) *FindMyLocationReport {
	latInt := int32(binary.BigEndian.Uint32(payload[0:4]))
	longInt := int32(binary.BigEndian.Uint32(payload[4:8]))
	return &FindMyLocationReport{
		Latitude:   float64(latInt) / 10000000,
		Longitude:  float64(longInt) / 10000000,
		Accuracy:   payload[8],
		Timestamp:  report.Timestamp,
		Confidence: report.Confidence,
	}
}

func decryptPayload(cipherText, symmetricKey, tag []byte) ([]byte, error) {
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

func kdf(secret, ephemeralKey []byte) []byte {
	hash := sha256.New()
	hash.Write(secret)
	counter := make([]byte, 4)
	binary.BigEndian.PutUint32(counter, 1)
	hash.Write(counter)
	hash.Write(ephemeralKey)
	return hash.Sum(nil)
}

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
