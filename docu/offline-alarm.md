# Offline alarmgeluid

Onder **Instellingen > Offline alarmbestand** kan een MP3 worden geupload.
De upload vervangt `/noalarm.mp3` in LittleFS; er wordt maar een alarmbestand
bewaard. Alle ingestelde alarmen gebruiken hetzelfde bestand als noodgeluid.

## Formaat en grootte

- Alleen MP3 (MPEG Audio Layer III), maximaal 512 KiB / 524288 bytes.
- Gebruik bij voorkeur een gewone MP3 met 64 tot 128 kbit/s, zonder grote
  albumafbeeldingen of tags. Mono is voldoende voor een alarm.
- Bij 128 kbit/s past ongeveer 33 seconden; bij 64 kbit/s ongeveer 66 seconden.
  Het bestand wordt door de bestaande decoder steeds opnieuw afgespeeld.
- De decoder ondersteunt ook andere lokale formaten, maar voor dit
  noodbestand wordt bewust alleen MP3 geaccepteerd. M4A ondersteunt in deze
  decoder bijvoorbeeld geen lokale bestandsloop.

De LittleFS-partitie is 0x360000 bytes (3,375 MiB). De uploadlimiet van 512 KiB
laat ruimte voor fonts, logo's, instellingen en het oude bestand terwijl de
vervanger binnenkomt. De server controleert de actuele vrije ruimte en houdt
128 KiB reserve aan. Bij weinig ruimte kan de werkelijke uploadlimiet lager
zijn. Browsercontrole gebeurt voor verzending; de server controleert opnieuw
en schrijft nooit meer dan de toegestane bestandsgrootte.

## Installatie en vervanging

Plaats het standaardbestand als `data/noalarm.mp3` en upload daarna ook het
LittleFS-bestandssysteem, bijvoorbeeld met:

```sh
~/.platformio/penv/bin/pio run -e stable -t uploadfs
```

Een normale firmware-OTA bevat dit bestandssysteem niet. Het standaardbestand
moet dus eenmaal via een filesystem-upload of de browser op de ESP komen.
Een latere filesystem-upload vervangt ook een eerder via de browser gekozen
alarmbestand door de inhoud van de data-map.

Browseruploads worden eerst naar `/noalarm.upload` geschreven. De server
controleert de ontvangen lengte, eventuele ID3-tag en twee volledige MP3-frame-
headers voordat LittleFS het bestaande bestand atomisch vervangt. Dit is een
formaatcontrole, geen volledige decodecontrole van alle audiogegevens.
Bij afbreken, formaatfouten of schrijffouten blijft het oude bestand behouden.
Tijdens een actief of gesnoozed alarm en een firmware-update zijn uploads
geblokkeerd. Bij herstart wordt eventueel achtergebleven tijdelijke data bij
een volgende upload opgeruimd.

## Offline gedrag

- Bij het afgaan wordt de gekozen radiostream gebruikt als wifi beschikbaar is.
- Ontbreekt wifi of mislukt het openen van de stream, dan start het lokale MP3.
- Valt wifi tijdens een lopend alarm weg, dan schakelt dat alarm over op MP3.
- De lokale loop blijft actief tot handmatig stoppen, snooze of auto-off.
  Wifiherstel onderbreekt een werkend lokaal alarm niet.
- Normale radioweergave en DLNA schakelen niet automatisch naar het noodgeluid.
- Na een mislukte wifi-opstartpoging gaat de initialisatie van alarmen, audio
  en invoer door. De bestaande wifi-/configuratie-timeouts gelden nog steeds.

Voor offline alarmen na een herstart is een geldige, lopende RTC met werkende
batterij nodig. De systeemklok wordt bij opstarten uit de RTC geladen en waar
mogelijk met NTP gesynchroniseerd. Zonder geldige RTC en zonder netwerk is het
juiste alarmtijdstip niet bekend; hiervoor verschijnt een seriele waarschuwing.
Een ontbrekend of beschadigd MP3-bestand wordt ook in de seriele log gemeld.

## Controle op het apparaat

1. Installeer het standaardbestand en controleer de bestandsstatus in Instellingen.
2. Zet een alarm enkele minuten vooruit en schakel wifi uit; controleer de loop,
   het alarmvolume, snooze, handmatig stoppen en auto-off.
3. Herstart zonder wifi en controleer RTC-tijd en het volgende alarm.
4. Schakel wifi tijdens een spelend online alarm uit en controleer de terugval.
5. Upload een nieuwe MP3, verbreek een upload en probeer een te groot bestand;
   alleen een volledige toegestane upload mag het oude bestand vervangen.