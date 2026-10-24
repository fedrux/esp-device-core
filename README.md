# ESP Device Core

Un core semplice e riutilizzabile per progetti Arduino su ESP8266 ed ESP32.

## Cosa offre

- Wi-Fi primario e secondario;
- IP DHCP o statico;
- portale locale di configurazione quando entrambe le reti non sono disponibili;
- salvataggio persistente delle reti in EEPROM;
- pagina web protetta da autenticazione Basic;
- aggiornamento firmware OTA protetto con token CSRF locale;
- riavvio e diagnostica;
- hook opzionale per integrare il proprio bot Telegram senza inserire token nel core.

## Avvio rapido

1. Copia `examples/BasicDevice` in un nuovo sketch.
2. Copia `secrets.example.h` come `secrets.h`.
3. Inserisci le credenziali locali in `secrets.h`.
4. Installa la libreria `ESP Device Core`.
5. Compila e carica il firmware via USB la prima volta.
6. Apri l'IP mostrato dal dispositivo e visita `/admin`.

Se nessuna rete è disponibile, il dispositivo crea un access point `ESP-Setup-XXXX`.
Collegati a quella rete e apri `http://192.168.4.1:8080/`. Il portale permette di
salvare entrambe le reti, scegliere DHCP o IP statico e riavviare il dispositivo.
Quando il dispositivo è già collegato alla LAN, il portale è disponibile
sullo stesso server e sulla stessa porta 80, all'indirizzo
`http://192.168.1.110/admin`.

## Sicurezza

- `secrets.h` è escluso da Git.
- OTA e configurazione richiedono autenticazione.
- Il core non contiene token Telegram, password Wi-Fi o chiavi API.
- Cambia subito la password predefinita dell'esempio.
- Non esporre la pagina web direttamente su Internet.

## Telegram

Il core non implementa un bot Telegram proprietario: ogni progetto può collegare
la propria libreria e usare `deviceCore.statusText()` e `deviceCore.restart()`.
Questa separazione evita di pubblicare token e consente di mantenere i comandi
specifici di Spruzzini, Energia e Pylontech.
In questo modo un progetto per spruzzini, energia o sensori mantiene i suoi
comandi senza duplicare la gestione Wi-Fi, web e OTA.

## Repeater

La v1 non è un repeater NAT. Il portale di configurazione è un access point
locale per recuperare il dispositivo, non un'estensione Internet. Un repeater
completo dipende dal chipset, dal core Wi-Fi e dalla topologia della rete e va
implementato e testato come progetto separato.

## Integrazione firmware esistenti

Ogni firmware ha il proprio server web sulla porta 80. Per aggiungere il core a
un firmware che ha già una pagina web o un bot Telegram, usare un solo
`ESP8266WebServer`/`WebServer` per quella scheda e registrare le route
applicative e quelle amministrative sullo stesso oggetto. Il core è comune ai
quattro firmware, ma i server restano separati per dispositivo. La procedura è documentata in
[examples/LegacyIntegration/README.md](examples/LegacyIntegration/README.md).

## Licenza

MIT. Vedi [LICENSE](LICENSE).
