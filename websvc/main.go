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

	// Message ID 0 - temperature in celcius
	tempBy, err := client.DownloadDataByte(context.Background(), 0, 0, maxDays, time.Duration(maxBitReportSpreadMins)*time.Minute)
	if err == nil {
		log.Printf("ambient temperature: %f, report: %+v", float64(tempBy.Value)/3+40, tempBy)
	} else {
		log.Printf("failed to downnload temperature byte: %v", err)
	}
	// Message ID 1 - humidity %
	humidityBy, err := client.DownloadDataByte(context.Background(), 1, 0, maxDays, time.Duration(maxBitReportSpreadMins)*time.Minute)
	if err == nil {
		log.Printf("relative humidity: %f%%, report: %+v", float64(humidityBy.Value)/2.55, humidityBy)
	} else {
		log.Printf("failed to downnload humidity byte: %v", err)
	}
	// Message ID 2 - pressure hpa
	pressBy1, err1 := client.DownloadDataByte(context.Background(), 2, 0, maxDays, time.Duration(maxBitReportSpreadMins)*time.Minute)
	pressBy2, err2 := client.DownloadDataByte(context.Background(), 2, 1, maxDays, time.Duration(maxBitReportSpreadMins)*time.Minute)
	if err1 == nil && err2 == nil {
		pressIntVal := util.TwoBeBytes([]byte{pressBy1.Value, pressBy2.Value})
		log.Printf("ambient pressure: %f hpa, report: %+v %+v", float64(pressIntVal)/(65535.0/(1200.0-100.0))+100, pressBy1, pressBy2)
	} else {
		log.Printf("failed to downnload pressure bytes: %v %v", err1, err2)
	}

	// Grab the location reports too.
	loc, err := client.DownloadLocation(context.Background(), maxDays)
	if err != nil {
		log.Fatal(err)
	}
	for _, report := range loc {
		log.Printf("location report: %+v", report)
	}
}
