#pragma once
#include "ESP8266WiFi.h"
enum { HTTP_GET, HTTP_POST, UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { int status=0;String filename;size_t currentSize=0;uint8_t buf[4096]{}; };
struct FakeClient { IPAddress ip=IPAddress(192,168,10,20);IPAddress localIP(){return ip;} };
class ESP8266WebServer {
 public:
  using Handler=std::function<void()>;
  std::map<std::string,Handler> routes,uploads;
  std::map<std::string,String> args,headers;
  FakeClient connection;
  HTTPUpload incoming;
  bool auth=true;int code=0;String body;
  explicit ESP8266WebServer(int) {}
  void collectHeaders(const char**,size_t) {} void begin() {} void handleClient() {}
  void on(const char* path,int,Handler handler) { routes[path]=handler; }
  void on(const char* path,int,Handler handler,Handler upload) { routes[path]=handler;uploads[path]=upload; }
  void send(int status,const char*,const String& value) { code=status;body=value; }
  void send_P(int status,const char* type,const char* value) { send(status,type,value); }
  void send_P(int status,const char* type,const char* value,size_t size) { send(status,type,String(std::string(value,size))); }
  void sendHeader(const char*,const char*) {}
  bool authenticate(const char*,const char*) { return auth; }
  void requestAuthentication() { code=401; }
  bool hasArg(const char* name) { return args.count(name); }
  String arg(const char* name) { return args[name]; }
  String header(const char* name) { return headers[name]; }
  FakeClient& client() { return connection; }
  HTTPUpload& upload() { return incoming; }
  void call(const char* path) { code=0;routes.at(path)(); }
};
