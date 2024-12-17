package findmy

import (
	"context"
	"crypto/ecdsa"
	"crypto/elliptic"
	"encoding/binary"
	"fmt"
	"math/big"
	"slices"
	"sort"
	"time"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/util"
)

// DecryptedLocation represents a location report encrypted by an Apple device
type DecryptedLocation struct {
	// Latitude in decimal degrees, positive is north and negative is south.
	Latitude float64
	// Longitude in decimal degrees, positive is east and negative is east.
	Longitude float64
	// AccuracyMetres is the self-reported location accuracy of the apple device which heard the beacon.
	AccuracyMetres byte
	// ConfidencePercent appears to fluctuate between 0-3 for accuracy < 150 metres. I am uncertain what the percentage means.
	ConfidencePercent byte
	// Timestamp is the wall clock reported by the apple device which heard the beacon.
	Timestamp time.Time
}

func (loc *DecryptedLocation) Valid() bool {
	return loc.AccuracyMetres > 0
}

// DecryptReport decrypts the encrypted payload of a location report.
// Apple device encrypts its own location by the public key broadcast by a bluetooth beacon, and uploads the encrypted location report to Apple.
func (client *Client) DecryptReport(report ReportResponse, locationPrivateKey []byte) (DecryptedLocation, error) {
	var decryptedLocation DecryptedLocation
	curve := elliptic.P224()
	payloadData := report.EncryptedPayload()
	ephemeralKeyBytes := payloadData[len(payloadData)-16-10-57 : len(payloadData)-16-10]
	encData := payloadData[len(payloadData)-16-10 : len(payloadData)-16]
	tag := payloadData[len(payloadData)-16:]
	privateKey := new(ecdsa.PrivateKey)
	privateKey.D = new(big.Int).SetBytes(locationPrivateKey)
	privateKey.PublicKey.Curve = curve
	x, y := elliptic.Unmarshal(curve, ephemeralKeyBytes)
	ephemeralPublicKey := &ecdsa.PublicKey{Curve: curve, X: x, Y: y}
	sharedKeyBytes := util.Ecdh(ephemeralPublicKey, privateKey)
	derivedKey := util.Kdf(sharedKeyBytes, ephemeralKeyBytes)
	decryptedPayload, err := util.Decrypt(encData, derivedKey, tag)
	if err != nil {
		return DecryptedLocation{}, err
	}
	// The timestamp and confidence are not encrypted.
	decodeTimeAndConfidence(payloadData, &decryptedLocation)
	// The location and accuracy were encrypted.
	decryptedLocation.Latitude = float64(int32(binary.BigEndian.Uint32(decryptedPayload[0:4]))) / 10000000
	decryptedLocation.Longitude = float64(int32(binary.BigEndian.Uint32(decryptedPayload[4:8]))) / 10000000
	decryptedLocation.AccuracyMetres = decryptedPayload[8]
	return decryptedLocation, nil
}

func decodeTimeAndConfidence(encryptedPayload []byte, loc *DecryptedLocation) {
	seenTimeStamp := binary.BigEndian.Uint32(encryptedPayload[:4])
	loc.Timestamp = time.Date(2001, 1, 1, 0, 0, 0, 0, time.UTC).Add(time.Duration(seenTimeStamp) * time.Second).Local()
	loc.ConfidencePercent = encryptedPayload[4]
}

// DownloadLocation downloads the recent (from latest to oldest) location reports and decrypts them.
func (client *Client) DownloadLocation(ctx context.Context, lookBackDays int) (ret []DecryptedLocation, err error) {
	locAdvertKeyHash, err := util.HashedBeacon(client.LocationAdvertisementKey)
	if err != nil {
		return nil, fmt.Errorf("failed to calculate hashed advertisement key: %w", err)
	}
	multiReportResp, err := client.doReportRequest(ctx, MultiReportRequest{HashedAdvertisedPublicKey: []string{locAdvertKeyHash}, Days: lookBackDays})
	if err != nil {
		return nil, err
	}
	// There are occasional decryption failures (about 1 in 50).
	var decryptErrCount int
	for _, resp := range multiReportResp.Results {
		loc, err := client.DecryptReport(resp, client.LocationPrivateKey)
		if err != nil || loc.Latitude == 0 && loc.Longitude == 0 && loc.AccuracyMetres == 0 {
			// About 0.5% of the location reports decrypt successfully but don't contain location data.
			// An even smaller portion contain corrupted encrypted payload that fail to decrypt.
			decryptErrCount++
		} else {
			ret = append(ret, loc)
		}
	}
	if decryptErrCount > 0 && len(ret) == 0 {
		err = fmt.Errorf("failed to decrypt all %d location reports, is the location private key correct?", decryptErrCount)
	}
	// Remove invalid reports and sort them from latest to oldest.
	slices.DeleteFunc(ret, func(loc DecryptedLocation) bool {
		return loc.Longitude == 0 && loc.Latitude == 0 && loc.AccuracyMetres == 0
	})
	sort.Slice(ret, func(i, j int) bool {
		return ret[j].Timestamp.Before(ret[i].Timestamp)
	})
	return
}
