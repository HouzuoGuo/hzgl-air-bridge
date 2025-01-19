package web

const homeTemplateText = `
<!doctype html>
<html>
  <head>
    <title>hzgl-air-bridge</title>
  </head>
  <body>
    <h3>Location</h3>
    <p>Last updated {{.LastLocation.Timestamp.Format "2006-01-02 15:04:05"}} accurate to {{.LastLocation.AccuracyMetres}} meters</p>
    <iframe width="480" height="320"
      frameborder="0" scrolling="no"
      marginheight="0" marginwidth="0"
      src="https://www.openstreetmap.org/export/embed.html?bbox={{(index .RecentTrail 0).Longitude | add -0.01}},{{(index .RecentTrail 0).Latitude | add -0.01}},{{(index .RecentTrail 0).Longitude | add 0.01}},{{(index .RecentTrail 0).Latitude | add 0.01}}&layer=mapnik{{range .RecentTrail}}&marker={{.Latitude}}%2C{{.Longitude}}{{end}}"
      style="border: 1px solid black">
    </iframe>

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
