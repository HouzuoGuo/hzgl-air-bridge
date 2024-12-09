# hzgl-air-bridge

WIP: a telemetry data system based on OpenHaystack.

The work derives from:

- https://github.com/seemoo-lab/openhaystack (AGPL-3.0)
- https://github.com/positive-security/send-my (AGPL-3.0)
- https://github.com/dchristl/macless-haystack (GPL-3.0)

## Instructions

Delete all persisted files (e.g. Apple credentials) and start over:

``` bash
docker ps -aq | xargs docker rm -f
docker volume rm -f anisette-v3_data
docker volume rm -f mh_data
docker network remove mh-network
```

Now start the two web servers:

```bash
docker network create hzgl-air-bridge-network
docker run -d --restart always --name anisette -p 6969:6969 --volume anisette-v3_data:/home/Alcoholic/.config/anisette-v3/lib/ --network hzgl-air-bridge-network dadoum/anisette-v3-server
# Username and password input requires an interactive terminal.
docker run -it --restart unless-stopped --name macless-haystack -p 6176:6176 --volume mh_data:/app/data --network hzgl-air-bridge-network hzgl/air-bridge-ws
```
