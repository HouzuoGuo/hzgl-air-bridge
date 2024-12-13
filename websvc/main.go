package main

import (
	"context"
	"flag"
	"log"
	"os"
	"time"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/findmy"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/util"
)

func main() {
	var locPrivKey, locAdvertKey string

	flag.StringVar(&locPrivKey, "locprivkey", "", "the base64-encoded private key for decrypting location beacons")
	flag.StringVar(&locAdvertKey, "locadvertkey", "", "the base64-encoded advertisement key for retrieving location beacons, not to be confused with hashed advertisement key!")

	var dataPrefixMagic, dataModemId string
	flag.StringVar(&dataPrefixMagic, "dataprefix", "", "the base64-encoded 2 prefix bytes for telemetry data transfer")
	flag.StringVar(&dataModemId, "datamodem", "", "the base64-encoded 4 data modem ID bytes for telemetry data transfer")

	var reportWebAddress string
	flag.StringVar(&reportWebAddress, "reportaddr", "http://localhost:6176/", "url of the web service served by docker image hzgl/air-bridge-ws:latest")

	var maxDays int
	flag.IntVar(&maxDays, "days", 1, "query this many days of location reports in search of locations and data bytes")
	var maxBitReportSpreadMins int
	flag.IntVar(&maxBitReportSpreadMins, "spreadmins", 30, "tolerate this many minutes of uncertainty when retrieving data bytes")

	flag.Parse()

	if locPrivKey == "" || locAdvertKey == "" || dataPrefixMagic == "" || dataModemId == "" {
		flag.PrintDefaults()
		os.Exit(1)
	}

	client := findmy.NewClient(reportWebAddress, util.Base64Decode(locPrivKey), util.Base64Decode(locAdvertKey), util.Base64Decode(dataPrefixMagic), util.Base64Decode(dataModemId))

	data := make([]*findmy.DataByte, 0, 4)
	for i := 0; i < 4; i++ {
		by, err := client.DownloadDataByte(context.Background(), 0, i, maxDays, time.Duration(maxBitReportSpreadMins)*time.Minute)
		if err != nil {
			log.Fatal(err)
		}
		data = append(data, by)
	}

	for i, d := range data {
		log.Printf("data byte %d: %c %+v", i, d.Value, d)
	}

	loc, err := client.DownloadLocation(context.Background(), maxDays)
	if err != nil {
		log.Fatal(err)
	}
	for _, report := range loc {
		log.Printf("location report: %+v", report)
	}
}
