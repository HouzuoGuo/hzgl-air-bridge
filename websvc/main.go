package main

import (
	"flag"
	"log"
	"os"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/findmy"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/util"
)

func main() {
	var locPrivKey, locAdvKey string

	flag.StringVar(&locPrivKey, "locprivkey", "", "the base64-encoded private key for decrypting location beacons")
	flag.StringVar(&locAdvKey, "locadvkey", "", "the base64-encoded advertisement key for retrieving location beacons, not to be confused with hashed advertisement key!")

	var dataPrefixMagic, dataModemId string
	flag.StringVar(&dataPrefixMagic, "dataprefix", "", "the base64-encoded 2 prefix bytes for telemetry data transfer")
	flag.StringVar(&dataModemId, "datamodem", "", "the base64-encoded 4 data modem ID bytes for telemetry data transfer")

	var reportWebAddress string
	flag.StringVar(&reportWebAddress, "reportaddr", "http://localhost:6176/", "url of the web service served by docker image hzgl/air-bridge-ws:latest")
	flag.Parse()

	if locPrivKey == "" || locAdvKey == "" || dataPrefixMagic == "" || dataModemId == "" {
		flag.PrintDefaults()
		os.Exit(1)
	}

	client := findmy.FindMyClient{
		LocationReportHttpAddress: reportWebAddress,
		LocationPrivateKey:        util.Base64Decode(locPrivKey),
		LocationAdvertisementKey:  util.Base64Decode(locAdvKey),
		DataPrefixMagic:           util.Base64Decode(dataPrefixMagic),
		DataModemID:               util.Base64Decode(dataModemId),
	}

	data := make([]byte, 0, 4)
	for i := 0; i < 4; i++ {
		by := client.DownloadByte(0, i)
		data = append(data, by)
	}

	for i, d := range data {
		log.Printf("data byte %d: %c", i, d)
	}
}
