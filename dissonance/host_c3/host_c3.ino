/* host_c3.ino -- run on ESP32-C3 (Host) */
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

const char* ap_ssid = "ESP_MUSIC_AP";
const char* ap_pass = "pass1234"; // change for production

WiFiServer tcpServer(5000);
WiFiUDP udp;
const IPAddress broadcastIP(255,255,255,255);
const uint16_t UDP_PORT = 4210;
const unsigned long TIME_BEACON_MS = 1000UL; // beacon every 1s

unsigned long lastBeacon = 0;

// Host time base: when we receive SET_TIME <unix_float>, we compute:
//   baseUnixOffset = provided_time - (millis()/1000.0)
// so current host unix time = baseUnixOffset + (millis()/1000.0)
double baseUnixOffset = 0.0;
bool haveBaseUnix = false;

// Pending plays queue for the host itself (so the host can play the notes it schedules)
struct PendingPlay {
  String note;
  unsigned long execMs;
  bool active;
};
PendingPlay pendingPlays[8]; // small fixed-size queue; adjust if needed

void setup() {
  Serial.begin(115200);
  // Start SoftAP
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(ap_ssid, ap_pass);
  Serial.printf("SoftAP started: %s, IP: %s\n", ap_ssid, WiFi.softAPIP().toString().c_str());
  tcpServer.begin();
  udp.begin(UDP_PORT); // bind to UDP port for sending
  lastBeacon = millis();
}

void sendTimeBeacon() {
  // Send: TIME <unix_seconds> <millis_part>
  // We prefer to send a consistent host unix time if we've been given SET_TIME by the conductor.
  double hostTime;
  if (haveBaseUnix) {
    hostTime = baseUnixOffset + ((double)millis() / 1000.0);
    unsigned long unixSec = (unsigned long)hostTime;
    unsigned long msPart = (unsigned long)((hostTime - (double)unixSec) * 1000.0 + 0.5);
    char buf[80];
    snprintf(buf, sizeof(buf), "TIME %lu %lu\n", unixSec, msPart);
    udp.beginPacket(broadcastIP, UDP_PORT);
    udp.write((uint8_t*)buf, strlen(buf));
    udp.endPacket();
  } else {
    // Fallback: use system time for seconds and local millis() for ms part.
    uint64_t nowUnix = (uint64_t)(time(nullptr));
    unsigned long ms = millis() % 1000;
    char buf[80];
    snprintf(buf, sizeof(buf), "TIME %llu %lu\n", (unsigned long long)nowUnix, ms);
    udp.beginPacket(broadcastIP, UDP_PORT);
    udp.write((uint8_t*)buf, strlen(buf));
    udp.endPacket();
  }
}

void broadcastPlay(const String &playLine) {
  // Forward PLAY lines as-is over UDP to all nodes
  udp.beginPacket(broadcastIP, UDP_PORT);
  udp.write((uint8_t*)playLine.c_str(), playLine.length());
  udp.endPacket();
  Serial.print("Broadcasted: "); Serial.println(playLine);
}

// Host-side play helper (prints and is a placeholder for actual sound output)
void playNoteHost(const String &note) {
  Serial.printf("HOST PLAYING note %s at local time %.3f\n", note.c_str(), (double)millis() / 1000.0);
  // TODO: map note -> actual sound trigger (GPIO, DAC, I2S, etc.) if host should output audio
}

// Schedule a play on the host itself (non-blocking)
void scheduleHostPlay(const String &note, double targetHostUnix) {
  double currentHostUnix = haveBaseUnix ? (baseUnixOffset + ((double)millis() / 1000.0)) : (double)time(nullptr);
  double delaySec = targetHostUnix - currentHostUnix;
  if (delaySec <= 0) {
    playNoteHost(note);
    return;
  }
  unsigned long execMs = millis() + (unsigned long)(delaySec * 1000.0 + 0.5);
  // enqueue into pendingPlays
  for (int i = 0; i < (int)(sizeof(pendingPlays) / sizeof(pendingPlays[0])); ++i) {
    if (!pendingPlays[i].active) {
      pendingPlays[i].note = note;
      pendingPlays[i].execMs = execMs;
      pendingPlays[i].active = true;
      Serial.printf("Scheduled host play %s at execMs=%lu\n", note.c_str(), execMs);
      return;
    }
  }
  // queue full -> play immediately as fallback
  Serial.println("Pending plays queue full; playing immediately on host");
  playNoteHost(note);
}

void handleTcpClient(WiFiClient &client) {
  // read TCP lines; each line should be e.g. "PLAY C4 1698700000.123\n"
  while (client.connected() && client.available()) {
    String line = client.readStringUntil('\n');
    if (line.length() == 0) continue;
    if (line.startsWith("PLAY ")) {
      // Broadcast to all nodes
      broadcastPlay(line + "\n");
      // Also schedule the host itself to play the note at the target time
      char noteStr[16];
      double target = 0.0;
      int n = sscanf(line.c_str(), "PLAY %15s %lf", noteStr, &target);
      if (n >= 2) {
        scheduleHostPlay(String(noteStr), target);
      } else {
        Serial.print("Malformed PLAY line: "); Serial.println(line);
      }
    } else if (line.startsWith("SET_TIME ")) {
      // Accept SET_TIME <unix_seconds_float> to set host's view of unix time
      // Compute baseUnixOffset such that baseUnixOffset + millis()/1000.0 == provided_time
      double t = 0.0;
      int n = sscanf(line.c_str(), "SET_TIME %lf", &t);
      if (n >= 1) {
        baseUnixOffset = t - ((double)millis() / 1000.0);
        haveBaseUnix = true;
        Serial.printf("SET_TIME applied. baseUnixOffset=%.6f\n", baseUnixOffset);
      } else {
        Serial.print("Malformed SET_TIME line: "); Serial.println(line);
      }
      // Broadcast SET_TIME to nodes so they can sync too
      broadcastPlay(line + "\n");
    } else {
      // ignore or log other commands
      Serial.print("TCP recv: "); Serial.println(line);
    }
  }
}

void loop() {
  // Accept TCP connections and read commands
  WiFiClient client = tcpServer.available();
  if (client) {
    Serial.println("PC connected via TCP");
    // handle in-line; for production you'd want to keep connection open in a task
    while (client.connected()) {
      if (client.available()) handleTcpClient(client);
      // optionally break if no data for a while
      delay(5);
    }
    client.stop();
    Serial.println("PC disconnected");
  }

  // Periodic time beacon
  unsigned long now = millis();
  if (now - lastBeacon >= TIME_BEACON_MS) {
    sendTimeBeacon();
    lastBeacon = now;
  }

  // Check pending host plays and fire any that are due
  unsigned long curMs = millis();
  for (int i = 0; i < (int)(sizeof(pendingPlays) / sizeof(pendingPlays[0])); ++i) {
    if (pendingPlays[i].active && (long)(curMs - pendingPlays[i].execMs) >= 0) {
      // due (handles wraparound)
      playNoteHost(pendingPlays[i].note);
      pendingPlays[i].active = false;
    }
  }

  // small delay to yield
  delay(2);
}
