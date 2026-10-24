#include "ESPDeviceCore.h"

String (*ESPDeviceCore::statusProvider_)() = nullptr;

ESPDeviceCore::ESPDeviceCore(const DeviceCoreConfig& config)
    : config_(config) {}

static bool parseIpAddress(const String& value, uint8_t output[4]) {
  int start = 0;
  for (int octet = 0; octet < 4; ++octet) {
    const int end = value.indexOf('.', start);
    const String part = value.substring(start, end < 0 ? value.length() : end);
    if (part.isEmpty()) return false;
    const int number = part.toInt();
    if (number < 0 || number > 255 || String(number) != part) return false;
    output[octet] = static_cast<uint8_t>(number);
    if (end < 0) return octet == 3;
    start = end + 1;
  }
  return false;
}

void ESPDeviceCore::begin() {
  Serial.begin(115200);
  EEPROM.begin(kStorageSize);
  csrfToken_ = String(random(0x100000, 0xFFFFFF), HEX) + String(random(0x100000, 0xFFFFFF), HEX);
  loadStoredConfig();
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
  if (!config_.useDhcp) {
    IPAddress ip(config_.staticIp);
    IPAddress gateway(config_.gateway);
    IPAddress subnet(config_.subnet);
    IPAddress dns(config_.dns);
    if (ip != INADDR_NONE && gateway != INADDR_NONE && subnet != INADDR_NONE) {
      WiFi.config(ip, gateway, subnet, dns);
    }
  }
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

bool ESPDeviceCore::validCsrf() {
  return server_.hasArg("csrf_token") && server_.arg("csrf_token") == csrfToken_;
}

bool ESPDeviceCore::loadStoredConfig() {
  EEPROM.get(0, storedConfig_);
  if (storedConfig_.magic != kStorageMagic) return false;
  config_.primarySsid = storedConfig_.primarySsid;
  config_.primaryPassword = storedConfig_.primaryPassword;
  config_.secondarySsid = storedConfig_.secondarySsid;
  config_.secondaryPassword = storedConfig_.secondaryPassword;
  config_.useDhcp = storedConfig_.useDhcp != 0;
  memcpy(config_.staticIp, storedConfig_.staticIp, sizeof(config_.staticIp));
  memcpy(config_.gateway, storedConfig_.gateway, sizeof(config_.gateway));
  memcpy(config_.subnet, storedConfig_.subnet, sizeof(config_.subnet));
  memcpy(config_.dns, storedConfig_.dns, sizeof(config_.dns));
  return true;
}

bool ESPDeviceCore::saveStoredConfig() {
  storedConfig_.magic = kStorageMagic;
  strlcpy(storedConfig_.primarySsid, config_.primarySsid, sizeof(storedConfig_.primarySsid));
  strlcpy(storedConfig_.primaryPassword, config_.primaryPassword, sizeof(storedConfig_.primaryPassword));
  strlcpy(storedConfig_.secondarySsid, config_.secondarySsid, sizeof(storedConfig_.secondarySsid));
  strlcpy(storedConfig_.secondaryPassword, config_.secondaryPassword, sizeof(storedConfig_.secondaryPassword));
  storedConfig_.useDhcp = config_.useDhcp ? 1 : 0;
  memcpy(storedConfig_.staticIp, config_.staticIp, sizeof(storedConfig_.staticIp));
  memcpy(storedConfig_.gateway, config_.gateway, sizeof(storedConfig_.gateway));
  memcpy(storedConfig_.subnet, config_.subnet, sizeof(storedConfig_.subnet));
  memcpy(storedConfig_.dns, config_.dns, sizeof(storedConfig_.dns));
  EEPROM.put(0, storedConfig_);
  return EEPROM.commit();
}

void ESPDeviceCore::clearStoredConfig() {
  memset(&storedConfig_, 0, sizeof(storedConfig_));
  EEPROM.put(0, storedConfig_);
  EEPROM.commit();
}

void ESPDeviceCore::registerRoutes() {
  server_.on("/", HTTP_GET, [this] { setupMode_ ? sendAdminPage() : sendStatus(); });
  server_.on("/admin", HTTP_GET, [this] { sendAdminPage(); });
  server_.on("/admin/api/status", HTTP_GET, [this] {
    if (authorized()) sendStatus();
  });
  server_.on("/admin/api/restart", HTTP_POST, [this] {
    if (authorized() && validCsrf()) {
      server_.send(200, "application/json", "{\"message\":\"Riavvio in corso\"}");
      restart();
    } else if (server_.hasArg("csrf_token")) {
      server_.send(403, "application/json", "{\"message\":\"Token CSRF non valido\"}");
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
  String page = "<!doctype html><meta charset=utf-8><title>ESP Device Core</title>"
    "<style>body{font-family:sans-serif;max-width:720px;margin:2rem auto}label{display:block;margin-top:.7rem}input{width:100%;padding:.4rem}button{margin-top:1rem;padding:.5rem}</style>"
    "<h1>ESP Device Core</h1><p><a href='/admin/api/status'>Stato JSON</a></p>"
    "<form method=post action='/admin/api/config'><input type=hidden name=csrf_token value='" + csrfToken_ + "'>"
    "<label>Wi-Fi primario<input name=primary_ssid maxlength=63 value='" + String(config_.primarySsid) + "'></label>"
    "<label>Password primaria<input name=primary_password type=password maxlength=63></label>"
    "<label>Wi-Fi secondario<input name=secondary_ssid maxlength=63 value='" + String(config_.secondarySsid) + "'></label>"
    "<label>Password secondaria<input name=secondary_password type=password maxlength=63></label>"
    "<label><input type=checkbox name=dhcp " + String(config_.useDhcp ? "checked" : "") + "> DHCP</label>"
    "<label>IP statico (opzionale)<input name=static_ip placeholder='192.168.1.110'></label>"
    "<label>Gateway<input name=gateway placeholder='192.168.1.254'></label>"
    "<label>Subnet<input name=subnet value='255.255.255.0'></label>"
    "<label>DNS<input name=dns placeholder='192.168.1.254'></label>"
    "<button>Salva Wi-Fi e riavvia</button></form>"
    "<form method=post action='/admin/api/restart'><input type=hidden name=csrf_token value='" + csrfToken_ + "'><button>Riavvia</button></form>"
    "<form method=post action='/admin/api/update' enctype='multipart/form-data'>"
    "<input type=hidden name=csrf_token value='" + csrfToken_ + "'><input type=file name=firmware accept='.bin' required><button>Carica firmware</button></form>";
  server_.send(200, "text/html; charset=utf-8", page);
}

void ESPDeviceCore::handleConfig() {
  if (!authorized()) return;
  if (!validCsrf()) {
    server_.send(403, "application/json", "{\"message\":\"Token CSRF non valido\"}");
    return;
  }
  String primarySsid = server_.arg("primary_ssid");
  String primaryPassword = server_.arg("primary_password");
  String secondarySsid = server_.arg("secondary_ssid");
  String secondaryPassword = server_.arg("secondary_password");
  if (primaryPassword.isEmpty()) primaryPassword = config_.primaryPassword;
  if (secondaryPassword.isEmpty()) secondaryPassword = config_.secondaryPassword;
  if (primarySsid.length() > 63 || primaryPassword.length() > 63 ||
      secondarySsid.length() > 63 || secondaryPassword.length() > 63) {
    server_.send(400, "application/json", "{\"message\":\"Credenziali troppo lunghe\"}");
    return;
  }
  char* primarySsidValue = strdup(primarySsid.c_str());
  char* primaryPasswordValue = strdup(primaryPassword.c_str());
  char* secondarySsidValue = strdup(secondarySsid.c_str());
  char* secondaryPasswordValue = strdup(secondaryPassword.c_str());
  if (!primarySsidValue || !primaryPasswordValue || !secondarySsidValue || !secondaryPasswordValue) {
    free(primarySsidValue);
    free(primaryPasswordValue);
    free(secondarySsidValue);
    free(secondaryPasswordValue);
    server_.send(500, "application/json", "{\"message\":\"Memoria insufficiente\"}");
    return;
  }
  config_.primarySsid = primarySsidValue;
  config_.primaryPassword = primaryPasswordValue;
  config_.secondarySsid = secondarySsidValue;
  config_.secondaryPassword = secondaryPasswordValue;
  config_.useDhcp = server_.hasArg("dhcp");
  if (!config_.useDhcp) {
    const String staticIp = server_.arg("static_ip");
    const String gateway = server_.arg("gateway");
    const String subnet = server_.arg("subnet");
    const String dns = server_.arg("dns");
    if (!parseIpAddress(staticIp, config_.staticIp) ||
        !parseIpAddress(gateway, config_.gateway) ||
        !parseIpAddress(subnet, config_.subnet) ||
        !parseIpAddress(dns, config_.dns)) {
      server_.send(400, "application/json", "{\"message\":\"Indirizzo IP non valido\"}");
      return;
    }
  }
  if (!saveStoredConfig()) {
    server_.send(500, "application/json", "{\"message\":\"Salvataggio configurazione fallito\"}");
    return;
  }
  server_.send(200, "application/json", "{\"message\":\"Configurazione salvata, riavvio in corso\"}");
  delay(300);
  restart();
}

void ESPDeviceCore::handleUpdate() {
  if (!authorized()) return;
  if (!validCsrf()) {
    server_.send(403, "application/json", "{\"message\":\"Token CSRF non valido\"}");
    return;
  }
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
