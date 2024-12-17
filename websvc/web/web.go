package web

import (
	"html/template"
	"log"
	"net"
	"net/http"
	"slices"
	"sort"
	"strconv"

	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/findmy"
	"github.com/HouzuoGuo/hzgl-air-bridge/websvc/recorder"
)

var tmplFuncs = template.FuncMap{
	"add": func(a, b float64) float64 { return a + b },
}

type HomeData struct {
	Records      []recorder.Record
	RecentTrail  []findmy.DecryptedLocation
	LastLocation findmy.DecryptedLocation
}

type Server struct {
	Address  string
	Port     int
	Handler  string
	mux      *http.ServeMux
	recorder *recorder.Recorder
}

func New(address string, port int, handler string, recorder *recorder.Recorder) (srv *Server) {
	srv = &Server{
		Address:  address,
		Port:     port,
		Handler:  handler,
		mux:      http.NewServeMux(),
		recorder: recorder,
	}
	srv.mux.HandleFunc(handler, func(w http.ResponseWriter, r *http.Request) {
		clonedRecords := slices.Clone(recorder.Records)
		// The table displays newer records first.
		sort.Slice(clonedRecords, func(i, j int) bool {
			return clonedRecords[i].Time.After(clonedRecords[j].Time)
		})
		// Collect the 20 latest location records to plot a trail.
		var recentTrail []findmy.DecryptedLocation
		var lastLocation findmy.DecryptedLocation
		for i := 0; i < len(clonedRecords) && len(recentTrail) < 20; i++ {
			if rec := clonedRecords[i]; rec.Location.Valid() {
				recentTrail = append(recentTrail, rec.Location)
				if !lastLocation.Valid() {
					lastLocation = rec.Location
				}
			}
		}
		// The trail travels from an older waypoint to a newer waypoint.
		sort.Slice(recentTrail, func(i, j int) bool {
			return recentTrail[i].Timestamp.Before(recentTrail[j].Timestamp)
		})
		_ = template.Must(template.New("").Funcs(tmplFuncs).Parse(homeTemplateText)).Execute(w, HomeData{
			Records:      clonedRecords,
			RecentTrail:  recentTrail,
			LastLocation: lastLocation,
		})
	})
	return
}

func (srv *Server) StartAndBlock() error {
	httpServer := http.Server{
		Addr:    net.JoinHostPort(srv.Address, strconv.Itoa(srv.Port)),
		Handler: srv.mux,
	}
	log.Printf("serving lightweight web app on %s:%d", srv.Address, srv.Port)
	return httpServer.ListenAndServe()
}
