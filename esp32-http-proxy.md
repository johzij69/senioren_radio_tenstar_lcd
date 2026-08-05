# HTTP-naar-HTTPS Proxy voor ESP32

## Probleem
ESP32 heeft onvoldoende heap geheugen (~33KB) voor moderne SSL handshake (vereist 40-80KB)

## Oplossing: Lightweight HTTP Proxy

### Optie A: HAProxy HTTP Frontend (SIMPELST)

```haproxy
# HTTP frontend voor ESP32 (poort 8080, alleen lokaal netwerk)
frontend frontend-esp32-http
    bind :8080
    mode http
    
    # Alleen toestaan vanaf lokaal netwerk
    acl is_local_network src 192.168.0.0/16 10.0.0.0/8 172.16.0.0/12
    http-request deny unless is_local_network
    
    # Direct naar backend zonder SSL
    use_backend backend_img

backend backend_img
    mode http
    server img img.prio-ict.nl:443 ssl verify none
```

**ESP32 gebruikt dan:**
```
http://img.prio-ict.nl:8080/api/images/NPO-Radio5.jpg
```

### Optie B: Nginx Reverse Proxy

```nginx
server {
    listen 8080;
    
    # Alleen lokaal netwerk
    allow 192.168.0.0/16;
    deny all;
    
    location /api/images/ {
        proxy_pass https://img.prio-ict.nl:444;
        proxy_ssl_verify off;
        proxy_ssl_server_name on;
    }
}
```

### Optie C: Stunnel (SSL Tunnel)

```ini
[esp32-images]
client = no
accept = 8080
connect = 443
cert = /etc/stunnel/stunnel.pem
```

## Security Overwegingen

✅ **VEILIG** omdat:
- HTTP alleen binnen lokaal netwerk (192.168.x.x)
- Proxy op server doet SSL-terminatie
- Externe toegang altijd via HTTPS (poort 444)
- ESP32 is een IoT device zonder gevoelige user data

❌ **NIET VEILIG** als:
- HTTP verkeer over internet gaat
- ESP32 communiceert over publieke WiFi

## Implementatie

### 1. Pas HAProxy config aan:
```bash
sudo nano /etc/haproxy/haproxy.cfg
# Voeg frontend-esp32-http toe
sudo systemctl restart haproxy
```

### 2. Test vanaf ESP32 netwerk:
```bash
curl -v http://img.prio-ict.nl:8080/api/images/NPO-Radio5.jpg
```

### 3. Pas ESP32 code aan:
```cpp
// In urlmanager.cpp of waar je de image URL bouwt
String imageUrl = "http://img.prio-ict.nl:8080/api/images/" + stationName + ".jpg";
```

## Voordelen
- ✅ Geen geheugen issues op ESP32
- ✅ Sneller (geen SSL overhead op ESP32)
- ✅ Betrouwbaar (geen SSL timeouts)
- ✅ Secure (binnen LAN)
- ✅ Toekomstbestendig (server kan SSL updaten zonder ESP32 reflash)

## Nadelen
- ⚠️ Vereist server configuratie
- ⚠️ HTTP binnen LAN (maar dit is acceptabel voor IoT)
