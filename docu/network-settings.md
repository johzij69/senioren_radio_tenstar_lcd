# Netwerkinstellingen

De eerste wifi-verbinding blijft via WiFiManager werken. Open daarna in de
webinterface **Netwerk** (`/netwerk`) en druk op **Scannen**. De ESP32-S3 toont
bereikbare 2,4GHz-accesspoints met SSID, BSSID, signaalsterkte in dBm en kanaal.
Mesh-punten met hetzelfde SSID verschijnen afzonderlijk, mits ze een eigen
BSSID hebben. Een minder negatieve dBm-waarde betekent een sterker signaal.

Selecteer een accesspoint van het huidige netwerk en kies **Opslaan en
verbinden**. Andere SSID's zijn zichtbaar maar niet selecteerbaar; een ander
netwerk instellen blijft via WiFiManager gaan. De bestaande inloggegevens
worden hergebruikt. De pagina toont zowel de actieve als de opgeslagen BSSID.

De voorkeur en bijbehorende inloggegevens staan in een aparte Preferences-
namespace `radioNetwork`. Het wachtwoord wordt niet via de netwerk-API
teruggestuurd. Na opslaan wisselt de verbinding na ongeveer twee seconden;
de webpagina en audioverbinding kunnen hierdoor tijdelijk wegvallen.

Bij opstarten en na verbindingsverlies probeert de ESP eerst de opgeslagen
BSSID, zonder een vast kanaal op te leggen. Na de ingestelde timeout
(5 tot 300 seconden, standaard 30) mag elk accesspoint met hetzelfde SSID
worden gebruikt. De voorkeurs-BSSID blijft opgeslagen. Een werkende
terugvalverbinding wordt niet onderbroken om terug te schakelen; bij een
volgende verbindingsuitval of herstart wordt de voorkeur opnieuw geprobeerd.
Als bij opstarten ook terugval niet lukt, blijft WiFiManager beschikbaar.

**Other > reset network** wist deze netwerkvoorkeur en de wifi-inloggegevens
van de ESP32-driver en herstart het apparaat. Zenders, alarmen en overige
radio-instellingen blijven behouden. Na de herstart wordt wifi opnieuw via
WiFiManager ingesteld.

Scans worden asynchroon gestart en buiten de HTTP-callbacks afgehandeld.
Opslaan en verbinden lopen via een wachtrij; HTTP-verzoeken wachten niet op
een scan of wifi-verbinding. Scannen gebruikt wel de gedeelde wifi-radio en
kan tijdens audiostreaming tijdelijk minder bandbreedte geven.

## Controle op het apparaat

1. Scan bij een mesh-netwerk en controleer meerdere BSSID's bij hetzelfde SSID.
2. Kies een punt, controleer de actieve BSSID en herstart om opslag te testen.
3. Schakel het voorkeurspunt uit en controleer terugval na de timeout.
4. Schakel alle punten uit en weer in en controleer opnieuw verbinden.
5. Kies **reset network** en controleer de WiFiManager-portal en het behoud
   van zenders en alarmen.