# hzgl-air-bridge

Defeat air-gapped systems by exfiltrating data using Apple Find My network.

The project comprises:

- A custom beacon firmware for ESP32-C3 to beacon ambient condition data and location reports.
- Web servers for reading reports without having to own an Apple device.
- A lightweight web app for storing and reading the recent reports.

## Credits & prior research

This project derives from:

- https://github.com/seemoo-lab/openhaystack (AGPL-3.0)
  - With kudos to its FindMy network protocol research and beacon firmware code.
- https://github.com/positive-security/send-my (AGPL-3.0)
  - With kudos to its FindMy network protocol research (https://positive.security/blog/send-my) and beacon firmware code.
- https://github.com/dchristl/macless-haystack (GPL-3.0)
  - With kudos to its location retrieval web service and elliptic curve decryption implementation.

## Usage instructions

### 1. Apple account preparation

Instead of using your own main Apple account, please prepare a new separate Apple account.

When registering the new account, make sure to use SMS text for two factor authentication.

Apple internally maintains a "trust status" for each account, try log into the new account on a genuine Apple device help improve its trust level.

### 2. Generate keys for location reporting

Navigate to `provision/` and run:

``` bash
> ./genkey.py
Using python3
Output will be written to output/
```

### 2. Edit parameters for telemetry data reporting

Navigate to `firmware/src/`, copy `custom.h.example` and rename into custom.h.

The file contains parameters for the firmware to beacon both location and data. Follow in-file instructions to change the parameters. The keys file line "hashed adv key" is not used anywhere.

### 3. Upload beacon firmware

If you wish to transmit ambient condition readings, wire up a BME280 sensor to the I2C bus. Put a 0.96 inch display on the I2C bus too as you like.

Without BME280 the device will beacon reading value 0 for ambient conditions, and continue beaconing location reports.

<img src="https://raw.githubusercontent.com/HouzuoGuo/hzgl-air-bridge/refs/heads/master/prototype.png" width="627" height="320" />

In Visual Studio Code, install PlatformIO plugin and then open directory `firmwrae/`.

Change the serial port in `firmwrae/platformio.ini` to that belongs to your development board, and then execute `PlatformIO: Upload` action.

### 4. Launch server infrastructure

Retrieving Find My location and data reports requires these server programs:

```bash
docker network create hzgl-air-bridge-network
docker run -d --restart always --name anisette -p 6969:6969 --volume anisette-data:/home/Alcoholic/.config/anisette-v3/ --network hzgl-air-bridge-network hzgl/anisette-v3-server
# Username and password input requires an interactive terminal.
# See https://github.com/HouzuoGuo/macless-haystack for the image build instructions.
docker run -it --restart always --name macless-haystack -p 6176:6176 --volume bridge-data:/app/data/ --network hzgl-air-bridge-network hzgl/air-bridge-ws
```

`hzgl/air-bridge-ws` will walk you through Apple account login and registers a fake macbook device under your Apple account.

If you encounter a rare failure "Enter the correct password for this Apple Account" after entering 2FA code, visit https://appleid.apple.com/ and proceed to login with user name and password,
stop at 2FA and close the web page, then restart the `macless-haystack` container and repeat the login, it should succeed in the new attempt.

#### To start over

If you have to start over (e.g. changing to a different Apple account, or re-register the fake macbook), first remove the existing fake macbook from the Apple account, and then:

```bash
docker ps -aq | xargs docker rm -f
docker volume rm -f anisette-data
docker volume rm -f bridge-data
docker network remove hzgl-air-bridge-network
docker pull hzgl/air-bridge-ws
```

The procedure deletes all stored apple ID and fake macbook device credentials.

#### 5. Read data and location reports

To retrieve the latest data & location reports, navigate to `websvc/` and run:

``` bash
go run main.go \
  # Common reporting parameters.
  -days=1 \ # Retrieve data and location up to this many days old, keep it low (<3) to reduce unnecessary API calls to Apple.
  -spreadmins=20 \ # Tolerate this many minutes of spread between individual bites when re-assembling whole data bytes, 20 minutes is good for most cases, any shorter may prevent byte data recovery.
  -airbridgews=http://localhost:6176 \ # The URL of hzgl/air-bridge-ws container web server.

  # Location reporting parameters.
  -locprivkey='the private key found in output/some.keys file' \
  -locadvertkey='the advertisement key found in output/some.keys file' \

  # Data reporting parameters.
  -custommagic=... \ # The base64-encoded value of const uint8_t custom_magic_key.

  # Save all reports to this file.
  -file=/tmp/hzgl-air-bridge.json
```

Leave it running, and it will download location and data reports at a regular interval.

A built-in web server provides a lightweight frontend to view the reports:

``` bash
...
  # Save all reports to this file.
  -file=/tmp/hzgl-air-bridge.json \
  -webaddress=localhost \
  -webport=34681 \
  -webhandler=/air-bridge
```

<img src="https://raw.githubusercontent.com/HouzuoGuo/hzgl-air-bridge/refs/heads/master/web-demo.png" width="415" height="435" />

#### 6. Routine maintenance

Every couple of weeks, repeat the `To start over` and `Lauch server infrastructure` procedure to delete and re-register the fake macbook device.
Failure to do so will occasionally result in Apple account ban.

## News

- 20251001 - the SMS 2FA authentication process seems broken, I'll keep an eye on the upstream bug reports to look for a solution.
- 20260117 - fixed SMS 2FA with kudos to upstream project discussions, added workaround for occasional login failure after 2FA.

## License

This project derives from AGPLv3 (openhaystack & send-my) and GPLv3 (macless-haystack) licensed code, the project is therefore licensed under AGPLv3.
