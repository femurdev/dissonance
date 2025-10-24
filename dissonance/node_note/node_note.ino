/* note_node.ino -- run on ESP32 note nodes */
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "ESP_MUSIC_AP";
const char* pass = "pass1234";

WiFiUDP udp;
const uint16_t UDP_PORT = 4210;

double clockOffset = 0.0;   // host_unix_time - (millis()/1000.0)
const double SMOOTH_ALPHA = 0.2; // smoothing factor for offset

void playNote(const String &note) {
  Serial.printf("PLAYING note %s at local time %.3f (offset %.3f)\n",
                note.c_str(), (double)millis()/1000.0, clockOffset);
  // TODO: map note -> actual sound trigger (GPIO, DAC, I2S, etc.)
}

void schedulePlay(const String &note, double targetHostUnix) {
  double localNow = (double)millis() / 1000.0;
  double localTarget = targetHostUnix - clockOffset; // target in local epoch-seconds
  double delaySec = localTarget - localNow;
  if (delaySec <= 0) {
    // If negative, play immediately
    playNote(note);
  } else {
    // Non-blocking scheduling: spawn a simple task using millis() check
    unsigned long execTimeMs = millis() + (unsigned long)(delaySec * 1000.0 + 0.5);
    // simple busy-scheduler: push to a small queue (here simplified: blocking delay)
    // For robust operation, use a timer or task queue. Simpler: create a lambda-like loop.
    while (millis() < execTimeMs) { delay(1); } // OK for small numbers of nodes; replace for heavy use
    playNote(note);
  }
}

void processUdpPacket(char *buf, int len) {
  buf[len] = '\0';
  String s = String(buf);
  s.trim();
  if (s.startsWith("TIME ")) {
    // TIME <unix_seconds> <millis_part>
    // Example: TIME 1698700000 12345
    // We'll compute hostUnixApprox = unix_seconds + millis_part/1000
    char tag[6];
    unsigned long msPart = 0;
    double unixSec = 0.0;
    sscanf(buf, "%5s %lf %lu", tag, &unixSec, &msPart);
    double hostTime = unixSec + (msPart / 1000.0);
    double localNow = (double)millis() / 1000.0;
    double newOffset = hostTime - localNow;
    // Exponential smoothing
    clockOffset = SMOOTH_ALPHA * newOffset + (1.0 - SMOOTH_ALPHA) * clockOffset;
    // Serial
    Serial.printf("TIME beacon: host=%.3f local=%.3f offset=%.6f\n", hostTime, localNow, clockOffset);
  } else if (s.startsWith("PLAY ")) {
    // PLAY <note> <target_unix_time>
    // e.g. PLAY C4 1698700000.823
    char noteStr[16];
    double target = 0.0;
    int n = sscanf(buf, "PLAY %15s %lf", noteStr, &target);
    if (n >= 2) {
      String note = String(noteStr);
      Serial.printf("Received PLAY %s @ %.3f (host unix)\n", note.c_str(), target);
      schedulePlay(note, target);
    }
  } else if (s.startsWith("SET_TIME ")) {
    // optional: host instructs a direct SET_TIME <unix_float>
    double t;
    sscanf(buf, "SET_TIME %lf", &t);
    double localNow = (double)millis() / 1000.0;
    double newOffset = t - localNow;
    clockOffset = newOffset;
    Serial.printf("SET_TIME applied. Offset now %.6f\n", clockOffset);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.printf("Connecting to %s\n", ssid);
  while (WiFi.status() != WL_CONNECTED) { delay(200); Serial.print("."); }
  Serial.println("\nConnected, IP: " + WiFi.localIP().toString());
  udp.begin(UDP_PORT);
  Serial.printf("Listening UDP on %d\n", UDP_PORT);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    static char buf[200];
    int len = udp.read(buf, sizeof(buf)-1);
    if (len > 0) processUdpPacket(buf, len);
  }
  delay(1);
}
