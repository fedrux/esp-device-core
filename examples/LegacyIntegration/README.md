# Integrazione con firmware legacy

Il core deve essere integrato nello stesso server web dello sketch. Ogni scheda
ha il proprio server sulla porta 80; il core e le route non vengono condivisi
tra schede tramite rete. Lo sketch può già possedere:

- un server applicativo sulla porta 80;
- un bot Telegram con comandi specifici;
- una logica di sensori o attuatori già collaudata.

Non creare un secondo `WiFiServer` o `WebServer` **dentro lo stesso firmware**.
Convertire il server legacy
in `ESP8266WebServer`, usare la porta 80 e registrare tutte le route sul server
restituito da `deviceCore.server()`. Le route `/admin/*` sono riservate al core:

```cpp
#include <ESPDeviceCore.h>

DeviceCoreConfig coreConfig{
  "spruzzini",
  WIFI_PRIMARY_SSID, WIFI_PRIMARY_PASSWORD,
  WIFI_SECONDARY_SSID, WIFI_SECONDARY_PASSWORD,
  ADMIN_USER, ADMIN_PASSWORD, SETUP_AP_PASSWORD,
  15000
};

ESPDeviceCore deviceCore(coreConfig);

String applicationStatus() {
  return String("uscite=") + numeroUsciteAttive();
}

void setup() {
  // Inizializza qui la logica legacy e il bot Telegram.
  // Registra le route applicative su deviceCore.server() prima di begin().
  deviceCore.setStatusProvider(applicationStatus);
  deviceCore.begin();
}

void loop() {
  // Mantieni i loop legacy; il core gestisce lo stesso server.
  deviceCore.loop();
  gestisciBotTelegram();
  aggiornaAttuatori();
}
```

Dopo il caricamento:

- applicazione esistente e amministrazione: `http://IP-DISPOSITIVO/`;
- amministrazione, configurazione Wi-Fi e OTA:
  `http://IP-DISPOSITIVO/admin`;
- modalità recupero: collegarsi a `ESP-Setup-XXXX` e aprire
  `http://192.168.4.1/`.

I token Telegram devono stare in un file locale `secrets.h`, escluso da Git.
Non copiare token presenti negli sketch storici nella repository pubblica.

## Mappatura dispositivi

| Progetto | IP previsto | Porta applicazione | Porta core |
|---|---:|---:|---:|
| Spruzzini | 192.168.1.110 | 80 |
| Energia | 192.168.1.111 | 80 |
| Pylontech | 192.168.1.109 | 80 |
| Spruzzini orto | 192.168.1.112 | 80 |

La migrazione va fatta una scheda alla volta: prima si convertono le route del
server legacy, poi si compila e si verifica il firmware sulla scheda corretta.
