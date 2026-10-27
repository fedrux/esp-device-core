"""Local UI fixture, never a device deployment. Runs on 127.0.0.1:8766."""
from copy import deepcopy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
from urllib.parse import parse_qs

ROOT = Path(__file__).resolve().parents[1]
STATUS = {
    "device": "Dispositivo di prova", "core_version": "1.1.0", "board": "esp8266",
    "application_id": "basic-device", "firmware_version": "1.1.0",
    "status": "Wi-Fi collegato", "application_status": 'Sensore pronto: "lettura valida"',
    "wifi_connected": True, "wifi_ssid": "Rete di prova", "ip": "127.0.0.1:8766", "wifi_rssi": -56,
    "uptime_s": 3605, "free_heap": 30000, "reset_reason": "Accensione", "disconnects": 1,
    "connection_attempts": 2, "last_error": "", "setup_mode": False, "setup_ip": "",
    "trial_pending": False, "trial_seconds_left": 120, "csrf_token": "test-token",
    "config": {"primary_ssid": "Rete di prova", "secondary_ssid": "Riserva", "dhcp": True,
               "static_ip": "192.168.10.20", "gateway": "192.168.10.1", "subnet": "255.255.255.0", "dns": "1.1.1.1"}}


class Preview(BaseHTTPRequestHandler):
    status = deepcopy(STATUS)
    previous = None

    def reply(self, code, body, content_type="application/json"):
        data = body.encode() if isinstance(body, str) else json.dumps(body).encode()
        self.send_response(code); self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data))); self.end_headers(); self.wfile.write(data)

    def do_GET(self):
        if self.path in ("/admin", "/"):
            source = (ROOT / "src/DeviceAdminPage.h").read_text(encoding="utf-8")
            self.reply(200, re.search(r'R"EDC\((.*)\)EDC";', source, re.S)[1], "text/html; charset=utf-8")
        elif self.path == "/admin/api/status":
            self.reply(200, self.status)
        else:
            self.reply(404, {})

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        if self.headers.get("X-CSRF-Token") != self.status["csrf_token"]:
            self.reply(403, {"message": "Sessione non valida"}); return
        if self.path.endswith("/scan"):
            self.reply(200, {"networks": [{"ssid": "Rete rilevata", "rssi": -42}]})
        elif self.path.endswith("/config"):
            type(self).previous = deepcopy(self.status["config"])
            args = parse_qs(body.decode(), keep_blank_values=True)
            for key in self.status["config"]:
                self.status["config"][key] = key in args if key == "dhcp" else args.get(key, [""])[0]
            self.status["trial_pending"] = True
            self.reply(202, {"message": "Prova avviata per 120 secondi"})
        elif self.path.endswith("/confirm"):
            self.status["trial_pending"] = False
            self.reply(200, {"message": "Nuova configurazione confermata e salvata"})
        elif self.path.endswith("/cancel"):
            self.status["config"] = type(self).previous or self.status["config"]
            self.status["trial_pending"] = False
            self.reply(200, {"message": "Ripristino della rete precedente"})
        elif self.path.endswith("/restart"):
            self.status["csrf_token"] += "-restarted"
            self.reply(200, {"message": "Riavvio in corso"})
        elif self.path.endswith("/update"):
            start = body.find(b"EDCOTA2\0")
            if start < 0:
                self.reply(400, {"message": "Formato non valido"}); return
            self.status["firmware_version"] = body[start + 88:start + 120].split(b"\0")[0].decode()
            self.status["csrf_token"] += "-updated"
            self.reply(200, {"message": "Firmware verificato"})
        else:
            self.reply(404, {})

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", 8766), Preview).serve_forever()
