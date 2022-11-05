#include "DmxInput.h"
#include "LittleFS.h"
#include "ArduinoJson.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>

#ifndef STASSID
#define STASSID "PicoPyro"
#define STAPSK "itsathing"
#endif

// ------ Shared types ------

enum State {BOOT, SETTINGS, ACTIVE};

struct Config {
  int voltage;

  uint8_t dmx_pyro0_auth;
  uint8_t dmx_pyro1_auth;

  uint8_t dmx_pyro0_fire;
  uint8_t dmx_pyro1_fire;
};

struct Inputs {
  int pyro0_measure;
  int pyro1_measure;
};

struct Outputs { 
  bool pyro0_led;
  bool pyro1_led;
};

struct Pyro {
  bool pyro0_fire;
  bool pyro1_fire;
};

// ------ Shared variables ------

enum State system_state;
struct Config system_config;

bool setup_config_done = false;
bool setup_inputs_done = false;
bool setup_outputs_done = false;
bool setup_pyro_done = false;
bool setup_dmx_done = false;
bool setup_settings_done = false;


// ------ IO sepecific variables ------

const int led_pin = 25;
const int reset_button_pin = 14;

const int pyro0_led_pin = 19;
const int pyro1_led_pin = 21;

const int pyro0_measure_pin = 26;
const int pyro1_measure_pin = 27;


// ------ Pyro sepecific variables ------

const int pyro0_fire_pin = 20;
const int pyro1_fire_pin = 22;


// ------ DMX specific variables ------

const int dmx_rdm_pin = 16;
const int dmx_io_pin = 17;

const int dmx_start_channel = 1;
const int dmx_num_channels = 255;

DmxInput dmxInput;
volatile uint8_t buffer[DMXINPUT_BUFFER_SIZE(dmx_start_channel, dmx_num_channels)];


// ------ Settings specific variables ------

const byte DNS_PORT = 53;

const char *ssid = STASSID;
const char *password = STAPSK;

IPAddress apIP(172, 217, 28, 1);

WebServer server(80);
DNSServer dnsServer;

// ------ Setup functions ------

void setup_config() {

  // Check if already setup
  if (setup_config_done) {
    return;
  }

  if(!LittleFS.begin()){
    Serial.println("Failed to mount LittleFS \n");
    return;
  }

  File file = LittleFS.open("/config.dat", "r");

  if(!file){
    Serial.println("Cound not open config \n");
    return;
  }
  
  StaticJsonDocument<512> document;
  DeserializationError error = deserializeJson(document, file);

  if (error) {
    Serial.print(F("Could not decode config \n"));
    Serial.println(error.f_str());
    return;
  }

  system_config.voltage = document["voltage"];
  system_config.dmx_pyro0_auth = document["dmx_pyro0_auth"];
  system_config.dmx_pyro1_auth = document["dmx_pyro1_auth"];
  system_config.dmx_pyro0_fire = document["dmx_pyro0_fire"];
  system_config.dmx_pyro1_fire = document["dmx_pyro1_fire"];

  Serial.println(system_config.voltage);
  Serial.println(system_config.dmx_pyro0_auth);
  Serial.println(system_config.dmx_pyro1_auth);
  Serial.println(system_config.dmx_pyro0_fire);
  Serial.println(system_config.dmx_pyro1_fire);

  // Set done to true
  setup_config_done = true;
}

void setup_inputs() {

  // Check if already setup
  if (setup_inputs_done) {
    return;
  }

  pinMode(reset_button_pin, INPUT);
  Serial.println("Reset button setup \n");

  // Set done to true
  setup_inputs_done = true;
  
}

void setup_outputs() {

  // Check if already setup
  if (setup_outputs_done) {
    return;
  }
      
  Serial.begin(115200);
  Serial.println("Serial started \n");

  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, 0);
  Serial.println("Inbuilt led setup \n");
  
  pinMode(pyro0_led_pin, OUTPUT);
  digitalWrite(pyro0_led_pin, 0);
  Serial.println("Pyro0 led setup \n");

  pinMode(pyro1_led_pin, OUTPUT);
  digitalWrite(pyro1_led_pin, 0);
  Serial.println("Pyro1 led setup \n");

  // Set done to true
  setup_outputs_done = true;
  
}

void setup_pyro() {
  
  // Check if already setup
  if (setup_pyro_done) {
    return;
  }

  pinMode(pyro0_fire_pin, OUTPUT);
  digitalWrite(pyro0_fire_pin, 0);
  Serial.println("Pyro0 fire setup \n");

  pinMode(pyro1_fire_pin, OUTPUT);
  digitalWrite(pyro1_fire_pin, 0);
  Serial.println("Pyro1 fire setup \n");

  // Set done to true
  setup_pyro_done = true;
  
}

void setup_dmx() {

    // Check if already setup
    if (setup_settings_done) {
      return;
    }

    // Setup RDM
    pinMode(pyro1_fire_pin, OUTPUT);
    digitalWrite(pyro1_fire_pin, 0);
    Serial.println("DMX RDM setup \n");
    
    // Setup DMX
    dmxInput.begin(dmx_io_pin, dmx_start_channel, dmx_num_channels);
    dmxInput.read_async(buffer);
    Serial.println("DMX input started \n");

    // set done to true
    setup_dmx_done = true;
    
}

bool handleFileRead(String path) {

  String fullPath = "/www" + path;
  
  Serial.println(String("handleFileRead: ") + fullPath);

  if (fullPath.endsWith("/")) {
    fullPath += "index.html";
  }

  String contentType;
  contentType = mime::getContentType(fullPath);

  if (LittleFS.exists(fullPath)) {
    
    File file = LittleFS.open(fullPath, "r");
    
    if (server.streamFile(file, contentType) != file.size()) {
      Serial.println("Sent less data than expected!");
    }
    
    file.close();
    return true;
  }

  return false;
}

void setup_settings() {

    // Check if already setup
    if (setup_settings_done) {
      return;
    }

    // Setup Wifi 
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, password);
    Serial.println("Wifi AP started \n");

    // Setup HTTP server 
    
    server.onNotFound([]() {
      handleFileRead(server.uri());
    });
    
    server.on("/hotspot-detect.html", []() {
//      server.send(200, "text/plain", "this works as well");
    });
    
    server.begin();
    Serial.println("HTTP server started \n");

    // Setup DNS server
    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS server started \n");

    // Set done to true
    setup_settings_done = true;
    
}

void setup() {

  delay(5000);  

  // Setup hardware required by all
  setup_config();
  setup_inputs();
  setup_outputs();

  // Set the system state
  if (!digitalRead(reset_button_pin)) {
    system_state = SETTINGS;
  } else {
    system_state = ACTIVE;
  }

  // Setup the correct hardware based on run mode
  switch(system_state){
      case SETTINGS:
      
        setup_dmx();
        setup_settings();

        break;
  
      case ACTIVE:
      
        setup_dmx();
        setup_pyro();
        
        break;
  }

}

// ------ Loop functions ------

void loop_inputs(Inputs *inputs) {
  
  inputs->pyro0_measure = analogRead(pyro0_measure_pin);
  inputs->pyro1_measure = analogRead(pyro1_measure_pin);
  
}

void loop_outputs(Outputs outputs) {
  
  digitalWrite(pyro0_led_pin, outputs.pyro0_led);
  digitalWrite(pyro1_led_pin, outputs.pyro1_led);
  
}

void loop_pyro(Pyro pyro) {
  
  digitalWrite(pyro0_fire_pin, pyro.pyro0_fire);
  digitalWrite(pyro1_fire_pin, pyro.pyro1_fire);
  
}

void loop_dmx() {
  
  if(millis() > 100+dmxInput.latest_packet_timestamp()) {
    
    Serial.println("no data!");
    return;
    
  }

  Serial.print(buffer[1]);
  Serial.print(", ");
  Serial.print(buffer[255]);
  Serial.print(", ");
  Serial.println("");
    
}

void loop_settings() {
  
  dnsServer.processNextRequest();
  server.handleClient();
  
}

void loop() {

  // temp vars
  Inputs inputs;
  Outputs outputs = {false, false};
  Pyro pyro = {true, true};

  // Main control loop
  switch (system_state) {
    
      case SETTINGS:
        outputs = {true, true};
        
//        loop_dmx();
        loop_inputs(&inputs);

        loop_outputs(outputs);

        break;
  
      case ACTIVE:
      
//        loop_dmx();
        loop_inputs(&inputs);
        
        loop_outputs(outputs);
        loop_pyro(pyro);
        
        break;
        
  }

}

void loop1() {

  loop_settings();

}
