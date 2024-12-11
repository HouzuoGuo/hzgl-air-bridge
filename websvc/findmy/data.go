package findmy

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"io"
	"log"
	"net/http"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/crypt"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/util"
)

func (client *FindMyClient) advertisementPublicKey(bitIndex int, messageID int, guessBit bool) (attempt int, ret []byte) {
	ret = make([]byte, 28)
	copy(ret, client.DataPrefixMagic)
	binary.BigEndian.PutUint32(ret[2:], uint32(bitIndex))
	binary.BigEndian.PutUint32(ret[6:], uint32(messageID))
	copy(ret[10:], client.DataModemID)
	if guessBit {
		ret[27] = 1
	}
	for attempt = 0; attempt < 100; attempt++ {
		binary.BigEndian.PutUint32(ret[14:], uint32(attempt))
		if crypt.IsValidPubkey(ret) {
			return
		}
	}
	return 0, nil
}

func (client *FindMyClient) DownloadByte(messageID int, byteIndex int) byte {
	// Retrieve one byte at a time.
	bitTrueID := make(map[int]string)
	bitFalseID := make(map[int]string)
	var trueAndFalseIds []string
	for bi := byteIndex * 8; bi < (byteIndex+1)*8; bi++ {
		log.Printf("downloading bit %d", bi)
		_, bitTrueKey := client.advertisementPublicKey(bi, messageID, true)
		if bitTrueKey == nil {
			log.Fatalf("key calculation failure byte %d bit %d", byteIndex, bi)
		}
		trueAdvertisementID, err := util.HashedBeacon(bitTrueKey)
		if err != nil {
			log.Fatalf("true key beacon err byte %d bit %d err %v", byteIndex, bi, err)
		}
		bitTrueID[bi] = trueAdvertisementID

		_, bitFalseKey := client.advertisementPublicKey(bi, messageID, false)
		if bitFalseKey == nil {
			log.Fatalf("key calculation failure byte %d bit %d", byteIndex, bi)
		}
		falseAdvertisementID, err := util.HashedBeacon(bitFalseKey)
		if err != nil {
			log.Fatalf("false key beacon err byte %d bit %d err %v", byteIndex, bi, err)
		}
		bitFalseID[bi] = falseAdvertisementID
		trueAndFalseIds = append(trueAndFalseIds, trueAdvertisementID)
		trueAndFalseIds = append(trueAndFalseIds, falseAdvertisementID)
	}
	// Retrieve the preports of all IDs simultaneously.
	reportReq := MultiReportRequest{
		HashedAdvertisementKeyIds: trueAndFalseIds,
		Days:                      1,
	}
	reportReqBody, err := json.Marshal(reportReq)
	if err != nil {
		log.Fatalf("req serialisation err: %v", err)
	}
	httpReq, err := http.NewRequest(http.MethodPost, client.LocationReportHttpAddress, bytes.NewReader(reportReqBody))
	if err != nil {
		log.Fatalf("http req err %v", err)
	}
	httpReq.Header.Set("Content-Type", "application/json")
	var multiReportResp MultiReportResponse
	resp, err := http.DefaultClient.Do(httpReq)
	if err != nil {
		log.Fatalf("http resp err %v", err)
	}
	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Fatalf("http resp read err %v", err)
	}
	err = json.Unmarshal(respBody, &multiReportResp)
	if err != nil {
		log.Fatalf("http resp unmarshal err %v", err)
	}
	// Determine which bits have been seen.
	reportByID := multiReportResp.ToMap()
	resultBits := make([]byte, 0, 8)
	for i := byteIndex * 8; i < (byteIndex+1)*8; i++ {
		if rep, exists := reportByID[bitTrueID[i]]; exists {
			log.Printf("byte %d bit %d is 1: %+v", byteIndex, i, rep)
			resultBits = append(resultBits, 1)
		} else if rep, exists := reportByID[bitFalseID[i]]; exists {
			log.Printf("byte %d bit %d is 0: %+v", byteIndex, i, rep)
			resultBits = append(resultBits, 0)
		} else {
			log.Fatalf("failed to retrieve byte %d bit %d", byteIndex, i)
		}
	}
	return util.BigEndianBitsToByte(resultBits)
}
