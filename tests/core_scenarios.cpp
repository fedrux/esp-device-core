#include "ESPDeviceCore.h"
#include <cassert>
#include <iostream>
#include <fstream>
uint32_t fakeMillis=0;
FakeESP ESP;
FakeWiFi WiFi;
FakeEEPROM EEPROM;
FakeUpdate Update;
ESP_DEVICE_FIRMWARE(identity, "test-app", "1.0");
DeviceCoreConfig config() {
  DeviceCoreConfig c{"test-device","primary","first-pass","secondary","second-pass","admin","admin-pass","recovery-pass"};
  c.firmware=&identity;return c;
}
void tick(ESPDeviceCore& core,uint32_t elapsed) { fakeMillis+=elapsed;core.loop(); }
void reset() { WiFi=FakeWiFi();EEPROM=FakeEEPROM();Update=FakeUpdate();fakeMillis=0;ESP.restarted=false; }
String token(DeviceWebServer& server) {
  server.call("/admin/api/status");const auto start=server.body.value.find("\"csrf_token\":\"")+14;
  return server.body.value.substr(start,32);
}
void prepareConfig(DeviceWebServer& s) {
  s.args.clear();s.args["primary_ssid"]="new-network";s.args["primary_password"]="new-password";s.args["secondary_ssid"]="";s.args["dhcp"]="on";
  s.headers["X-CSRF-Token"]=token(s);
}
std::vector<uint8_t> package(const char* path) {
  std::ifstream f(path,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});
}
void upload(DeviceWebServer& s,const std::vector<uint8_t>& bytes,bool abort=false) {
  auto& u=s.incoming;u.filename="firmware.edc";u.status=UPLOAD_FILE_START;s.uploads.at("/admin/api/update")();
  // Header split across multiple callbacks, then header and payload in one callback.
  for(size_t offset=0;offset<bytes.size();) {
    u.status=UPLOAD_FILE_WRITE;u.currentSize=std::min(size_t(73),bytes.size()-offset);
    memcpy(u.buf,bytes.data()+offset,u.currentSize);s.uploads.at("/admin/api/update")();offset+=u.currentSize;
  }
  u.status=abort?UPLOAD_FILE_ABORTED:UPLOAD_FILE_END;s.uploads.at("/admin/api/update")();
  if(!abort)s.call("/admin/api/update");
}
int main(int argc,char** argv) {
  assert(argc==3);
  reset();
  {
    ESPDeviceCore core(config());core.begin();assert(fakeMillis==0);assert(WiFi.lastSsid=="primary");
    auto& s=core.server();assert(!s.routes.count("/"));
    tick(core,15001);assert(WiFi.lastSsid=="secondary");
    tick(core,15001);assert(core.setupMode());assert(WiFi.ap);
    tick(core,10001);assert(WiFi.lastSsid=="primary");
    WiFi.connection=WL_CONNECTED;tick(core,1);tick(core,30001);assert(!core.setupMode());
    WiFi.connection=WL_DISCONNECTED;tick(core,1);assert(WiFi.lastSsid=="primary");
    s.call("/admin/api/restart");assert(s.code==403);assert(!ESP.restarted);
    s.auth=false;s.call("/admin/api/config");assert(s.code==401);s.auth=true;
    core.setStatusProvider([] { return String("value \"quoted\"\\slash\nline"); });
    s.call("/admin/api/status");assert(s.body.value.find("\\\"quoted\\\"\\\\slash\\u000aline")!=std::string::npos);
  }
  reset();
  {
    ESPDeviceCore core(config());core.begin();auto& s=core.server();prepareConfig(s);
    s.args["primary_password"]="";s.call("/admin/api/config");tick(core,701);
    assert(WiFi.lastPassword==""); // Different SSID never inherits an old password.
    s.call("/admin/api/config/cancel");assert(s.code==200);assert(WiFi.lastSsid=="new-network");
    tick(core,701);assert(WiFi.lastSsid=="primary");assert(WiFi.lastPassword=="first-pass");
    prepareConfig(s);s.args["primary_ssid"]="primary";s.args["primary_password"]="";
    s.call("/admin/api/config");tick(core,701);assert(WiFi.lastPassword=="first-pass");
    // Rebooting during an unconfirmed trial must load original saved defaults.
    ESPDeviceCore reboot(config());reboot.begin();assert(WiFi.lastSsid=="primary");assert(EEPROM.commits==0);
  }
  reset();
  {
    ESPDeviceCore core(config());core.begin();WiFi.connection=WL_CONNECTED;tick(core,1);
    auto& s=core.server();prepareConfig(s);s.call("/admin/api/config");assert(s.code==202);assert(EEPROM.commits==0);
    tick(core,701);assert(WiFi.lastSsid=="new-network");
    tick(core,120001);assert(WiFi.lastSsid=="primary");assert(EEPROM.commits==0);
    s.call("/admin/api/status");assert(s.body.value.find("ripristinata")!=std::string::npos);
    prepareConfig(s);s.call("/admin/api/config");tick(core,701);WiFi.connection=WL_CONNECTED;tick(core,1);
    s.connection.ip=WiFi.softAPIP();s.call("/admin/api/config/confirm");assert(s.code==409);
    s.connection.ip=WiFi.localIP();s.call("/admin/api/config/confirm");assert(s.code==200);assert(EEPROM.commits==1);
    ESPDeviceCore reboot(config());reboot.begin();assert(WiFi.lastSsid=="new-network");
  }
  reset();
  {
    ESPDeviceCore core(config());core.begin();auto& s=core.server();prepareConfig(s);
    s.args.erase("dhcp");s.args["static_ip"]="192.168.10.20";s.args["gateway"]="192.168.11.1";s.args["subnet"]="255.255.255.0";s.args["dns"]="1.1.1.1";
    s.call("/admin/api/config");assert(s.code==400);tick(core,701);assert(WiFi.lastSsid=="primary");assert(EEPROM.commits==0);
  }
  reset();
  {
    ESPDeviceCore core(config());core.begin();auto& s=core.server();auto valid=package(argv[1]),wrong=package(argv[2]);
    upload(s,valid);assert(s.code==403);assert(Update.starts==0);
    s.headers["X-CSRF-Token"]=token(s);upload(s,wrong);assert(s.code==400);assert(Update.starts==0);
    upload(s,valid,true);assert(Update.activations==0);assert(!Update.running);
    auto extra=valid;extra.push_back(0);upload(s,extra);assert(s.code==400);assert(Update.activations==0);
    upload(s,valid);assert(s.code==200);assert(Update.activations==1);assert(!ESP.restarted);
    tick(core,1201);assert(ESP.restarted);
  }
  std::cout<<"Core scenarios passed: fallback, reconnect, recovery AP, rollback, confirm, persistence, validation, auth, OTA rejection/abort/success\n";
}
