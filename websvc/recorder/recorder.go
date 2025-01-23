package recorder

import (
	"context"
	"encoding/json"
	"errors"
	"log"
	"math/rand"
	"os"
	"sort"
	"time"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/findmy"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/util"
)

type Record struct {
	TemperatureC  float64                  `json:"temp_c,omitempty"`
	HumidityPct   float64                  `json:"humidity_pct,omitempty"`
	PressureHpa   float64                  `json:"pressure_hpa,omitempty"`
	BTDeviceCount int                      `json:"bt_device_count,omitempty"`
	ManualMessage int                      `json:"manual_message,omitempty"`
	Time          time.Time                `json:"time,omitempty"`
	BitSpread     time.Duration            `json:"bit_spread,omitempty"`
	Location      findmy.DecryptedLocation `json:"location,omitempty"`
}

type Recorder struct {
	Records            []Record      `json:"records"`
	FileName           string        `json:"-"`
	MaxDays            int           `json:"-"`
	MaxBitReportSpread time.Duration `json:"-"`

	client            *findmy.Client
	lastTemp          time.Time
	lastHumidity      time.Time
	lastPressure      time.Time
	lastLocation      time.Time
	lastBTDeviceCount time.Time
	lastManualMessage time.Time
}

func New(fileName string, client *findmy.Client, maxDays int, maxBitReportSpread time.Duration) (rec *Recorder, err error) {
	rec = &Recorder{
		FileName:           fileName,
		MaxDays:            maxDays,
		MaxBitReportSpread: maxBitReportSpread,
		client:             client,
	}
	content, err := os.ReadFile(fileName)
	if err == nil {
		if err = json.Unmarshal(content, &rec); err != nil {
			return
		}
	} else if errors.Is(err, os.ErrNotExist) {
		err = nil
	} else {
		return
	}
	for _, rep := range rec.Records {
		if rep.TemperatureC != 0 && rep.Time.After(rec.lastTemp) {
			rec.lastTemp = rep.Time
		}
		if rep.PressureHpa != 0 && rep.Time.After(rec.lastPressure) {
			rec.lastPressure = rep.Time
		}
		if rep.HumidityPct != 0 && rep.Time.After(rec.lastHumidity) {
			rec.lastHumidity = rep.Time
		}
		if rep.Location.Valid() && rep.Time.After(rec.lastLocation) {
			rec.lastLocation = rep.Time
		}
		if rep.BTDeviceCount != 0 && rep.Time.After(rec.lastBTDeviceCount) {
			rec.lastBTDeviceCount = rep.Time
		}
		if rep.ManualMessage != 0 && rep.Time.After(rec.lastManualMessage) {
			rec.lastManualMessage = rep.Time
		}
	}
	return
}

func (rec *Recorder) downloadTemp() {
	// Message ID 0 - temperature in celcius
	tempBy, err := rec.client.DownloadDataByte(context.Background(), 0, 0, rec.MaxDays, rec.MaxBitReportSpread)
	if err != nil || !tempBy.ReportTime.After(rec.lastTemp) {
		return
	}
	val := float64(tempBy.Value)/3 - 40
	log.Printf("ambient temperature: %f, report: %+v", val, tempBy)
	rec.Records = append(rec.Records, Record{
		TemperatureC: val,
		Time:         tempBy.ReportTime,
		BitSpread:    tempBy.BitReportSpread,
	})
	rec.lastTemp = tempBy.ReportTime
}

func (rec *Recorder) downloadHumidity() {
	// Message ID 1 - humidity %
	humidityBy, err := rec.client.DownloadDataByte(context.Background(), 1, 0, rec.MaxDays, rec.MaxBitReportSpread)
	if err != nil || !humidityBy.ReportTime.After(rec.lastHumidity) {
		return
	}
	val := float64(humidityBy.Value) / 2.55
	log.Printf("relative humidity: %f%%, report: %+v", val, humidityBy)
	rec.Records = append(rec.Records, Record{
		HumidityPct: val,
		Time:        humidityBy.ReportTime,
		BitSpread:   humidityBy.BitReportSpread,
	})
	rec.lastHumidity = humidityBy.ReportTime
}

func (rec *Recorder) downloadPressure() {
	// Message ID 2 - pressure hpa
	pressBy1, err1 := rec.client.DownloadDataByte(context.Background(), 2, 0, rec.MaxDays, rec.MaxBitReportSpread)
	pressBy2, err2 := rec.client.DownloadDataByte(context.Background(), 2, 1, rec.MaxDays, rec.MaxBitReportSpread)
	if err1 != nil || err2 != nil || !pressBy1.ReportTime.After(rec.lastPressure) || !pressBy2.ReportTime.After(rec.lastPressure) {
		return
	}
	pressIntVal := util.TwoBeBytes([]byte{pressBy1.Value, pressBy2.Value})
	val := float64(pressIntVal)/(65535.0/(1200.0-100.0)) + 100
	log.Printf("ambient pressure: %f hpa, report: %+v %+v", val, pressBy1, pressBy2)

	greaterSpread := pressBy1.BitReportSpread
	if pressBy2.BitReportSpread > greaterSpread {
		greaterSpread = pressBy2.BitReportSpread
	}

	rec.lastPressure = pressBy1.ReportTime
	if pressBy2.ReportTime.After(pressBy1.ReportTime) {
		rec.lastPressure = pressBy2.ReportTime
	}

	rec.Records = append(rec.Records, Record{
		PressureHpa: val,
		Time:        rec.lastPressure,
		BitSpread:   greaterSpread,
	})
}

func (rec *Recorder) downloadBTDeviceCount() {
	// Message ID 4 - count of nearby bluetooth devices
	countBy, err := rec.client.DownloadDataByte(context.Background(), 4, 0, rec.MaxDays, rec.MaxBitReportSpread)
	if err != nil || !countBy.ReportTime.After(rec.lastBTDeviceCount) {
		return
	}
	log.Printf("bluetooth device count %d, report: %+v", countBy.Value, countBy)
	rec.Records = append(rec.Records, Record{
		BTDeviceCount: int(countBy.Value),
		Time:          countBy.ReportTime,
		BitSpread:     countBy.BitReportSpread,
	})
	rec.lastBTDeviceCount = countBy.ReportTime
}

func (rec *Recorder) downloadManualInputMessage() {
	// Message ID 5 - manual message input
	messageBy, err := rec.client.DownloadDataByte(context.Background(), 5, 0, rec.MaxDays, rec.MaxBitReportSpread)
	if err != nil || !messageBy.ReportTime.After(rec.lastManualMessage) {
		return
	}
	log.Printf("manual message input %d, report: %+v", messageBy.Value, messageBy)
	rec.Records = append(rec.Records, Record{
		ManualMessage: int(messageBy.Value),
		Time:          messageBy.ReportTime,
		BitSpread:     messageBy.BitReportSpread,
	})
	rec.lastManualMessage = messageBy.ReportTime
}

func (rec *Recorder) downloadLocation() {
	loc, err := rec.client.DownloadLocation(context.Background(), rec.MaxDays)
	if err != nil {
		return
	}
	// Sort from oldest to newest to store all newer location records.
	sort.Slice(loc, func(a, b int) bool {
		return loc[a].Timestamp.Before(loc[b].Timestamp)
	})
	for _, rep := range loc {
		if rep.Timestamp.After(rec.lastLocation) {
			rec.Records = append(rec.Records, Record{
				Time:     rep.Timestamp,
				Location: rep,
			})
			log.Printf("location: %+v", rep)
			rec.lastLocation = rep.Timestamp
		}
	}
}

func (rec *Recorder) StartAndBlock() error {
	for i := 0; ; i++ {
		duration := time.Duration(3*60+rand.Intn(2*60)) * time.Second
		log.Printf("sleeping %v before reading the next report at round #%d", duration, i)
		time.Sleep(duration)
		// The manual input message has greater importance than the environment sensor telemetry.
		// Make more attempts at downloading the manual input message.
		switch i % 7 {
		case 0:
			rec.downloadLocation()
		case 1:
			rec.downloadTemp()
		case 2:
			rec.downloadHumidity()
		case 3:
			rec.downloadManualInputMessage()
		case 4:
			rec.downloadPressure()
		case 5:
			rec.downloadBTDeviceCount()
		case 6:
			rec.downloadManualInputMessage()
		}
		content, err := json.Marshal(*rec)
		if err != nil {
			return err
		}
		if err := os.WriteFile(rec.FileName, content, 0644); err != nil {
			return err
		}
	}
}
