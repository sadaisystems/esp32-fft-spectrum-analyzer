#include <arduinoFFT.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESP32_Supabase.h>
#include "secrets.h"

#define SAMPLES         1024          // Must be a power of 2
#define SAMPLING_FREQ   38000         // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE       1000          // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define AUDIO_IN_PIN    35            // Signal in on this pin

#define NUM_BANDS       8             // Number of frequency bands to use

#define NOISE           200           // Used as a crude noise filter, values below this are ignored

// Sampling and FFT stuff
unsigned int sampling_period_us;
byte peak[] = {0,0,0,0,0,0,0,0};              // The length of these arrays must be >= NUM_BANDS
// int oldBarHeights[] = {0,0,0,0,0,0,0,0};
int bandValues[] = {0,0,0,0,0,0,0,0};
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime;
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// WIFI server stuff
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebSocketsServer server = WebSocketsServer(80);

StaticJsonDocument<500> sendJson;
StaticJsonDocument<500> receiveJson;

// Supabase stuff
Supabase db;
String sb_url = SB_URL;
String sb_api_key = SB_API_KEY;
String sb_table = SB_TABLE;
const String sb_email = SB_EMAIL;
const String sb_password = SB_PASSWORD;
bool upsert = false;

const int sendingInterval = 60;

// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = server.remoteIP(num);
        Serial.printf("[%u] Connection from ", num);
        Serial.println(ip.toString());
      }
      break;

    // Echo text message back to client
    case WStype_TEXT:
      Serial.printf("[%u] Text: %s\n", num, payload);
      server.sendTXT(num, payload);
      break;

    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}


void setup() {
  Serial.begin(115200);
  // LED indicator of turned on/off
  pinMode(5, OUTPUT);      // set the LED pin mode
  
  // FFT sampling period
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));

  delay(10);
  // WebServer
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Init server
  server.begin();
  server.onEvent(onWebSocketEvent);
  delay(1000);

  // Supabase
  Serial.print("Connecting to Supabase .");
  db.begin(sb_url, sb_api_key);
  // Create new row with start time
  String jsonOutString = ""; 
  JsonObject sendObject = sendJson.to<JsonObject>();
  sendObject["duration"] = 0;
  sendObject["is_music"] = true;
  serializeJson(sendObject, jsonOutString);
  int HTTPresponseCode = db.insert(sb_table, jsonOutString, upsert);
  Serial.print("HTTP response code (SupaBase-insert): ");
  Serial.println(HTTPresponseCode);
  db.urlQuery_reset();
  delay(1000);
}

void loop() {
    // WebServer part
    server.loop();
    // FFT part
    bool detected = performFFT();
    if(detected)
    {
      // Print the band values
      // Serial.print("Band values:");
      // for(int i = 0; i < NUM_BANDS; i++){
      //   Serial.print(" " + String(bandValues[i]));
      // }
      // Serial.println(); 

      // Send the band values to the client
      String jsonOutString = "";
      JsonObject sendObject = sendJson.to<JsonObject>();
      sendObject["band0"] = bandValues[0];
      sendObject["band1"] = bandValues[1];
      sendObject["band2"] = bandValues[2];
      sendObject["band3"] = bandValues[3];
      sendObject["band4"] = bandValues[4];
      sendObject["band5"] = bandValues[5];
      sendObject["band6"] = bandValues[6];
      sendObject["band7"] = bandValues[7];
      serializeJson(sendJson, jsonOutString);
      Serial.println(jsonOutString);
      server.broadcastTXT(jsonOutString);

      // Supabase part
      // int HTTPresponseCode = db.insert("bandTable", jsonOutString, upsert);
      // Serial.print("HTTP response code (SupaBase): ");
      // Serial.println(HTTPresponseCode);
      // db.urlQuery_reset();
    }
    else {
      Serial.println("Nothing...");
    }   
}

bool performFFT() {
  // Reset bandValues[]
  for (int i = 0; i < NUM_BANDS; i++){
    bandValues[i] = 0;
  }

  // Sample the audio pin
  for (int i = 0; i < SAMPLES; i++) {
    newTime = micros();
    vReal[i] = analogRead(AUDIO_IN_PIN); // ~ 10 uS on ESP32
    vImag[i] = 0;
    while ((micros() - newTime) < sampling_period_us) { /* chill */ }
  }

  // Compute FFT
  FFT.DCRemoval();
  FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(FFT_FORWARD);
  FFT.ComplexToMagnitude();

  // Analyse FFT results
  bool notNoise = false;
  for (int i = 2; i < (SAMPLES/2); i++){       // Only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
    if (vReal[i] > NOISE) {
      // 8 bands analysis
      if (i<=3 )           bandValues[0]  += (int)vReal[i];
      if (i>3   && i<=6  ) bandValues[1]  += (int)vReal[i];
      if (i>6   && i<=13 ) bandValues[2]  += (int)vReal[i];
      if (i>13  && i<=27 ) bandValues[3]  += (int)vReal[i];
      if (i>27  && i<=55 ) bandValues[4]  += (int)vReal[i];
      if (i>55  && i<=112) bandValues[5]  += (int)vReal[i];
      if (i>112 && i<=229) bandValues[6]  += (int)vReal[i];
      if (i>229          ) bandValues[7]  += (int)vReal[i];

      notNoise = true;
    }
  }

  return notNoise;
}
