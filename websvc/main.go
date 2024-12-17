package main

import (
	"flag"
	"log"
	"os"
	"time"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/findmy"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/recorder"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/util"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/web"
)

func main() {
	// Common reporting parameters.
	var maxDays int
	flag.IntVar(&maxDays, "days", 1, "query this many days of location reports in search of locations and data bytes")
	var maxBitReportSpreadMins int
	flag.IntVar(&maxBitReportSpreadMins, "spreadmins", 30, "tolerate this many minutes of uncertainty when retrieving data bytes")
	var reportWebAddress string
	flag.StringVar(&reportWebAddress, "airbridgews", "http://localhost:6176/", "url of the hzgl/air-bridge-ws:latest web service")

	// Location reporting parameters.
	var locPrivKey, locAdvertKey string
	flag.StringVar(&locPrivKey, "locprivkey", "", "the base64-encoded private key for decrypting location beacons")
	flag.StringVar(&locAdvertKey, "locadvertkey", "", "the base64-encoded advertisement key for retrieving location beacons, not to be confused with hashed advertisement key!")

	// Data reporting parameters.
	var pubkeyMagic1, pubkeyMagic2, modemID int
	flag.IntVar(&pubkeyMagic1, "pubkey1", 0, "the custom_pubkey_magic1 from custom.h, converted from hex to decimal.")
	flag.IntVar(&pubkeyMagic2, "pubkey2", 0, "the custom_pubkey_magic2 from custom.h, converted from hex to decimal.")
	flag.IntVar(&modemID, "modemid", 0, "the custom_modem_id from custom.h, converted from hex to decimal.")

	// Persistence & web frontend parameters.
	var saveFileName string
	flag.StringVar(&saveFileName, "file", "/tmp/hzgl-air-bridge.json", "save retrieved data and location reports to this json file")
	var webAddress string
	var webPort int
	var webHandler string
	flag.StringVar(&webAddress, "webaddress", "localhost", "run web frontend server on this address")
	flag.IntVar(&webPort, "webport", 34581, "run web frontend server on this port number")
	flag.StringVar(&webHandler, "webhandler", "/air-bridge", "serve the html app on this handler path")

	flag.Parse()

	if locPrivKey == "" || locAdvertKey == "" || pubkeyMagic1 < 1 || pubkeyMagic2 < 1 || modemID < 1 || saveFileName == "" {
		flag.PrintDefaults()
		os.Exit(1)
	}

	client := findmy.NewClient(reportWebAddress, util.Base64Decode(locPrivKey), util.Base64Decode(locAdvertKey), pubkeyMagic1, pubkeyMagic2, modemID)
	rec, err := recorder.New(saveFileName, client, maxDays, time.Duration(maxBitReportSpreadMins)*time.Minute)
	if err != nil {
		log.Fatal(err)
	}

	if webPort > 0 && webAddress != "" && webHandler != "" {
		go func() {
			webSrv := web.New(webAddress, webPort, webHandler, rec)
			log.Fatal(webSrv.StartAndBlock())
		}()
	}

	if err := rec.StartAndBlock(); err != nil {
		log.Fatal(err)
	}
}
