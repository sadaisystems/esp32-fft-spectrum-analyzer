#include <arduinoFFT.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESP32_Supabase.h>
#include <Adafruit_NeoPixel.h>

#include "secrets.h"

// FFT stuff
#define SAMPLES         1024          // Must be a power of 2
#define SAMPLING_FREQ   38000         // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE       1000          // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define AUDIO_IN_PIN    35            // Signal in on this pin
#define NUM_BANDS       8             // Number of frequency bands to use
#define NOISE           1000           // Used as a crude noise filter, values below this are ignored

unsigned int sampling_period_us;
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

// Supabase stuff
Supabase db;
String sb_url = SB_URL;
String sb_api_key = SB_API_KEY;
String sb_table = SB_TABLE;
const String sb_email = SB_EMAIL;
const String sb_password = SB_PASSWORD;
bool upsert = false;

// NeoPixel stuff
#define pinDIN 25
#define numLED 8

Adafruit_NeoPixel rgbWS = Adafruit_NeoPixel(numLED, pinDIN, NEO_GRB + NEO_KHZ800);

// Gobal variables
int start_time;
bool perfrom_fft = false;

// -----------------------------------------------------------------MAIN FUNCTIONS-----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
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
  Serial.print("Connecting to Supabase...");
  db.begin(sb_url, sb_api_key);
  Serial.println(" Connected");

  start_time = millis();

  delay(1000);

  // NeoPixel
  rgbWS.begin();
  signalRGB('w');
}

void loop() {
    // WebServer part
    server.loop();
    // FFT part
    if(!perfrom_fft) {
      return;
    }

    bool detected = performFFT();
    if(detected)
    {
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
    }
    else {
      Serial.println("Nothing...");
    } 

    // NeoPixel part
    for(int i = 0; i < numLED; i++) {
      if (bandValues == 0){
        setRGB(0, 0, 0, i);
        continue;
      }
      int displayValue = bandValues[i] / AMPLITUDE;
      displayValue = map(displayValue, 0, 50, 0, 255);
      switch (i)
      {
      case 0:
        setRGB(0, 0, displayValue, i);
        break;
      case 1:
        setRGB(0, displayValue / 2, displayValue, i);
        break;
      case 2:
        setRGB(0, displayValue, 0, i);
        break;
      case 3:
        setRGB(0, displayValue, displayValue / 2, i);
        break;
      case 4:
        setRGB(displayValue, 0, 0, i);
        break;
      case 5:
        setRGB(displayValue, 0, displayValue / 2, i);
        break;
      case 6:
        setRGB(displayValue, displayValue / 2, 0, i);
        break;
      case 7:
        setRGB(displayValue, displayValue, 0, i);
        break;
      default:
        break;
      }
    }
}
// -----------------------------------------------------------------FFT FUNCTIONS-----------------------------------------------------------------
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
// -----------------------------------------------------------------NeoPixel FUNCTIONS-----------------------------------------------------------------
void setRGB(uint8_t r, uint8_t g, uint8_t b, int led_number) {
  uint32_t color = rgbWS.Color(r, g, b);
  // nastavení barvy pro danou LED diodu,
  // číslo má pořadí od nuly
  rgbWS.setPixelColor(led_number, color);
  // aktualizace barev na všech modulech
  rgbWS.show();
}

void signalRGB(char color){
  uint8_t r, g, b;
  switch (color)
  {
  case 'r':
    r = 255;
    g = 0;
    b = 0;
    break;
  case 'g':
    r = 0;
    g = 255;
    b = 0;
    break;
  case 'b':
    r = 0;
    g = 0;
    b = 255;
    break;
  case 'w':
    r = 255;
    g = 255;
    b = 255;
    break;
  default:
    r = 0;
    g = 0;
    b = 0;
    break;
  }
  for(int i = 0; i < numLED; i++) {
    setRGB(r, g, b, i);
    delay(100);
  }
  delay(1000);
  for(int i = 0; i < numLED; i++) {
    setRGB(0, 0, 0, i);
  }
}
// -----------------------------------------------------------------WEBSERVER FUNCTIONS-----------------------------------------------------------------
// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
    {
      Serial.printf("[%u] Disconnected!\n", num);
      signalRGB('r');
      break;
    }

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = server.remoteIP(num);
        Serial.printf("[%u] Connection from ", num);
        Serial.println(ip.toString());

        signalRGB('g');
      }
      break;

    // Echo text message back to client
    case WStype_TEXT:
    {
      String recieved_text = (char*) payload;

      if(recieved_text == "STOP") {
        if(perfrom_fft == false) {
          Serial.println("Recieved stop message, but FFT is already stopped");
          break;
        }

        perfrom_fft = false;
        Serial.println("Recieved stop message, stopping FFT");
        // Create new row in Supabase database
        String jsonOutString = "";
        JsonObject sendObject = sendJson.to<JsonObject>();
        float duration = (millis() - start_time) / 1000;
        sendObject["duration"] = duration;

        if(duration > 10) {
          sendObject["is_music"] = true;
        }
        else {
          sendObject["is_music"] = false;
        }
        
        serializeJson(sendObject, jsonOutString);

        int HTTPresponseCode = db.insert(sb_table, jsonOutString, upsert);
        Serial.print("HTTP response code (SupaBase-insert): ");
        Serial.println(HTTPresponseCode);
        db.urlQuery_reset();

        // neoPixel turn of the lights
        for(int i = 0; i < numLED; i++) {
          setRGB(0, 0, 0, i);
        }
        delay(500);
      } else if(recieved_text == "START") {
        if(perfrom_fft == true) {
          Serial.println("Recieved start message, but FFT is already started");
          break;
        }
        perfrom_fft = true;
        start_time = millis();

        Serial.println("Recieved start message, starting FFT");
        
        // neoPixel new connection indicator
        signalRGB('w');
      }
      else {
        Serial.println("Recieved unknown message");
      }
    }
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

