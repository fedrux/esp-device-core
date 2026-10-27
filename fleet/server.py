#!/usr/bin/env python3
"""Read-only private device inventory. Default: loopback, no third-party packages."""
import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import os
from pathlib import Path
import socket
import threading
import time
import urllib.error
import urllib.request
from urllib.parse import urlsplit

HERE = Path(__file__).resolve().parent
MAX_RESPONSE = 128 * 1024


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args):
        return None


def validate_url(url):
    parsed = urlsplit(url)
    if parsed.scheme != "http" or not parsed.hostname or parsed.username or parsed.password:
        raise ValueError("Device URL must be an HTTP LAN URL without credentials")
    if parsed.path not in ("", "/") or parsed.query or parsed.fragment:
        raise ValueError("Use the device base URL without path or query")
    addresses = socket.getaddrinfo(parsed.hostname, parsed.port or 80, type=socket.SOCK_STREAM)
    # Permit RFC1918 LAN and loopback only, including local .local names after resolution.
    networks = [ipaddress.ip_network(n) for n in ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "127.0.0.0/8", "::1/128", "fc00::/7", "fe80::/10")]
    if not addresses or any(not any(ipaddress.ip_address(a[4][0].split('%')[0]) in n for n in networks) for a in addresses):
        raise ValueError("Device address is outside the LAN")
    return parsed


class Fleet:
    def __init__(self, config_path, state_path):
        self.config_path, self.state_path = Path(config_path), Path(state_path)
        self.lock = threading.Lock()
        self.cached_at, self.cached = 0, None
        try:
            self.history = json.loads(self.state_path.read_text(encoding="utf-8"))
            if not isinstance(self.history, dict):
                self.history = {}
        except (OSError, ValueError):
            self.history = {}

    def probe(self, device):
        result = {key: device.get(key) for key in ("id", "name", "role", "url", "adapter")}
        previous = self.history.get(device["id"], {})
        if not isinstance(previous, dict):
            previous = {}
        result.update(reachable=None, data_current=None, last_seen=previous.get("last_seen"),
                      firmware_version=previous.get("firmware_version"), core_version=None, error=None)
        telemetry_keys = device.get("telemetry_keys", [])
        if telemetry_keys:
            result["telemetry"] = {key: None for key in telemetry_keys}
        if not device.get("url"):
            result["error"] = "Indirizzo da configurare: stato non monitorato"
            return result
        try:
            validate_url(device["url"])
            adapter = device.get("adapter", "core")
            endpoint = {"core": "/admin/api/status", "json": "/api/status", "http": "/"}[adapter]
            headers = {"Accept": "application/json", "User-Agent": "ESP-Device-Fleet/1.1"}
            if adapter == "core":
                user = os.environ.get(device.get("user_env", ""), "")
                password = os.environ.get(device.get("password_env", ""), "")
                if not user or not password:
                    raise ValueError("Credenziali di lettura non configurate sul PC")
                headers["Authorization"] = "Basic " + base64.b64encode(f"{user}:{password}".encode()).decode()
            # Explicitly bypass environment proxies for private LAN polling.
            opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
            request = urllib.request.Request(device["url"].rstrip("/") + endpoint, headers=headers)
            with opener.open(request, timeout=3) as response:
                raw = response.read(MAX_RESPONSE + 1)
                if len(raw) > MAX_RESPONSE:
                    raise ValueError("Risposta troppo grande")
                result["reachable"] = True
                if adapter != "http":
                    data = json.loads(raw)
                    if not isinstance(data, dict):
                        raise ValueError("Risposta del dispositivo non valida")
                    for key, expected in device.get("expected", {}).items():
                        if data.get(key) != expected:
                            raise ValueError("Identità del dispositivo inattesa")
                    # Expose an explicit allowlist; never forward CSRF tokens or passwords.
                    for key in ("ip", "wifi_ssid", "uptime_s", "free_heap"):
                        result[key] = data.get(key)
                    result["firmware_version"] = data.get("firmware_version") or data.get("version")
                    result["core_version"] = data.get("core_version")
                    if adapter == "json":
                        current_field = device.get("data_current_field")
                        result["data_current"] = data.get(current_field) is True if current_field else None
                        result["application_status"] = "Dati aggiornati" if result["data_current"] else "Dati da verificare"
                    else:
                        result["application_status"] = data.get("application_status")
                        result["last_error"] = data.get("last_error") or None
                    if telemetry_keys:
                        if device.get("telemetry_path") != "/api/status" or len(telemetry_keys) > 24:
                            raise ValueError("Percorso o campi telemetria non validi")
                        try:
                            telemetry_request = urllib.request.Request(device["url"].rstrip("/") + "/api/status", headers=headers)
                            with opener.open(telemetry_request, timeout=3) as telemetry_response:
                                telemetry_raw = telemetry_response.read(MAX_RESPONSE + 1)
                            if len(telemetry_raw) > MAX_RESPONSE:
                                raise ValueError("Telemetria troppo grande")
                            telemetry = json.loads(telemetry_raw)
                            if not isinstance(telemetry, dict):
                                raise ValueError("Telemetria non valida")
                            result["telemetry"] = {key: telemetry.get(key) for key in telemetry_keys}
                            current_field = device.get("telemetry_current_field")
                            if current_field:
                                result["data_current"] = telemetry.get(current_field) is True
                        except (OSError, ValueError):
                            result["data_current"] = False
                            result["error"] = "Telemetria non disponibile; dispositivo raggiungibile"
                result["last_seen"] = datetime.now(timezone.utc).isoformat()
                result["admin_url"] = device["url"].rstrip("/") + "/admin" if adapter != "http" else None
        except urllib.error.HTTPError as exc:
            result["reachable"] = True
            if device.get("adapter") == "http" and exc.code in (401, 403):
                result["last_seen"] = datetime.now(timezone.utc).isoformat()
                result["application_status"] = f"Server web attivo; accesso protetto (HTTP {exc.code})"
            else:
                result["error"] = "Autenticazione richiesta o rifiutata" if exc.code in (401, 403) else f"Risposta HTTP {exc.code}: stato non verificato"
        except (OSError, ValueError, KeyError, TypeError) as exc:
            result["reachable"] = False if isinstance(exc, OSError) else result["reachable"]
            result["error"] = "Dispositivo non raggiungibile" if isinstance(exc, OSError) else str(exc)
        return result

    def snapshot(self):
        with self.lock:
            if self.cached and time.monotonic() - self.cached_at < 5:
                return self.cached
            config = json.loads(self.config_path.read_text(encoding="utf-8-sig"))
            devices = config["devices"]
            if len(devices) > 32 or len({d["id"] for d in devices}) != len(devices):
                raise ValueError("Inventory must have at most 32 unique device IDs")
            with ThreadPoolExecutor(max_workers=8) as pool:
                results = list(pool.map(self.probe, devices))
            for result in results:
                if result["last_seen"] and result["reachable"]:
                    self.history[result["id"]] = {k: result[k] for k in ("last_seen", "firmware_version")}
            temp = self.state_path.with_suffix(".tmp")
            temp.write_text(json.dumps(self.history, indent=2), encoding="utf-8")
            temp.replace(self.state_path)
            self.cached = {"checked_at": datetime.now(timezone.utc).isoformat(), "devices": results}
            self.cached_at = time.monotonic()
            return self.cached


def make_handler(fleet):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            # Reject DNS rebinding to the loopback service.
            if self.headers.get("Host") not in (f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"):
                self.send_error(403)
                return
            try:
                if self.path in ("/", "/index.html"):
                    data, content_type = (HERE / "index.html").read_bytes(), "text/html; charset=utf-8"
                elif self.path == "/api/devices":
                    data, content_type = json.dumps(fleet.snapshot(), ensure_ascii=False).encode(), "application/json; charset=utf-8"
                elif self.path == "/favicon.ico":
                    self.send_response(204); self.end_headers(); return
                else:
                    self.send_error(404); return
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'")
                self.end_headers(); self.wfile.write(data)
            except (OSError, ValueError, KeyError, TypeError):
                data = b'{"message":"Inventario non disponibile: controlla configurazione e permessi locali"}'
                self.send_response(500); self.send_header("Content-Type", "application/json"); self.end_headers(); self.wfile.write(data)

        def log_message(self, *args):
            pass
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--state", type=Path)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    state = args.state or args.config.with_name("state.json")
    fleet = Fleet(args.config, state)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(fleet))
    print(f"Device inventory: http://127.0.0.1:{args.port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
