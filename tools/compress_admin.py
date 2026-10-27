"""Regenerate the flash-resident gzip admin page after editing its HTML source."""
from pathlib import Path
import gzip
import re

root = Path(__file__).resolve().parents[1]
source = (root / "src/DeviceAdminPage.h").read_text(encoding="utf-8")
page = re.search(r'R"EDC\((.*)\)EDC";', source, re.S)[1].encode()
packed = gzip.compress(page, compresslevel=9, mtime=0)
header = "#pragma once\n#include <Arduino.h>\n// Generated from DeviceAdminPage.h; do not edit by hand.\nstatic const uint8_t DEVICE_ADMIN_GZIP[] PROGMEM = {\n"
header += ",\n".join(",".join(f"0x{byte:02x}" for byte in packed[i:i + 24]) for i in range(0, len(packed), 24))
header += "\n};\n"
(root / "src/DeviceAdminPageGzip.h").write_text(header, encoding="utf-8")
print(f"Admin page: {len(page)} -> {len(packed)} bytes")
