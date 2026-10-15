# OTA Server (Server Pull OTA)

Deze map bevat een Docker-opstelling voor ESP32 OTA achter een bestaande HAProxy (SSL offloading).

HAProxy handelt TLS/certificaten af en proxyt HTTP door naar deze container.

Doel-URL's voor de ESP32 blijven:

- https://ota.prio-ict.nl/webradio
- http://ota.prio-ict.local/webradio

De ESP32 gebruikt:

- /webradio/version.json
- /webradio/firmware.bin

## Structuur

- app.py: Python OTA server (Flask)
- Dockerfile: container image voor de Python server
- docker-compose.yml: start alleen de OTA app (HTTP backend)
- data/webradio/version.txt: actuele firmwareversie
- data/webradio/firmware.bin: OTA firmwarebestand

## Gebruik

1. Plaats firmware in:

   data/webradio/firmware.bin

2. Zet versie in:

   data/webradio/version.txt

   Voorbeeld:
   1.4.2

3. Start stack:

   docker compose up -d --build

4. Test endpoints:

   - http://<docker-host>:8000/webradio/version.json
   - via HAProxy: https://ota.prio-ict.nl/webradio/version.json

## HAProxy configuratie

Voorbeeldconcept:

- Frontend in HAProxy op 443 (en evt 80)
- Backend target: de host/poort van deze container (bijv. 127.0.0.1:8000 of docker netwerkservice)
- Header `X-Forwarded-Proto` op `https` zetten is aanbevolen

Deze OTA container doet zelf geen TLS-terminatie.

### Voorbeeld HAProxy snippet

Pas IP/hostnamen aan naar jouw omgeving.

```haproxy
# ACL's voor hostnamen
acl host_ota_pub hdr(host) -i ota.prio-ict.nl
acl host_ota_local hdr(host) -i ota.prio-ict.local

frontend fe_http
   bind *:80
   mode http

   # Interne URL mag direct op HTTP
   use_backend be_ota_http if host_ota_local

   # Publieke URL op HTTP -> redirect naar HTTPS
   http-request redirect scheme https code 301 if host_ota_pub

frontend fe_https
   bind *:443 ssl crt /etc/haproxy/certs/ota.prio-ict.nl.pem
   mode http

   # SSL offloading; backend ziet gewone HTTP
   http-request set-header X-Forwarded-Proto https
   http-request set-header X-Forwarded-Port 443

   use_backend be_ota_http if host_ota_pub

backend be_ota_http
   mode http
   option httpchk GET /webradio/version.json
   http-check expect status 200

   # Container backend (voorbeeld)
   server ota1 127.0.0.1:8000 check
```

Opmerkingen:

- Gebruik je Docker networking met servicenaam, dan kan backend ook zijn: `server ota1 ota-app:8000 check`.
- Zorg dat DNS naar HAProxy wijst voor beide hostnamen.
- De ESP32 OTA URL's blijven exact:
  - `https://ota.prio-ict.nl/webradio`
  - `http://ota.prio-ict.local/webradio`

## Firmware build artefact kopieren

Na PlatformIO build staat firmware meestal op:

.pio/build/stable/firmware.bin

Voor OTA deploy:

cp .pio/build/stable/firmware.bin ota-server/data/webradio/firmware.bin

## OTA en vrij geheugen

Een melding `BIGNUM - Memory allocation failed` betekent dat de TLS-handshake
geen geheugen kan reserveren. De daaropvolgende `connection refused` is dan
een gevolg van die mislukte handshake, niet noodzakelijk een weigering door
de OTA-server. Vrije PSRAM alleen bewijst niet dat er genoeg intern geheugen
of een voldoende groot aaneengesloten blok beschikbaar is.

De firmware verwerkt OTA-verzoeken in de bestaande webservertask, buiten de
AsyncWebServer-callbacks. Er wordt bij een update geen extra taskstack meer
gereserveerd. Voor de download geeft de audiotask zijn verbindingen en
decoderbuffers vrij en bevestigt dit met een event. Bij een timeout wordt
de update afgebroken. De seriele log toont daarna het vrije interne geheugen,
het grootste vrije interne blok en de vrije PSRAM.

Alleen `HTTP_UPDATE_OK` veroorzaakt een expliciete herstart. Een ontbrekend
manifest, mislukte download of antwoord zonder update herstart de ESP niet;
de fout blijft beschikbaar via `/api/ota/status`. Na een mislukte poging
kun je de radiozender opnieuw starten.

Installeer een reparatie voor een defect OTA-pad zo nodig eenmalig via USB.
Publiceer daarna voor een volgende test uitsluitend de applicatie
`firmware.bin`, niet de gecombineerde `firmware.factory.bin`. Het manifest
moet dezelfde versie noemen als `FIRMWARE_VERSION` in de gepubliceerde build;
alleen de versie op de server aanpassen verandert de versie in de firmware
niet. Een geslaagde update kan anders toch een oude versietekst tonen.

Controleer op het apparaat met een spelende HTTPS-zender dat de audioresources
worden vrijgegeven, de flash slaagt en de nieuwe versie na herstart verschijnt.
Test daarnaast een onbereikbare OTA-server: de fout moet zichtbaar blijven
zonder automatische herstart.
