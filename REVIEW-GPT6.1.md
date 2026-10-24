# Revisione richiesta a GPT-6.1

## Obiettivo

Realizzare un solo core riutilizzabile per firmware ESP8266/ESP32 che fornisca:

- connessione Wi-Fi primaria e secondaria;
- configurazione persistente;
- eventuale DHCP o IP statico;
- pagina web;
- configurazione amministrativa;
- aggiornamento firmware OTA;
- riavvio e diagnostica;
- integrazione Telegram senza pubblicare token;
- mantenimento delle funzioni applicative già esistenti.

Per **ogni firmware/dispositivo** deve esistere **un solo server web e una
sola porta 80**. I quattro firmware non condividono un server fisico: ciascuna
scheda mantiene il proprio server sul proprio IP. Il core comune deve quindi
essere integrato nello stesso server delle funzioni applicative di quel
firmware e non deve avviare un secondo server su `8080` o su un'altra porta.

## Repository e stato Git

- Repository pubblica: https://github.com/fedrux/esp-device-core
- Branch di lavoro: `platform-v1`
- Pull request: https://github.com/fedrux/esp-device-core/pull/1
- Commit iniziale: `4e089c1`
- CI iniziale: `70da55b`
- Implementazione piattaforma: `8a460f9`
- Correzione architetturale single-server: `7876ab7`

Il branch `platform-v1` non è stato fuso automaticamente in `main`.

## File modificati o aggiunti

- `src/ESPDeviceCore.h`
- `src/ESPDeviceCore.cpp`
- `examples/BasicDevice/BasicDevice.ino`
- `examples/LegacyIntegration/README.md`
- `README.md`
- `.github/workflows/compile-example.yml`

## Cosa è stato implementato nel core

### Wi-Fi

- tentativo sulla rete primaria;
- fallback sulla rete secondaria;
- modalità access point `ESP-Setup-XXXX` se entrambe falliscono;
- salvataggio persistente in EEPROM;
- supporto DHCP;
- supporto IP statico, gateway, subnet e DNS;
- form web per aggiornare le configurazioni.

### Web e sicurezza

- `ESP8266WebServer` per ESP8266;
- `WebServer` per ESP32;
- autenticazione Basic sulle route amministrative;
- token CSRF generato all'avvio per POST di configurazione, riavvio e OTA;
- aggiornamento firmware `.bin` autenticato;
- endpoint JSON di stato;
- amministrazione prevista sullo stesso server e sulla porta 80.

Route amministrative previste:

- `/admin`
- `/admin/api/status`
- `/admin/api/config`
- `/admin/api/restart`
- `/admin/api/update`

### Telegram

Il core non contiene un bot Telegram e non contiene token. Espone invece
metodi e stato riutilizzabili, lasciando ai singoli progetti i comandi
specifici. I token devono restare in `secrets.h`, escluso da Git.

### CI e compilazione

La GitHub Actions compila l'esempio su:

- `esp8266:esp8266:nodemcuv2`;
- `esp32:esp32:esp32`.

Le compilazioni locali riuscite:

- ESP8266 NodeMCU;
- ESP32 generico.

Un errore locale precedente era dovuto a compilazioni Arduino parallele che
usavano la stessa directory temporanea, non a un errore del codice.

## Quattro firmware da mantenere

| Progetto | IP previsto | Scheda/linea |
|---|---:|---|
| Pylontech | `192.168.1.109` | monitor NodeMCU con parser Pylontech |
| Spruzzini | `192.168.1.110` | controller con 8 uscite e bot Telegram |
| Energia | `192.168.1.111` | varianti LED/OLED e chiamate server |
| Spruzzini orto | `192.168.1.112` | variante controller con bot Telegram |

Gateway/DNS previsto: `192.168.1.254`.

## Funzioni applicative da non perdere

### Spruzzini

- controllo delle uscite;
- cicli temporizzati;
- comandi Telegram esistenti;
- pagina web esistente;
- stato delle uscite;
- pulsante fisico;
- NTP;
- messaggi e log applicativi.

Il firmware attuale usa un `WiFiServer` manuale sulla porta 80 e funzioni come
`checkForClient`, `triggerPin`, `cicloSpruzzini` e `writer`.

### Spruzzini orto

- comandi Telegram per l'orto;
- controllo pin dedicato;
- pagina web esistente;
- NTP e log applicativi.

Anche questo firmware usa un server manuale e non deve essere sovrascritto
alla cieca.

### Energia

- variante LED;
- variante OLED ESP32-C3;
- richieste al server principale e alla riserva;
- visualizzazione stato batteria, rete, inverter e Tuya;
- IP statico e logica di fallback.

### Pylontech

- parser seriale Pylontech;
- pagina web e API già presenti;
- autenticazione e OTA già presenti nel progetto;
- monitoraggio batterie e conferma finale del protocollo.

Il problema Pylontech diagnosticato in precedenza era nel ritorno RS232:
veniva ricevuto solo l'eco `pwr`, senza frame dati e senza
`Command completed successfully`.

## Punto architetturale critico

La prima proposta usava il core su porta `8080` e lasciava il server
applicativo sulla porta 80. Questo è stato riconosciuto come sbagliato perché
l'utente vuole **un solo core/server che faccia tutto**.

La correzione `7876ab7` ha riportato il core alla porta 80 e ha aggiornato la
documentazione. Tuttavia, la conversione concreta del firmware Spruzzini non
è ancora completa:

- il vecchio sketch usa ancora `WiFiServer` con parsing HTTP manuale;
- il core usa `ESP8266WebServer`;
- non è sufficiente istanziare entrambi;
- bisogna migrare il server Spruzzini a un unico `ESP8266WebServer`;
- bisogna registrare sullo stesso oggetto le route applicative e le route
  `/admin/*` del core;
- bisogna conservare i comportamenti Telegram, pin, cicli e pagina esistente;
- bisogna compilare e provare il firmware migrato prima dell'upload.

La stessa strategia va poi applicata alle altre tre famiglie, con adattatori
specifici quando il server esistente è già diverso.

## Revisione tecnica richiesta

GPT-6.1 dovrebbe controllare in particolare:

1. Se `ESPDeviceCore` può davvero condividere un solo `WebServer` con le route
   applicative senza collisioni.
2. Se l'ordine di registrazione delle route è corretto.
3. Se l'endpoint `/` del core può sovrascrivere la pagina applicativa.
4. Se la modalità AP di configurazione resta raggiungibile tramite `/admin`.
5. Se la persistenza EEPROM è compatibile con ESP8266 ed ESP32.
6. Se la gestione delle stringhe e dei puntatori evita leak e frammentazione
   heap su ESP8266.
7. Se la validazione IP è completa.
8. Se la protezione CSRF è adeguata per un dispositivo locale.
9. Se l'upload OTA gestisce correttamente errori parziali e file non `.bin`.
10. Se l'autenticazione Basic e le password di esempio sono sicure.
11. Se il core non rompe i bot Telegram esistenti.
12. Se la migrazione di `WiFiServer` manuale a `ESP8266WebServer` conserva tutte
    le funzioni degli Spruzzini.
13. Se servono API esplicite per registrare route applicative prima o dopo
    `begin()`.
14. Se è necessario rimuovere o rendere condizionale la route `/` del core.
15. Se il progetto deve usare `Preferences`/LittleFS su ESP32 invece di EEPROM.
16. Se la CI deve includere ESP32-C3 oltre al generico ESP32.
17. Se il vero repeater NAT deve restare fuori dal core.

## Vincoli

- non pubblicare password Wi-Fi;
- non pubblicare password admin;
- non pubblicare token Telegram;
- non modificare `.env` del progetto MondoDentista;
- non cancellare o sovrascrivere gli originali;
- non caricare firmware su una scheda senza identificare porta e modello;
- non dichiarare completata la migrazione dei quattro firmware finché non sono
  compilati e verificati singolarmente;
- mantenere una sola porta/server web per dispositivo;
- mantenere le funzioni applicative già create.

## Esito attuale onesto

Il core pubblico è stato ampliato e compilato, ma la piattaforma completa
multi-firmware non è ancora pronta per il caricamento sui dispositivi fisici.
L'architettura target è:

```text
ESP Spruzzini (.110)       -> un server web sulla porta 80
ESP Energia (.111)          -> un server web sulla porta 80
ESP Pylontech (.109)        -> un server web sulla porta 80
ESP Spruzzini orto (.112)   -> un server web sulla porta 80
                              tutti usano lo stesso core comune
```

La parte ancora necessaria è la migrazione effettiva dei quattro sketch legacy
al rispettivo server unico e la verifica funzionale di ogni dispositivo.
