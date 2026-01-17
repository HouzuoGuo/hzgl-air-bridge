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
	flag.IntVar(&maxDays, "days", 1, "retrieve data and location up to this many days old, keep it low (<3) to reduce unnecessary API calls to Apple.")
	var maxBitReportSpreadMins int
	flag.IntVar(&maxBitReportSpreadMins, "spreadmins", 20, "tolerate this many minutes of spread between individual bites when re-assembling whole data bytes, 20 minutes is good for most cases, any shorter may prevent byte data recovery.")
	var reportWebAddress string
	flag.StringVar(&reportWebAddress, "airbridgews", "http://localhost:6176/", "url of the hzgl/air-bridge-ws:latest web service")

	// Location reporting parameters.
	var locPrivKey, locAdvertKey string
	flag.StringVar(&locPrivKey, "locprivkey", "", "the base64-encoded private key for decrypting location beacons")
	flag.StringVar(&locAdvertKey, "locadvertkey", "", "the base64-encoded advertisement key for retrieving location beacons, not to be confused with hashed advertisement key!")

	// Data reporting parameters.
	var customMagicKeyBase64 string
	flag.StringVar(&customMagicKeyBase64, "custommagic", "", "base64-encoded string of the custom_magic_key value (24 bytes) defined in the beacon firmware")

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

	if locPrivKey == "" || locAdvertKey == "" || customMagicKeyBase64 == "" || saveFileName == "" {
		flag.PrintDefaults()
		os.Exit(1)
	}

	client := findmy.NewClient(reportWebAddress, util.Base64Decode(locPrivKey), util.Base64Decode(locAdvertKey), util.Base64Decode(customMagicKeyBase64))
	rec, err := recorder.New(saveFileName, client, maxDays, time.Duration(maxBitReportSpreadMins)*time.Minute)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("loaded %d saved reports from %s", len(rec.Records), saveFileName)

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
