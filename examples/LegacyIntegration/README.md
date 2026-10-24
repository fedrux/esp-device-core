# Integrazione con firmware legacy

Il core può essere aggiunto a uno sketch che possiede già:

- un server applicativo sulla porta 80;
- un bot Telegram con comandi specifici;
- una logica di sensori o attuatori già collaudata.

Non sostituire il server o il bot esistente. Il core usa la porta amministrativa
`8080`, così web applicativo e gestione firmware convivono:

```cpp
#include <ESPDeviceCore.h>

DeviceCoreConfig coreConfig{
  "spruzzini",
  WIFI_PRIMARY_SSID, WIFI_PRIMARY_PASSWORD,
  WIFI_SECONDARY_SSID, WIFI_SECONDARY_PASSWORD,
  ADMIN_USER, ADMIN_PASSWORD, SETUP_AP_PASSWORD,
  15000, 8080
};

ESPDeviceCore deviceCore(coreConfig);

String applicationStatus() {
  return String("uscite=") + numeroUsciteAttive();
}

void setup() {
  // Inizializza qui la logica legacy e il bot Telegram.
  deviceCore.setStatusProvider(applicationStatus);
  deviceCore.begin();
}

void loop() {
  // Mantieni i loop legacy e aggiungi il servizio amministrativo.
  deviceCore.loop();
  gestisciBotTelegram();
  aggiornaAttuatori();
}
```

Dopo il caricamento:

- applicazione esistente: `http://IP-DISPOSITIVO/`;
- amministrazione, configurazione Wi-Fi e OTA:
  `http://IP-DISPOSITIVO:8080/admin`;
- modalità recupero: collegarsi a `ESP-Setup-XXXX` e aprire
  `http://192.168.4.1:8080/`.

I token Telegram devono stare in un file locale `secrets.h`, escluso da Git.
Non copiare token presenti negli sketch storici nella repository pubblica.

## Mappatura dispositivi

| Progetto | IP previsto | Porta applicazione | Porta core |
|---|---:|---:|---:|
| Spruzzini | 192.168.1.110 | 80 | 8080 |
| Energia | 192.168.1.111 | nativa/applicazione | 8080 |
| Pylontech | 192.168.1.109 | nativa/applicazione | 8080 |
| Spruzzini orto | 192.168.1.112 | 80 | 8080 |

La porta 8080 è intenzionale: evita di rompere pagine e API già usate dagli
sketch attuali. La migrazione di ogni sketch va compilata e verificata sulla
scheda corretta prima del caricamento.
