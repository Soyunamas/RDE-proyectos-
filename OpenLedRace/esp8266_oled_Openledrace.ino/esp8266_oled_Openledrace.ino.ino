#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>      
#include <FS.h>               
#include <LittleFS.h>         
#include <ArduinoJson.h>      

// --- Librería para la Pantalla OLED (ThingPulse) ---
#include <Wire.h>
#include <SSD1306Wire.h> 

// (Dirección I2C, SDA, SCL)
SSD1306Wire display(0x3C, 14, 12);

#define MAXLED         300    
#define PIN_LED        D3     
#define PIN_RESET      D7     // PIN para resetear configuración (Puentear a GND)
#define MQTTPORT       1883
#define SPEEDUPDATE    300

// --- Parámetros por defecto ---
char mqtt_server[40] = "broker.mqtt-dashboard.com";
char track_prefix[20] = "track01"; 
char num_laps_str[5] = "5";
char num_leds_str[5] = "300";

int NPIXELS = 300;     
byte loop_max = 5;     

String GREENCAR, REDCAR, SPEEDRED, SPEEDGREEN;
String START_RACE_TOPIC, SET_LAPS_TOPIC;

bool shouldSaveConfig = false; 
bool race_running = false; 
bool pending_start = false; // NUEVO: Bandera antibloqueos para iniciar carrera

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);

int PIN_P1=0;   
int PIN_P2=0;   

float speed1=0, speed2=0;
float dist1=0, dist2=0;
byte loop1=0, loop2=0;
byte leader=0;

byte last_loop1 = 255;
byte last_loop2 = 255;
bool last_mqtt_state = false;

float ACEL=0.2;
float kf=0.015; 
byte draworder=0;
unsigned long timestamp=0;
int tdelay = 5; 
long lastMsg = 0;

void saveConfigCallback () {
  shouldSaveConfig = true;
}

// Guarda los ajustes dinámicos en la memoria interna
void saveConfigToFile() {
  DynamicJsonDocument json(1024);
  json["mqtt_server"] = mqtt_server;
  json["track_prefix"] = track_prefix;
  json["num_laps"] = num_laps_str;
  json["num_leds"] = num_leds_str;

  File configFile = LittleFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(json, configFile);
    configFile.close();
  }
}

// Callback Modo AP
void configModeCallback (WiFiManager *myWiFiManager) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "MODO AP ACTIVO");
  display.drawString(0, 15, "Conectate al WiFi:");
  display.drawString(0, 28, String(myWiFiManager->getConfigPortalSSID()));
  display.drawString(0, 45, "IP: " + WiFi.softAPIP().toString());
  display.display();
}

// =========================================================================
// ACTUALIZACIÓN DE PANTALLA
// =========================================================================
void updateOLED(bool force = false) {
  if (!force && loop1 == last_loop1 && loop2 == last_loop2 && client.connected() == last_mqtt_state) return;
  
  last_loop1 = loop1;
  last_loop2 = loop2;
  last_mqtt_state = client.connected();

  display.clear();
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "IP: " + WiFi.localIP().toString());
  
  String mqttStatus = client.connected() ? "ON" : "OFF";
  display.drawString(0, 13, "MQTT: " + mqttStatus + " | " + String(track_prefix));
  
  display.drawString(0, 24, "--------------------------");
  
  display.drawString(0, 36, "Verde(P1): " + String(loop1) + " / " + String(loop_max));
  display.drawString(0, 48, "Rojo (P2): " + String(loop2) + " / " + String(loop_max));
  
  display.display();
}

void setup_config_and_wifi() {
  if (LittleFS.begin()) {
    if (LittleFS.exists("/config.json")) {
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        
        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, buf.get());
        if (!error) {
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(track_prefix, json["track_prefix"]);
          strcpy(num_laps_str, json["num_laps"]);
          strcpy(num_leds_str, json["num_leds"]);
        }
      }
    }
  }

  WiFiManagerParameter custom_mqtt_server("server", "Servidor MQTT", mqtt_server, 40);
  WiFiManagerParameter custom_track_prefix("prefix", "Prefijo Pista", track_prefix, 20);
  WiFiManagerParameter custom_laps("laps", "Vueltas", num_laps_str, 5);
  WiFiManagerParameter custom_leds("leds", "LEDs", num_leds_str, 5);

  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(configModeCallback); 
  
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_track_prefix);
  wifiManager.addParameter(&custom_laps);
  wifiManager.addParameter(&custom_leds);

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 20, "Conectando WiFi...");
  display.display();

  if (!wifiManager.autoConnect("OpenLedRace_AP", "12345678")) {
    delay(3000);
    ESP.reset();
  }

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(track_prefix, custom_track_prefix.getValue());
  strcpy(num_laps_str, custom_laps.getValue());
  strcpy(num_leds_str, custom_leds.getValue());

  if (shouldSaveConfig) {
    saveConfigToFile();
  }

  loop_max = atoi(num_laps_str);
  NPIXELS = atoi(num_leds_str);
  if(NPIXELS > MAXLED) NPIXELS = MAXLED; 
  if(loop_max <= 0) loop_max = 5;        

  GREENCAR = String(track_prefix) + "/greencar";
  REDCAR = String(track_prefix) + "/redcar";
  SPEEDRED = String(track_prefix) + "/redspeed";
  SPEEDGREEN = String(track_prefix) + "/greenspeed";
  
  START_RACE_TOPIC = String(track_prefix) + "/start";
  SET_LAPS_TOPIC = String(track_prefix) + "/set_laps";
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP8266-OpenLed-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(GREENCAR.c_str());
      client.subscribe(REDCAR.c_str());
      client.subscribe(START_RACE_TOPIC.c_str());
      client.subscribe(SET_LAPS_TOPIC.c_str());
      updateOLED(true); 
    } else {
      delay(5000);
    }
  }
}

// --- SECUENCIA DE SALIDA FÓRMULA 1 ---
void f1_start_sequence() {
  race_running = false; // Bloquea los mandos
  pending_start = false;
  
  loop1=0; loop2=0; dist1=0; dist2=0; speed1=0; speed2=0; timestamp=0;
  PIN_P1=0; PIN_P2=0;
  updateOLED(true);
  
  track.clear();
  track.show();
  delay(1000);
  
  // Enciende 5 pares de luces rojas secuencialmente
  for(int i = 0; i < 5; i++) {
    track.setPixelColor(i * 2, track.Color(255, 0, 0));
    track.setPixelColor((i * 2) + 1, track.Color(255, 0, 0));
    track.show();
    delay(1000);
  }
  
  // Tiempo de tensión aleatorio
  delay(random(500, 3000));
  
  // ¡Se apagan los semáforos! (Lights Out)
  track.clear();
  track.show();

  // === SOLUCIÓN DEFINITIVA A LA INERCIA ===
  // Vaciamos la cola de mensajes acumulados durante el semáforo rojo.
  // Gracias al uso de 'pending_start', esto ya NO produce cuelgues recursivos.
  for(int i=0; i<15; i++) {
    client.loop();
    delay(10);
  }
  
  // Nos aseguramos de borrar cualquier acelerón almacenado antes de empezar
  PIN_P1=0; PIN_P2=0;
  speed1=0; speed2=0;
  pending_start = false; // Ignoramos clicks repetidos de "Start" en el rojo
  
  // ¡Empieza la carrera!
  race_running = true; 
}

void callback(char* topic, byte* payload, unsigned int length) {
  String Topic(topic);
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  // Aceleradores (solo funcionan si la carrera ha empezado)
  if (Topic.equals(GREENCAR) && race_running) PIN_P1 = 1;
  if (Topic.equals(REDCAR) && race_running) PIN_P2 = 1;

  // Comando de inicio (ahora usa bandera para evitar bloqueos)
  if (Topic.equals(START_RACE_TOPIC)) {
    pending_start = true;
  }

  if (Topic.equals(SET_LAPS_TOPIC)) {
    if (!race_running || (dist1 < 5 && dist2 < 5)) { 
      int new_laps = msg.toInt();
      if (new_laps > 0 && new_laps < 100) { 
        loop_max = new_laps;
        itoa(loop_max, num_laps_str, 10);
        saveConfigToFile(); 
        updateOLED(true);
      }
    }
  }
}

// === CORRECCIÓN DE COLORES ===
// Ahora el coche Verde dibuja en VERDE y el Rojo en ROJO.
void draw_car1(void){ for(int i=0; i<=loop1; i++){ track.setPixelColor(((word)dist1 % NPIXELS)+i, track.Color(0,255-i*20,0)); } } // P1 VERDE
void draw_car2(void){ for(int i=0; i<=loop2; i++){ track.setPixelColor(((word)dist2 % NPIXELS)+i, track.Color(255-i*20,0,0)); } } // P2 ROJO

// === EFECTO BARRIDO Y BAJO CONSUMO USB ===
// Un cometa rápido de 15 LEDs que da 2 vueltas al circuito (tarda ~2.5 seg y usa poca corriente)
void winning_sweep(uint8_t r, uint8_t g, uint8_t b) {
  int comet_size = 15; 
  for (int cycle = 0; cycle < 2; cycle++) { 
    for (int i = 0; i < NPIXELS + comet_size; i+=2) { // i+=2 lo hace dinámico y veloz
      track.clear();
      for (int j = 0; j < comet_size; j++) {
        if (i - j >= 0 && i - j < NPIXELS) {
          int fade_r = r * (comet_size - j) / comet_size;
          int fade_g = g * (comet_size - j) / comet_size;
          int fade_b = b * (comet_size - j) / comet_size;
          track.setPixelColor(i - j, track.Color(fade_r, fade_g, fade_b));
        }
      }
      track.show();
    }
  }
  track.clear();
  track.show();
}

void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  pinMode(PIN_RESET, INPUT_PULLUP);

  display.init();
  display.flipScreenVertically();
  
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 5, "OPEN LED");
  display.drawString(30, 34, "RACE");
  display.display();
  delay(2500); 

  setup_config_and_wifi();

  track.updateLength(NPIXELS);
  track.begin(); 
  
  client.setServer(mqtt_server, MQTTPORT);
  client.setCallback(callback);

  updateOLED(true);
  
  f1_start_sequence();    
}

char msg[20];
   
void loop() {
    // Si la bandera está activa, reiniciamos (Evita los bloqueos del MQTT)
    if (pending_start) {
      f1_start_sequence();
    }

    // LÓGICA DE RESETEO
    if (digitalRead(PIN_RESET) == LOW) {
      long pressTime = millis();
      while(digitalRead(PIN_RESET) == LOW) {
        if(millis() - pressTime > 3000) { 
          display.clear();
          display.setFont(ArialMT_Plain_16);
          display.drawString(0, 15, "Borrando");
          display.drawString(0, 35, "Datos...");
          display.display();

          WiFiManager wifiManager;
          wifiManager.resetSettings();      
          LittleFS.format();                

          delay(2000);
          ESP.restart();                    
        }
        yield(); 
      }
    }

    for(int i=0; i<NPIXELS; i++){ track.setPixelColor(i, track.Color(0,0,0)); }
    
    if (!client.connected()) { reconnect(); }
    
    if (client.connected() != last_mqtt_state) { updateOLED(true); }

    client.loop();

    if (PIN_P1==1) { PIN_P1=0; speed1+=ACEL; }
    speed1 -= speed1*kf; 
    
    if (PIN_P2==1) { PIN_P2=0; speed2+=ACEL; }
    speed2 -= speed2*kf; 

    long now = millis();
    if (now - lastMsg > SPEEDUPDATE) {
       lastMsg = now;
       snprintf(msg, 20, "%.2f", speed1*100);
       client.publish(SPEEDGREEN.c_str(), msg);
       snprintf(msg, 20, "%.2f", speed2*100);
       client.publish(SPEEDRED.c_str(), msg);       
    }
        
    if (race_running) {
      dist1 += speed1;
      dist2 += speed2;
    }

    if (dist1>dist2) leader=1; 
    if (dist2>dist1) leader=2;
      
    if (dist1 > NPIXELS*loop1) { loop1++; updateOLED(); }
    if (dist2 > NPIXELS*loop2) { loop2++; updateOLED(); }

    // === LÓGICA DE GANADORES ===
    if (loop1 > loop_max) {
      display.clear();
      display.setFont(ArialMT_Plain_24);
      display.drawString(10, 20, "GANA P1!");
      display.display();
      
      // Animación rápida Verde
      winning_sweep(0, 255, 0); 
      delay(500); 
      pending_start = true; // Programa la nueva salida de forma segura
    }
    else if (loop2 > loop_max) {
      display.clear();
      display.setFont(ArialMT_Plain_24);
      display.drawString(10, 20, "GANA P2!");
      display.display();
      
      // Animación rápida Roja
      winning_sweep(255, 0, 0); 
      delay(500);
      pending_start = true; // Programa la nueva salida de forma segura
    }

    if ((millis() & 512)==(512*draworder)) {
      if (draworder==0) draworder=1; else draworder=0;   
    } 

    if (draworder==0) { draw_car1(); draw_car2(); }
    else { draw_car2(); draw_car1(); }   
                 
    track.show(); 
    delay(tdelay);
}
