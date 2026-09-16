# esp32-smart-access-control

## Local Wi-Fi configuration

Wi-Fi credentials are loaded from a local `.env` file during the PlatformIO build. The file is ignored by Git and must never be committed.

1. Copy `.env.example` to `.env`.
2. Set `WIFI_SSID` and `WIFI_PASSWORD` to your network credentials.
3. Set a private `WIFI_AP_PASSWORD` with at least 8 characters for the fallback access point.
4. Build or upload with PlatformIO.

The build generates `include/secrets.h` locally. Both the `.env` file and generated header are ignored. If the old Wi-Fi password was already pushed to GitHub, change that Wi-Fi password because removing it from the latest commit does not remove it from Git history.