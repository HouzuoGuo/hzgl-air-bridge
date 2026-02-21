package findmy

import (
	"context"
	"fmt"
	"log"
	"maps"
	"math/rand/v2"
	"time"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/util"
)

// DataByte represents a byte of data retrieved from Find My network.
// Note that bits are transmitted independently from each other and will arrive out of order.
type DataByte struct {
	// Value is the byte value, assembled from reports of individually retrieved bits.
	Value byte
	// ReportTime is the timestamp of the latest bit retrieved across the byte.
	ReportTime time.Time
	// BitReportSpread is the maximum spread (latest - oldest) of the report representing each bit.
	BitReportSpread time.Duration
	// BitReport is the retrieved FindMy location report correspoding to each bit in big endian (most significant first).
	BitReport []ReportResponse
}

// dataAdvertPublicKey returns a public key constructed for a bit value guess (e.g. the 3rd bit of message 10 is true) .
func (client *Client) dataAdvertPublicKey(messageID int, guessBitIndex int, guessBitValue bool) (attempt byte, ret []byte) {
	// See firmware/src/bt.cpp (bt_set_addr_and_payload_for_bit) for the payload structure.
	ret = make([]byte, 28)
	copy(ret, client.CustomMagicKey)
	ret[24] = byte(messageID)
	ret[25] = byte(guessBitIndex)
	if guessBitValue {
		ret[26] = 1
	}
	// Some public keys constructed by the template above are invalid. Manipulate the attempt number byte to find a valid public key.
	for attempt = range 255 {
		ret[27] = byte(attempt)
		if util.IsValidPubkey(ret) {
			return
		}
	}
	return 0, nil
}

// DownloadDataByte retrieves a data byte for the specified message ID and byte index.
// maxBitReportSpread defines the maximum time between updates of the byte. It prevents incidental retrieval of outdated bits.
func (client *Client) DownloadDataByte(ctx context.Context, messageID, byteIndex, lookBackDays int, maxBitReportSpread time.Duration) (*DataByte, error) {
	bitTrueAdvId := make(map[int]string)
	bitFalseAdvId := make(map[int]string)
	for bi := byteIndex * 8; bi < (byteIndex+1)*8; bi++ {
		// Construct a public key to guess that the bit has true value.
		_, bitTrueKey := client.dataAdvertPublicKey(messageID, bi, true)
		if bitTrueKey == nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit true", messageID, byteIndex)
		}
		if bi == 0 {
			log.Printf("looking for data (true) report for advert key %s", util.HexString(bitTrueKey))
		}
		trueAdvertisementID, err := util.HashedBeacon(bitTrueKey)
		// log.Printf("@@@@@@@@@ bit true guess: msg %d byte %d bit %d: %s, adv: %s", messageID, byteIndex, bi, util.HexString(bitTrueKey), trueAdvertisementID)
		if err != nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit true, err: %w", messageID, byteIndex, err)
		}
		bitTrueAdvId[bi] = trueAdvertisementID
		// Construct a public key to guess that the bit has false value.
		_, bitFalseKey := client.dataAdvertPublicKey(messageID, bi, false)
		if bitFalseKey == nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit false", messageID, byteIndex)
		}
		falseAdvertisementID, err := util.HashedBeacon(bitFalseKey)
		// log.Printf("@@@@@@@@@ bit false guess: msg %d byte %d bit %d: %s, adv: %s", messageID, byteIndex, bi, util.HexString(bitFalseKey), falseAdvertisementID)
		if err != nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit false, err: %w", messageID, byteIndex, err)
		}
		bitFalseAdvId[bi] = falseAdvertisementID
	}
	// Circa May 2025, Apple iCloud API changed and won't return position reports for ALL requested public keys anymore.
	// Make independent report requests for every bit's two (guess=true & guess=false) guesses.
	allBitsResp := make(map[string]ReportResponse)
	for _, truePub := range bitTrueAdvId {
		resp, err := client.doReportRequest(ctx, MultiReportRequest{HashedAdvertisedPublicKey: []string{truePub}, Days: lookBackDays})
		if err != nil {
			return nil, fmt.Errorf("failed to execute location report http request: %w", err)
		}
		maps.Copy(allBitsResp, resp.ToMap())
		time.Sleep(time.Duration(10000+rand.IntN(7000)) * time.Millisecond)
	}
	for _, falsePub := range bitFalseAdvId {
		resp, err := client.doReportRequest(ctx, MultiReportRequest{HashedAdvertisedPublicKey: []string{falsePub}, Days: lookBackDays})
		if err != nil {
			return nil, fmt.Errorf("failed to execute location report http request: %w", err)
		}
		maps.Copy(allBitsResp, resp.ToMap())
		time.Sleep(time.Duration(10000+rand.IntN(7000)) * time.Millisecond)
	}
	// Correspond the retrieved reports to each bit guess, and thus recover the byte value.
	resultBits := make([]byte, 0, 8)
	oldestReportMillis := time.Now().UnixMilli()
	latestReportMillis := int64(0)
	bitReport := make([]ReportResponse, 8)
	for i := byteIndex * 8; i < (byteIndex+1)*8; i++ {
		bitIndex := i - byteIndex*8
		// To recover the bit value, check whether the true guess or the false guess comes back with a location report.
		trueRep, trueExists := allBitsResp[bitTrueAdvId[i]]
		falseRep, falseExists := allBitsResp[bitFalseAdvId[i]]
		if !trueExists && !falseExists {
			return nil, fmt.Errorf("no location report retrieved for message ID %d, byte index %d, bit index %d, try again in 10 minutes?", messageID, byteIndex, i)
		}
		// The bit value may change within the tolerated spread and result in both false and true reports.
		// Use the newer report to determine the bit value.
		var useTrue bool
		if trueExists && trueRep.DatePublishedUnixMillis > falseRep.DatePublishedUnixMillis {
			useTrue = true
		}
		if useTrue {
			bitReport[bitIndex] = trueRep
			resultBits = append(resultBits, 1)
			// log.Printf("@@@@@@@@ bit %d is true: %+v", bitIndex, trueRep)
		} else {
			bitReport[bitIndex] = falseRep
			resultBits = append(resultBits, 0)
			// log.Printf("@@@@@@@@ bit %d is false: %+v", bitIndex, falseRep)
		}
		bit := bitReport[bitIndex]
		if bit.DatePublishedUnixMillis < oldestReportMillis {
			oldestReportMillis = bit.DatePublishedUnixMillis
		} else if bit.DatePublishedUnixMillis > latestReportMillis {
			latestReportMillis = bit.DatePublishedUnixMillis
		}
	}
	by := &DataByte{
		Value:           util.ByteFromBits(resultBits),
		ReportTime:      time.UnixMilli(latestReportMillis),
		BitReportSpread: time.Duration(latestReportMillis-oldestReportMillis) * time.Millisecond,
		BitReport:       bitReport,
	}
	// log.Printf("@@@@@@@@@ all bit reports: %+v", by)
	if by.BitReportSpread > maxBitReportSpread {
		return nil, fmt.Errorf("the bit report time spread %v exceeds the tolerance %v, the byte value likely contains outdated bits: %+v", by.BitReportSpread, maxBitReportSpread, by)
	}
	return by, nil
}
