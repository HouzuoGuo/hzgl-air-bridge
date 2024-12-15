# hzgl-air-bridge

WIP: an infrastructure for collecting telemetry data from air-gapped system using Apple Find My network as data carrier.

## Credits & prior research

This project derives from:

- https://github.com/seemoo-lab/openhaystack (AGPL-3.0)
  - With kudos to its FindMy network protocol research and beacon firmware code.
- https://github.com/positive-security/send-my (AGPL-3.0)
  - With kudos to its FindMy network protocol research and beacon firmware code.
- https://github.com/dchristl/macless-haystack (GPL-3.0)
  - With kudos to its location retrieval web service and elliptic curve decryption implementation.

## Usage instructions

### Generate keys for location reporting

TODO

### Edit parameters for telemetry data reporting

TODO

### Upload beacon firmware

TODO

### Launch server infrastructure

TODO

Delete all persisted files (e.g. Apple credentials) and start over:

```bash
docker ps -aq | xargs docker rm -f
docker volume rm -f anisette-data
docker volume rm -f bridge-data
docker network remove hzgl-air-bridge-network
```

Now start the two web servers:

```bash
docker network create hzgl-air-bridge-network
docker run -d --restart always --name anisette -p 6969:6969 --volume anisette-data:/home/Alcoholic/.config/anisette-v3/ --network hzgl-air-bridge-network hzgl/anisette-v3-server
# Username and password input requires an interactive terminal.
# See https://github.com/HouzuoGuo/macless-haystack for the image build instructions.
docker run -it --restart unless-stopped --name macless-haystack -p 6176:6176 --volume bridge-data:/app/data/ --network hzgl-air-bridge-network hzgl/air-bridge-ws
```

## License

This project derives from AGPLv3 (openhaystack & send-my) and GPLv3 (macless-haystack, see also https://github.com/dchristl/macless-haystack/issues/147) licensed code, the project is therefore licensed under AGPLv3.
