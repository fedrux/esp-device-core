#include "ESPDeviceCore.h"

String (*ESPDeviceCore::statusProvider_)() = nullptr;

ESPDeviceCore::ESPDeviceCore(const DeviceCoreConfig& config) : config_(config) {}

void ESPDeviceCore::begin() {
  Serial.begin(115200);
  connectWifi();
  registerRoutes();
  server_.begin();
}

void ESPDeviceCore::loop() {
  server_.handleClient();
}

void ESPDeviceCore::setStatusProvider(String (*provider)()) {
  statusProvider_ = provider;
}

String ESPDeviceCore::statusText() const {
  return statusProvider_ ? statusProvider_() : status_;
}

bool ESPDeviceCore::restart() {
  status_ = "Riavvio richiesto";
  delay(50);
#if defined(ESP8266)
  ESP.restart();
#else
  ESP.restart();
#endif
  return true;
}

bool ESPDeviceCore::setupMode() const {
  return setupMode_;
}

DeviceWebServer& ESPDeviceCore::server() {
  return server_;
}

void ESPDeviceCore::connectWifi() {
  WiFi.mode(WIFI_STA);
  const char* networks[] = {config_.primarySsid, config_.secondarySsid};
  const char* passwords[] = {config_.primaryPassword, config_.secondaryPassword};
  for (unsigned i = 0; i < 2; ++i) {
    if (!networks[i] || !networks[i][0]) continue;
    WiFi.begin(networks[i], passwords[i]);
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < config_.wifiTimeoutMs) {
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
      status_ = "Wi-Fi collegato: " + WiFi.SSID() + " / " + WiFi.localIP().toString();
      return;
    }
  }
  startSetupAp();
}

void ESPDeviceCore::startSetupAp() {
  setupMode_ = true;
  WiFi.mode(WIFI_AP);
  String suffix = WiFi.macAddress();
  suffix.replace(":", "");
  suffix = suffix.substring(max(0, static_cast<int>(suffix.length()) - 4));
  WiFi.softAP(String("ESP-Setup-") + suffix, config_.apPassword);
  status_ = "Modalita configurazione: http://192.168.4.1/";
}

bool ESPDeviceCore::authorized() {
  if (server_.authenticate(config_.adminUser, config_.adminPassword)) return true;
  server_.requestAuthentication();
  return false;
}

void ESPDeviceCore::registerRoutes() {
  server_.on("/", HTTP_GET, [this] { sendStatus(); });
  server_.on("/admin", HTTP_GET, [this] { sendAdminPage(); });
  server_.on("/admin/api/status", HTTP_GET, [this] {
    if (authorized()) sendStatus();
  });
  server_.on("/admin/api/restart", HTTP_POST, [this] {
    if (authorized()) {
      server_.send(200, "application/json", "{\"message\":\"Riavvio in corso\"}");
      restart();
    }
  });
  server_.on("/admin/api/config", HTTP_POST, [this] { handleConfig(); });
  server_.on("/admin/api/update", HTTP_POST, [this] { handleUpdate(); }, [this] { handleUpload(); });
  server_.onNotFound([this] { server_.send(404, "text/plain", "Pagina non trovata"); });
}

void ESPDeviceCore::sendStatus() {
  String body = "{\"device\":\"" + String(config_.deviceName) + "\",\"status\":\"" +
                statusText() + "\",\"setup_mode\":" + (setupMode_ ? "true" : "false") + "}";
  server_.send(200, "application/json", body);
}

void ESPDeviceCore::sendAdminPage() {
  if (!authorized()) return;
  server_.send(200, "text/html; charset=utf-8",
    "<!doctype html><meta charset=utf-8><title>ESP Device Core</title>"
    "<h1>ESP Device Core</h1><p><a href='/admin/api/status'>Stato JSON</a></p>"
    "<form method=post action='/admin/api/restart'><button>Riavvia</button></form>"
    "<form method=post action='/admin/api/update' enctype='multipart/form-data'>"
    "<input type=file name=firmware accept='.bin' required><button>Carica firmware</button></form>");
}

void ESPDeviceCore::handleConfig() {
  if (!authorized()) return;
  server_.send(501, "application/json", "{\"message\":\"Configurazione Wi-Fi: usare il portale setup nella v1\"}");
}

void ESPDeviceCore::handleUpdate() {
  if (!authorized()) return;
  if (Update.end(true)) {
    server_.send(200, "application/json", "{\"message\":\"Firmware verificato, riavvio in corso\"}");
    delay(500);
    restart();
  } else {
    server_.send(400, "application/json", "{\"message\":\"Firmware non valido\"}");
  }
}

void ESPDeviceCore::handleUpload() {
  if (!authorized()) return;
  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!upload.filename.endsWith(".bin")) return;
#if defined(ESP8266)
    if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) return;
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) return;
#endif
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.end(false);
  }
}
