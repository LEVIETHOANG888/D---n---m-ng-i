#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>//
#include <WebSocketsServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char* AP_SSID = "DemNguoi";
const char* AP_PASS = "12345678";

const uint8_t IR_A = 14;   
const uint8_t IR_B = 12;   
const bool ACTIVE_LOW = true; 
LiquidCrystal_I2C lcd(0x27, 16, 2);

ESP8266WebServer server(80);
WebSocketsServer ws(81);   

long peopleCount = 0;

enum State { IDLE, SEEN_A, SEEN_B };
State st = IDLE;

uint32_t tStart = 0;
const uint32_t T_WINDOW = 450; 


const char page[] PROGMEM = R"rawliteral(
<!doctype html>
<meta charset="utf-8">
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Đếm người</title>
<div style="font-family:Arial;text-align:center;margin-top:40px">
  <div style="font-size:20px">SỐ NGƯỜI TRONG PHÒNG</div>
  <div id="cnt" style="font-size:100px;font-weight:bold;color:red">0</div>
  <button onclick="ws.send('reset')" style="font-size:18px;padding:10px 20px">Reset</button>
</div>
<script>
let ws = new WebSocket('ws://' + location.hostname + ':81/');
ws.onmessage = e => document.getElementById('cnt').innerText = e.data;
</script>
)rawliteral";


inline bool active(uint8_t pin) {
  bool v = digitalRead(pin);
  return ACTIVE_LOW ? !v : v;
}


void lcdPrintFixed(uint8_t col, uint8_t row, const String &text) {
  lcd.setCursor(col, row);
  lcd.print(text);

  int remain = 16 - col - (int)text.length();
  for (int i = 0; i < remain; i++) lcd.print(' ');
}

void updateLCD() {

  lcdPrintFixed(0, 0, "WiFi: DemNguoi");
  
  lcdPrintFixed(0, 1, "Count: " + String(peopleCount));
}


void sendCount(uint8_t client = 255) {
  String msg = String(peopleCount);
  if (client == 255) ws.broadcastTXT(msg);
  else ws.sendTXT(client, msg);

  updateLCD(); 
}

void onWs(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (type == WStype_CONNECTED) {
    sendCount(num);
  }
  if (type == WStype_TEXT) {
    String cmd;
    cmd.reserve(len);
    for (size_t i = 0; i < len; i++) cmd += (char)payload[i];
    cmd.trim();

    if (cmd == "reset") {
      peopleCount = 0;
      sendCount();
    }
  }
}


void setup() {
  Serial.begin(9600);

  Wire.begin(2, 0);

  lcd.init();         
  lcd.backlight();
  lcd.clear();
  updateLCD();

  pinMode(IR_A, INPUT_PULLUP);
  pinMode(IR_B, INPUT_PULLUP);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", []() {
    server.send(200, "text/html; charset=utf-8", page);
  });
  server.begin();

  ws.begin();
  ws.onEvent(onWs);

  Serial.println("He thong san sang");
}


bool prevA = false, prevB = false;

void loop() {
  server.handleClient();
  ws.loop();

  bool a = active(IR_A);
  bool b = active(IR_B);

  bool riseA = (!prevA && a); prevA = a;
  bool riseB = (!prevB && b); prevB = b;

  uint32_t now = millis();

  switch (st) {
    case IDLE:
      if (riseA) { st = SEEN_A; tStart = now; }
      else if (riseB) { st = SEEN_B; tStart = now; }
      break;

    case SEEN_A:
      if (riseB) {
        peopleCount++;     
        sendCount();
        st = IDLE;
      } else if (now - tStart > T_WINDOW) {
        st = IDLE;
      }
      break;

    case SEEN_B:
      if (riseA) {
        if (peopleCount > 0) peopleCount--; 
        sendCount();
        st = IDLE;
      } else if (now - tStart > T_WINDOW) {
        st = IDLE;
      }
      break;
  }
}
