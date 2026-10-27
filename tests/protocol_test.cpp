#include "DeviceCoreProtocol.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <fstream>
using namespace devicecore;
int main(int argc, char** argv) {
  uint8_t ip[4], gateway[4], subnet[4], dns[4];
  assert(parseIp("192.168.10.20", ip));
  assert(parseIp("192.168.10.1", gateway));
  assert(parseIp("255.255.255.0", subnet));
  assert(parseIp("1.1.1.1", dns));
  assert(validStatic(ip, gateway, subnet, dns));
  for (const char* invalid : {"", "1.2.3", "1.2.3.4.5", "-1.2.3.4", "a.2.3.4", "256.2.3.4", "1.2.3.4 ", "1..3.4"})
    assert(!parseIp(invalid, ip));
  assert(ip[0] == 192);  // Failed parsing must not partially mutate the candidate.
  assert(parseIp("192.168.11.1", gateway)); assert(!validStatic(ip, gateway, subnet, dns));
  assert(parseIp("192.168.10.1", gateway));
  assert(parseIp("255.0.255.0", subnet)); assert(!validStatic(ip, gateway, subnet, dns));
  assert(parseIp("255.255.255.0", subnet));
  for (const char* invalidHost : {"192.168.10.0", "192.168.10.255", "192.168.10.1", "224.0.0.1"}) {
    assert(parseIp(invalidHost, ip)); assert(!validStatic(ip, gateway, subnet, dns));
  }
  assert(crc32("123456789", 9) == 0xCBF43926);
  assert(argc == 2);
  std::ifstream file(argv[1], std::ios::binary);
  std::vector<uint8_t> package((std::istreambuf_iterator<char>(file)), {});
  assert(package.size() > 256);
  assert(!validatePackage(package.data(), "esp32c3", "test-app"));
  assert(validatePackage(package.data(), "esp8266", "test-app"));
  assert(validatePackage(package.data(), "esp32c3", "other-app"));
  package[20] ^= 1; assert(validatePackage(package.data(), "esp32c3", "test-app"));
  std::cout << "Protocol checks passed\n";
}
