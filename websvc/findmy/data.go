package findmy

import (
	"context"
	"encoding/binary"
	"fmt"
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
func (client *Client) dataAdvertPublicKey(messageID int, guessBitIndex int, guessBitValue bool) (attempt int, ret []byte) {
	// See firmware/src/main.cpp for the technique of encoding data in the public key portion of the beacon payload.
	ret = make([]byte, 28)
	ret[0] = byte(client.PubkeyMagic1)
	ret[1] = byte(client.PubkeyMagic2)
	binary.BigEndian.PutUint32(ret[2:], uint32(guessBitIndex))
	binary.BigEndian.PutUint32(ret[6:], uint32(messageID))
	binary.BigEndian.PutUint32(ret[10:], uint32(client.ModemID))
	if guessBitValue {
		ret[27] = 1
	}
	// Some public keys constructed by the template above are invalid. Manipulate the attempt number bytes to find a valid public key.
	for attempt = 0; attempt < 500; attempt++ {
		binary.BigEndian.PutUint32(ret[14:], uint32(attempt))
		if util.IsValidPubkey(ret) {
			return
		}
	}
	return 0, nil
}

// DownloadDataByte retrieves a data byte for the specified message ID and byte index.
// maxBitReportSpread defines the maximum time between updates of the byte. It prevents incidental retrieval of outdated bits.
func (client *Client) DownloadDataByte(ctx context.Context, messageID, byteIndex, lookBackDays int, maxBitReportSpread time.Duration) (*DataByte, error) {
	bitTrueID := make(map[int]string)
	bitFalseID := make(map[int]string)
	var trueAndFalseIds []string
	for bi := byteIndex * 8; bi < (byteIndex+1)*8; bi++ {
		// Construct a public key to guess that the bit has true value.
		_, bitTrueKey := client.dataAdvertPublicKey(messageID, bi, true)
		if bitTrueKey == nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit true", messageID, byteIndex)
		}
		trueAdvertisementID, err := util.HashedBeacon(bitTrueKey)
		if err != nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit true, err: %w", messageID, byteIndex, err)
		}
		bitTrueID[bi] = trueAdvertisementID
		// Construct a public key to guess that the bit has false value.
		_, bitFalseKey := client.dataAdvertPublicKey(messageID, bi, false)
		if bitFalseKey == nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit false", messageID, byteIndex)
		}
		falseAdvertisementID, err := util.HashedBeacon(bitFalseKey)
		if err != nil {
			return nil, fmt.Errorf("failed to calculate the advertisement public key for message ID %d, byte index %d, bit false, err: %w", messageID, byteIndex, err)
		}
		bitFalseID[bi] = falseAdvertisementID
		trueAndFalseIds = append(trueAndFalseIds, trueAdvertisementID)
		trueAndFalseIds = append(trueAndFalseIds, falseAdvertisementID)
	}
	// Request location reports from Find My network for all the true & false guesses.
	multiReportResp, err := client.doReportRequest(ctx, MultiReportRequest{HashedAdvertisedPublicKey: trueAndFalseIds, Days: lookBackDays})
	if err != nil {
		return nil, fmt.Errorf("failed to execute location report http request: %w", err)
	}
	// Correspond the retrieved reports to each bit guess, and thus recover the byte value.
	reportByID := multiReportResp.ToMap()
	resultBits := make([]byte, 0, 8)
	oldestReportMillis := time.Now().UnixMilli()
	latestReportMillis := int64(0)
	bitReport := make([]ReportResponse, 8)
	for i := byteIndex * 8; i < (byteIndex+1)*8; i++ {
		// To recover the bit value, check whether the true guess or the false guess comes back with a location report.
		trueRep, trueExists := reportByID[bitTrueID[i]]
		falseRep, falseExists := reportByID[bitFalseID[i]]
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
			bitReport[i-byteIndex*8] = trueRep
			resultBits = append(resultBits, 1)
		} else {
			bitReport[i-byteIndex*8] = falseRep
			resultBits = append(resultBits, 0)
		}
		bit := bitReport[i-byteIndex*8]
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
	if by.BitReportSpread > maxBitReportSpread {
		return nil, fmt.Errorf("the bit report time spread %v exceeds the tolerance %v, the byte value likely contains outdated bits: %+v", by.BitReportSpread, maxBitReportSpread, by)
	}
	return by, nil
}
