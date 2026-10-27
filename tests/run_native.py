#!/usr/bin/env python3
"""Run portable boundary checks and core lifecycle scenarios with fake hardware."""
from pathlib import Path
import subprocess
import tempfile
from test_tools import image, packager

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="esp-core-tests-") as directory:
    temp = Path(directory)
    valid, wrong = temp / "valid.edc", temp / "wrong.edc"
    valid.write_bytes(packager.package(image(board="esp8266")))
    wrong.write_bytes(packager.package(image(board="esp32c3")))
    flags = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I", str(ROOT / "src")]
    protocol, scenarios = temp / "protocol", temp / "scenarios"
    subprocess.run(flags + [str(ROOT / "tests/protocol_test.cpp"), "-o", str(protocol)], check=True)
    subprocess.run([str(protocol), str(wrong)], check=True)
    subprocess.run(flags + ["-DESP8266", "-I", str(ROOT / "tests/fakes"), str(ROOT / "tests/core_scenarios.cpp"), str(ROOT / "src/ESPDeviceCore.cpp"), "-o", str(scenarios)], check=True)
    subprocess.run([str(scenarios), str(valid), str(wrong)], check=True)
