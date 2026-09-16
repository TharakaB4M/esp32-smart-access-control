import json
from pathlib import Path

Import("env")


project_dir = Path(env["PROJECT_DIR"])
env_file = project_dir / ".env"
header_file = project_dir / "include" / "secrets.h"


if not env_file.is_file():
    raise RuntimeError("Missing .env. Copy .env.example to .env and set your Wi-Fi values.")


values = {}
for line_number, raw_line in enumerate(env_file.read_text(encoding="utf-8").splitlines(), 1):
    line = raw_line.strip()
    if not line or line.startswith("#"):
        continue
    if "=" not in line:
        raise RuntimeError(f"Invalid .env entry on line {line_number}: expected KEY=VALUE")
    key, value = line.split("=", 1)
    values[key.strip()] = value.strip().strip("\"'")


required_keys = ("WIFI_SSID", "WIFI_PASSWORD", "WIFI_AP_NAME", "WIFI_AP_PASSWORD")
missing_keys = [key for key in required_keys if not values.get(key)]
if missing_keys:
    raise RuntimeError("Missing values in .env: " + ", ".join(missing_keys))
if len(values["WIFI_AP_PASSWORD"]) < 8:
    raise RuntimeError("WIFI_AP_PASSWORD must contain at least 8 characters for ESP32 SoftAP.")


header = "// Generated from .env. Do not edit or commit this file.\n#pragma once\n\n"
for key in required_keys:
    header += f"#define {key} {json.dumps(values[key], ensure_ascii=False)}\n"

header_file.write_text(header, encoding="utf-8")