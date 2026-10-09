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
