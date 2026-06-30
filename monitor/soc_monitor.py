import paho.mqtt.client as mqtt
import ssl  

BROKER = "broker.emqx.io"
PORT = 8883  
TOPIC = "soc/sensor/rssi"
THRESHOLD = -65 

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ SECURE AI Monitor Connected to SOC Server! (TLS Enabled)")
        client.subscribe(TOPIC)
    else:
        print(f"❌ Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode("utf-8")
        rssi = float(payload)
        
        print(f"🔒 [SECURE] Current RSSI: {rssi} dBm", end=" ")
        
        if rssi < THRESHOLD:
            print(" --> 🚨 ALERT: INTRUSION DETECTED! (Signal dropped)")
        else:
            print(" --> ✅ Normal")
            
    except Exception as e:
        print(f"Error: {e}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.tls_set(cert_reqs=ssl.CERT_NONE)
client.tls_insecure_set(True)

print("🚀 Starting SECURE SOC AI Monitor...")
client.connect(BROKER, PORT, 60)

client.loop_forever()