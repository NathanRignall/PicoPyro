#include "DmxInput.h"
#include "LittleFS.h"
#include "ArduinoJson.h"

#include <pico/cyw43_arch.h>
#include <AsyncFSEditor_RP2040W.h>
#include <AsyncWebServer_RP2040W.h>

#include <DNSServer.h>
#include <Adafruit_NeoPixel.h>

// ------ Shared types ------

enum State
{
  BOOT,
  ACTIVE,
  SETTINGS,
  FAILED
};

enum Status_Led_State
{
  LED_ACTIVE,
  LED_ERROR,
  LED_SETUP,
  LED_FAILED
};

struct Config
{
  int voltage_threshold;

  uint8_t dmx_address;

  uint8_t dmx_pyro0_auth; // channel 1
  uint8_t dmx_pyro1_auth; // channel 2

  uint8_t dmx_pyro0_fire; // channel 3
  uint8_t dmx_pyro1_fire; // channel 4
};

struct Inputs
{
  int pyro0_measure;
  int pyro1_measure;
};

struct Outputs
{
  bool pyro0_led;
  bool pyro1_led;

  enum Status_Led_State status_led;
};

struct Pyro
{
  bool pyro0_fire;
  bool pyro1_fire;
};

struct DMX
{
  bool data;
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

// ------ Input sepecific variables ------

const int reset_button_pin = 14;

const int pyro0_measure_pin = 26;
const int pyro1_measure_pin = 27;

// ------ Output sepecific variables ------

const int led_pin = 25;

const int pyro0_led_pin = 19;
const int pyro1_led_pin = 21;

const int status_led_pin = 10;
int status_led_format = NEO_GRB + NEO_KHZ800;
Adafruit_NeoPixel *status_led_pixels;

// ------ Pyro sepecific variables ------

const int pyro0_fire_pin = 20;
const int pyro1_fire_pin = 22;

// ------ DMX specific variables ------

const int dmx_rdm_pin = 16;
const int dmx_io_pin = 17;

const int dmx_start_channel = 1;
const int dmx_num_channels = 255;

DmxInput dmxInput;
volatile uint8_t dmx_buffer[DMXINPUT_BUFFER_SIZE(dmx_start_channel, dmx_num_channels)];

// ------ Settings specific variables ------

const byte DNS_PORT = 53;

const char *ssid = "PicoPyro";
const char *password = "itsathing";

IPAddress apIP(172, 217, 28, 1);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

// ------ Setup functions ------

bool check_config(StaticJsonDocument<1024> document)
{

  if (!document.containsKey("voltage_threshold"))
  {
    Serial.println("Config is missing voltage_threshold \n");
    return false;
  }

  if (!document.containsKey("dmx_address"))
  {
    Serial.println("Config is missing dmx_address \n");
    return false;
  }

  if (!document.containsKey("dmx_pyro0_auth"))
  {
    Serial.println("Config is missing dmx_pyro0_auth \n");
    return false;
  }

  if (!document.containsKey("dmx_pyro1_auth"))
  {
    Serial.println("Config is missing dmx_pyro1_auth \n");
    return false;
  }

  if (!document.containsKey("dmx_pyro0_fire"))
  {
    Serial.println("Config is missing dmx_pyro0_fire \n");
    return false;
  }

  if (!document.containsKey("dmx_pyro1_fire"))
  {
    Serial.println("Config is missing dmx_pyro1_fire \n");
    return false;
  }

  if (document["voltage_threshold"] < 0 || document["voltage_threshold"] > 1023)
  {
    Serial.println("Config voltage_threshold is out of bounds \n");
    return false;
  }

  if (document["dmx_address"] < 1 || document["dmx_address"] > 251)
  {
    Serial.println("Config dmx_address is out of bounds \n");
    return false;
  }

  if (document["dmx_pyro0_auth"] < 0 || document["dmx_pyro0_auth"] > 255)
  {
    Serial.println("Config dmx_pyro0_auth is out of bounds \n");
    return false;
  }

  if (document["dmx_pyro1_auth"] < 0 || document["dmx_pyro1_auth"] > 255)
  {
    Serial.println("Config dmx_pyro1_auth is out of bounds \n");
    return false;
  }

  if (document["dmx_pyro0_fire"] < 0 || document["dmx_pyro0_fire"] > 255)
  {
    Serial.println("Config dmx_pyro0_fire is out of bounds \n");
    return false;
  }

  if (document["dmx_pyro1_fire"] < 0 || document["dmx_pyro1_fire"] > 255)
  {
    Serial.println("Config dmx_pyro1_fire is out of bounds \n");
    return false;
  }

  Serial.println("Config checked \n");

  return true;
}

bool save_config(StaticJsonDocument<1024> document)
{

  File file = LittleFS.open("/config.json", "w");

  if (!file)
  {
    Serial.println("Could not open config for writing \n");
    return false;
  }

  // check config is valid
  if (!check_config(document))
  {
    Serial.println("Config is invalid \n");
    return false;
  }

  serializeJson(document, file);
  file.close();

  Serial.println("Config saved \n");

  return true;
}

bool load_config()
{

  File file = LittleFS.open("/config.json", "r");

  if (!file)
  {
    Serial.println("Could not open config for reading \n");
    return false;
  }

  StaticJsonDocument<1024> document;
  DeserializationError error = deserializeJson(document, file);

  if (error)
  {
    Serial.println("Could not parse config \n");
    return false;
  }

  if (!check_config(document))
  {
    Serial.println("Config is invalid \n");
    return false;
  }

  system_config.voltage_threshold = document["voltage_threshold"];
  system_config.dmx_address = document["dmx_address"];
  system_config.dmx_pyro0_auth = document["dmx_pyro0_auth"];
  system_config.dmx_pyro1_auth = document["dmx_pyro1_auth"];
  system_config.dmx_pyro0_fire = document["dmx_pyro0_fire"];
  system_config.dmx_pyro1_fire = document["dmx_pyro1_fire"];

  // print system config to serial
  Serial.print("voltage_threshold: ");
  Serial.println(system_config.voltage_threshold);
  Serial.print("dmx_address: ");
  Serial.println(system_config.dmx_address);
  Serial.print("dmx_pyro0_auth: ");
  Serial.println(system_config.dmx_pyro0_auth);
  Serial.print("dmx_pyro1_auth: ");
  Serial.println(system_config.dmx_pyro1_auth);
  Serial.print("dmx_pyro0_fire: ");
  Serial.println(system_config.dmx_pyro0_fire);
  Serial.print("dmx_pyro1_fire: ");
  Serial.println(system_config.dmx_pyro1_fire);

  file.close();

  Serial.println("Config loaded \n");

  return true;
}

void setup_config()
{

  // Check if already setup
  if (setup_config_done)
  {
    return;
  }

  // setup LittleFS
  if (!LittleFS.begin())
  {
    Serial.println("Failed to mount LittleFS \n");
    system_state = FAILED;
    return;
  }

  // load config
  if (!load_config())
  {
    Serial.println("Failed to load config \n");
    system_state = FAILED;
    return;
  }

  Serial.println("Config setup \n");

  // Set done to true
  setup_config_done = true;
}

void setup_inputs()
{

  // Check if already setup
  if (setup_inputs_done)
  {
    return;
  }

  pinMode(reset_button_pin, INPUT);
  Serial.println("Reset button setup \n");

  // Set done to true
  setup_inputs_done = true;
}

void setup_outputs()
{

  // Check if already setup
  if (setup_outputs_done)
  {
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

  status_led_pixels = new Adafruit_NeoPixel(1, status_led_pin, status_led_format);
  status_led_pixels->begin();
  status_led_pixels->setBrightness(50);
  status_led_pixels->clear();
  status_led_pixels->setPixelColor(0, status_led_pixels->Color(255, 255, 255));
  status_led_pixels->show();
  Serial.println("Setatus led setup \n");

  // Set done to true
  setup_outputs_done = true;
}

void setup_pyro()
{

  // Check if already setup
  if (setup_pyro_done)
  {
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

void setup_dmx()
{

  // Check if already setup
  if (setup_settings_done)
  {
    return;
  }

  // Setup RDM
  pinMode(pyro1_fire_pin, OUTPUT);
  digitalWrite(pyro1_fire_pin, 0);
  Serial.println("DMX RDM setup \n");

  // Setup DMX
  dmxInput.begin(dmx_io_pin, dmx_start_channel, dmx_num_channels);
  dmxInput.read_async(dmx_buffer);
  Serial.println("DMX input started \n");

  // set done to true
  setup_dmx_done = true;
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("ws[Server: %s][ClientID: %u] WSClient connected\n", server->url(), client->id());
    client->text("Hello from RP2040W Server");
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("ws[Server: %s][ClientID: %u] WSClient disconnected\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    Serial.printf("ws[Server: %s][ClientID: %u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_DATA)
  {
    // data packet
    AwsFrameInfo *info = (AwsFrameInfo *)arg;

    if (info->final && info->index == 0 && info->len == len)
    {
      if (info->opcode == WS_TEXT)
      {
        data[len] = 0;

        // convert data to json
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, data);

        // check if there is an error
        if (error)
        {
          client->text("Could not decode json");
          Serial.print(F("Could not decode json \n"));
          Serial.println(error.f_str());
          return;
        }

        // check if the message is config
        if (doc.containsKey("config"))
        {
          // get the config
          JsonObject config = doc["config"];

          // check if the config is valid
          if (!check_config(config))
          {
            client->text("Invalid config");
            return;
          }

          // save the config
          if (!save_config(config))
          {
            client->text("Failed to save config");
            return;
          }

          // load the config
          if (!load_config())
          {
            client->text("Failed to load config");
            return;
          }
          
          // successful
          client->text("Saved config \n");
        }
        else
        {
          client->text("Invalid message");
          Serial.println("Invalid message \n");
          return;
        }
      }
    }
  }
}

void setup_settings()
{

  // Check if already setup
  if (setup_settings_done)
  {
    return;
  }

  // Setup Wifi
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);
  Serial.println("Wifi AP started \n");

  // Setup DNS server
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("DNS server started \n");

  // Setup the webserver
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->send(404); });
  server.begin();
  Serial.println("Webserver Setup \n");

  // Set done to true
  setup_settings_done = true;
}

void setup()
{

  //delay for 5 seconds
  delay(5000);

  // Setup hardware required by all
  setup_config();

  // setup inputs and outputs
  setup_inputs();
  setup_outputs();
  setup_dmx();

  // Set the system state
  if (digitalRead(reset_button_pin))
  {
    system_state = SETTINGS;
  }
  else
  {
    // check if system has failed before active
    if (system_state == FAILED)
    {
      Serial.println("System failed! \n");
      return;
    }
    else
    {
      system_state = ACTIVE;
    }
  }

  // Setup the correct hardware based on run mode
  switch (system_state)
  {
  case SETTINGS:

    setup_settings();
    break;

  case ACTIVE:

    setup_pyro();
    break;
  }

  Serial.println("Setup Complete! \n");

  // wait for dmx
  delay(50);
}

// ------ Loop functions ------

void loop_inputs(Inputs *inputs)
{

  inputs->pyro0_measure = analogRead(pyro0_measure_pin);
  inputs->pyro1_measure = analogRead(pyro1_measure_pin);
}

void loop_outputs(Outputs outputs)
{

  digitalWrite(pyro0_led_pin, outputs.pyro0_led);
  digitalWrite(pyro1_led_pin, outputs.pyro1_led);

  status_led_pixels->clear();

  switch (outputs.status_led)
  {

  case LED_ACTIVE:
    status_led_pixels->setPixelColor(0, status_led_pixels->Color(0, 255, 0));
    break;

  case LED_ERROR:
    status_led_pixels->setPixelColor(0, status_led_pixels->Color(255, 0, 0));
    break;

  case LED_SETUP:
    status_led_pixels->setPixelColor(0, status_led_pixels->Color(0, 0, 255));
    break;

  case LED_FAILED:
    status_led_pixels->setPixelColor(0, status_led_pixels->Color(255, 0, 255));
    break;
  }

  status_led_pixels->show();
}

void loop_pyro(Pyro pyro)
{

  digitalWrite(pyro0_fire_pin, pyro.pyro0_fire);
  digitalWrite(pyro1_fire_pin, pyro.pyro1_fire);
}

void loop_dmx(DMX *dmx)
{

  if (millis() > 50 + dmxInput.latest_packet_timestamp())
  {

    dmx->data = false;
  }
  else
  {

    dmx->data = true;
  }
}

void loop_settings()
{

  dnsServer.processNextRequest();
}

void loop_logic(DMX dmx, Inputs inputs, Outputs *outputs, Pyro *pyro)
{

  // check pyro 0 for auth and fire
  if (dmx.data && dmx_buffer[system_config.dmx_address + 0] == system_config.dmx_pyro0_auth && dmx_buffer[system_config.dmx_address + 1] == system_config.dmx_pyro0_fire)
  {
    pyro->pyro0_fire = 1;
  }
  else
  {
    pyro->pyro0_fire = 0;
  }

  // check pyro 1 for auth and fire
  if (dmx.data && dmx_buffer[system_config.dmx_address + 2] == system_config.dmx_pyro1_auth && dmx_buffer[system_config.dmx_address + 3] == system_config.dmx_pyro1_fire)
  {
    pyro->pyro1_fire = 1;
  }
  else
  {
    pyro->pyro1_fire = 0;
  }

  // check pyro 0 measured above threshold and set led
  if (inputs.pyro0_measure > system_config.voltage_threshold)
  {
    outputs->pyro0_led = 1;
  }
  else
  {
    outputs->pyro0_led = 0;
  }

  // check pyro 1 measured above threshold and set led
  if (inputs.pyro1_measure > system_config.voltage_threshold)
  {
    outputs->pyro1_led = 1;
  }
  else
  {
    outputs->pyro1_led = 0;
  }

  // force all outputs to 0 if system failed
  if (system_state == FAILED)
  {
    outputs->pyro0_led = 0;
    outputs->pyro1_led = 0;
    pyro->pyro0_fire = 0;
    pyro->pyro1_fire = 0;
  }

  // check system state and set status led
  if (system_state == ACTIVE)
  {
    // check dmx data and set status led
    if (dmx.data)
    {
      outputs->status_led = LED_ACTIVE;
    }
    else
    {
      outputs->status_led = LED_ERROR;
    }
  }
  else if (system_state == SETTINGS)
  {
    outputs->status_led = LED_SETUP;
  }
  else
  {
    outputs->status_led = LED_FAILED;
  }
}

void loop()
{

  // Local vars
  DMX dmx;
  Inputs inputs;
  Outputs outputs;
  Pyro pyro;

  // Main control loop
  switch (system_state)
  {

  case SETTINGS:
    loop_dmx(&dmx);
    loop_inputs(&inputs);

    loop_logic(dmx, inputs, &outputs, &pyro);
    loop_settings();

    loop_outputs(outputs);

    break;

  case ACTIVE:

    loop_dmx(&dmx);
    loop_inputs(&inputs);

    loop_logic(dmx, inputs, &outputs, &pyro);

    loop_outputs(outputs);
    loop_pyro(pyro);

    break;

  case FAILED:

    loop_logic(dmx, inputs, &outputs, &pyro);

    loop_outputs(outputs);

    break;
  }
}
