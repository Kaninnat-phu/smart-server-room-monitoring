# Smart Server Room — Environmental & Security Monitoring

![Python](https://img.shields.io/badge/Python-3.12-blue)
![ESP32-C6](https://img.shields.io/badge/Hardware-ESP32--C6-green)
![MQTT](https://img.shields.io/badge/Protocol-MQTT%2FTLS-purple)
![OSPF](https://img.shields.io/badge/Routing-OSPF-orange)
![Status](https://img.shields.io/badge/Status-Complete-green)

An IoT-based intrusion-monitoring system for a server room / Security Operations Center (SOC). An ESP32-C6 microcontroller uses its own Wi-Fi signal strength (RSSI) as a presence sensor — when a person moves between the device and the access point, the signal drops, and the system raises an intrusion alert. Data is sent encrypted over MQTT/TLS across a redundant OSPF-routed network to a Python monitoring dashboard.

> University project for ITCS372 (Computer Network Administration Essentials), Mahidol University. Team of 2.

---

## Why this is interesting

Traditional server-room monitoring relies on cameras or PIR sensors, which have blind spots and can be physically bypassed. This project takes a different angle: instead of adding a sensor, it reuses the **Wi-Fi signal itself** as the sensor. A human body absorbs and reflects 2.4 GHz signals, so a person walking past the ESP32-C6 causes a measurable RSSI drop. That drop is the detection signal — no camera required.

The project combines three layers that are usually taught separately: an IoT endpoint, a resilient routed network, and an encrypted application protocol, all validated with real packet captures.

---

## How it works

1. **Sense** — the ESP32-C6 continuously reads its Wi-Fi RSSI (signal strength in dBm).
2. **Transmit** — it publishes each reading over MQTT, encrypted with TLS on port 8883.
3. **Route** — packets travel across a segmented network (separate IoT and server zones) with OSPF dynamic routing providing automatic failover if a link goes down.
4. **Detect** — a Python script subscribes to the MQTT topic and applies a threshold rule: RSSI below **−65 dBm** is flagged as `INTRUSION DETECTED`, otherwise `Normal`.

---

## Detection method (honest description)

This is **rule-based threshold detection**, not machine learning. The logic is a single comparison:

```python
if rssi < THRESHOLD:   # THRESHOLD = -65 dBm
    alert("INTRUSION DETECTED")
else:
    status("Normal")
```

The novelty is in the *idea* — using Wi-Fi RSSI as a physical-presence sensor — not in the algorithm. A natural next step would be replacing the fixed threshold with a trained model (e.g. SVM or a small neural net) on RSSI patterns, which would handle environmental noise better.

---

## Network design

The network is segmented into two zones for security and traffic isolation:

| Network | IP range | Devices |
|---|---|---|
| IoT network | 192.168.10.0/24 | ESP32-C6 (via AP), AP1 |
| Server network | 192.168.20.0/24 | SOC server |
| Router link | 10.0.0.0/30 | R1 ↔ R2 (dual redundant links) |

**OSPF** is used instead of static routing so that if the primary R1–R2 link fails, traffic automatically reroutes over the backup link with minimal packet loss — important for a monitoring system that can't afford to go offline.

---

## Security

| Layer | Mechanism |
|---|---|
| Transport | MQTT over TLS (port 8883) — encrypts the RSSI payload against passive sniffing |
| Wireless | WPA3-SAE on the IoT Wi-Fi |
| Network | ACL on the gateway — permits only the ESP32-C6's address to reach the server |

> **Known limitation (transport security):** the current TLS setup disables certificate verification (`cert_reqs=ssl.CERT_NONE` / `setInsecure()`). This protects against passive eavesdropping but **not** against an active man-in-the-middle attack. A production version would pin the broker's CA certificate. This is left as a documented limitation rather than hidden.

---

## Validation (Wireshark + Packet Tracer)

Real packet captures were used to verify each layer of the stack:

- **`bootp` filter** — confirms the ESP32-C6 obtains an IP via DHCP (Discover → Request).
- **`tcp.port == 1883` filter** — shows the TCP handshake and ACKs for the MQTT session (captured during baseline testing, before TLS).
- **`mqtt` filter** — confirms `Publish Message` packets are reaching the correct topic `soc/sensor/rssi`.
- **Link-failure test** — manually downing the primary R1–R2 link; continuous ping shows brief `Request timed out` then recovery, proving OSPF failover.
- **Latency test** — end-to-end ICMP: 0% packet loss, ~35 ms average, showing TLS + OSPF add no meaningful bottleneck.

Captures and screenshots are in `docs/`.

---

## Tech stack

| Area | Technology |
|---|---|
| Sensor / firmware | ESP32-C6, C++ (Arduino), WiFi + PubSubClient |
| Messaging | MQTT over TLS (EMQX public broker) |
| Monitor | Python, paho-mqtt |
| Network | Cisco Packet Tracer, OSPF, WPA3, ACL |
| Analysis | Wireshark |

---

## Repository structure

```
smart-server-room-monitoring/
├── firmware/
│   └── esp32_sensor.ino       # ESP32-C6: Wi-Fi, RSSI capture, MQTT publish
├── monitor/
│   └── soc_monitor.py         # Python: subscribe, threshold detection, alerts
├── network/
│   ├── network_topology.pkt   # Packet Tracer network simulation
│   └── traffic_capture.pcapng # Wireshark capture
├── docs/
│   ├── report.pdf             # Full project report
│   └── screenshots/           # Wireshark + terminal output
└── README.md
```

---

## Running it

**ESP32-C6 firmware:** open `firmware/esp32_sensor.ino` in the Arduino IDE, set your Wi-Fi SSID and password (placeholders in the file), flash the board, and open Serial Monitor at 115200 baud.

**Python monitor:**
```bash
pip install paho-mqtt
python monitor/soc_monitor.py
```
With no one near the sensor it prints `Normal`; when a person blocks the line of sight and RSSI drops below −65 dBm it prints `INTRUSION DETECTED`.

---

## My role

This was a 2-person project. I (Kaninnat Phuangla-or) built the ESP32-C6 firmware, the Python monitoring script, the security layer (TLS / WPA3 / ACL), the Wireshark protocol validation, and the report. My teammate built the Packet Tracer network simulation.

---

## About

Built by **Kaninnat Phuangla-or** — ICT student at Mahidol University (Network & Security).

🔗 [LinkedIn](https://www.linkedin.com/in/kaninnat-phungla-or/) · [GitHub](https://github.com/Kaninnat-phu)

---

## Disclaimer

Educational / portfolio project. Uses a public MQTT broker and disables TLS certificate verification for simplicity — not production-secure as configured. See the security note above.
