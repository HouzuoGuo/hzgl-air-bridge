package web

const homeTemplateText = `
<!doctype html>
<html>
  <head>
    <title>hzgl-air-bridge</title>
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  </head>
  <body>
    <h3>Location trace</h3>
    <p>Last updated {{.LastLocation.Timestamp.Format "2006-01-02 15:04:05"}} accurate to {{.LastLocation.AccuracyMetres}} meters</p>
    <div id="map" style="height: 600px; border: 1px solid black;"></div>

    <script>
      let points = [
          {{- range $idx, $pt := .RecentTrail }}
          {{ if $idx }},{{ end }}
          { "lat": {{ $pt.Latitude }}, "lng": {{ $pt.Longitude }}, "time": "{{ $pt.Timestamp.Format "2006-01-02 15:04:05" }}" }
          {{- end }}
      ];

      let center = [{{ .LastLocation.Latitude }}, {{ .LastLocation.Longitude }}];

      function on_loaded() {
          var map = L.map('map').setView(center, 15);
          L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
            maxZoom: 19,
            attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
          }).addTo(map);

          points.forEach((pt) => {
            let popupText = "Latitude: " + pt.lat + ", Longitude: " + pt.lng + "<br>Time: " + pt.time;
            let circle = L.circle([pt.lat, pt.lng], {
                color: 'red',
                radius: 20
            }).addTo(map);
            circle.bindPopup(popupText);
          });
      }

      document.addEventListener('DOMContentLoaded', on_loaded);
    </script>

    <table>
    <thead>
        <tr>
        <th>Time</th>
        <th>Report</th>
        <th>Value</th>
        <th>Bit spread (lower is better)</th>
        </tr>
    </thead>
    <tbody>
      {{range .Records}}
      <tr>
        <td>{{.Time.Format "2006-01-02 15:04:05"}}</td>
        <td>
        {{if .TemperatureC}}Temperature
        {{else if .HumidityPct}}Humidity
        {{else if .PressureHpa}}Pressure
        {{else if .BTDeviceCount}}Nearby BT device count
        {{else if .ManualMessage}}Message
        {{else if .Location}}Location
        {{end}}
        </td>
        <td>
          {{if .TemperatureC}}
            {{printf "%.2f°C" .TemperatureC}}
          {{else if .HumidityPct}}
            {{printf "%.2f%%" .HumidityPct}}
          {{else if .PressureHpa}}
            {{printf "%.2f hPa" .PressureHpa}}
          {{else if .BTDeviceCount}}
            {{printf "%d" .BTDeviceCount}}
          {{else if .ManualMessage}}
            {{printf "%d" .ManualMessage}}
          {{else if .Location}}
            <a href="https://www.google.com/maps?q={{.Location.Latitude}},{{.Location.Longitude}}" target="_blank">
            Lat: {{printf "%.6f" .Location.Latitude}}, Lon: {{printf "%.6f" .Location.Longitude}}
            </a>
          {{end}}
        </td>
        <td>{{.BitSpread}}</td>
      </tr>
      {{end}}
    </tbody>
    </table>
  </body>
</html>
`
