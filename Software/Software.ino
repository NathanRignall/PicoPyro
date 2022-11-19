/* Copyright (C) 2022 Nathan Rignall
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Last modified 2022-11
 */

#include "DmxInput.h"
#include "LittleFS.h"
#include "ArduinoJson.h"

#include <pico/cyw43_arch.h>
#include <AsyncFSEditor_RP2040W.h>
#include <AsyncWebServer_RP2040W.h>

#include <DNSServer.h>
#include <Adafruit_NeoPixel.h>

#include "software.h"

// ------ Shared variables ------

enum State system_state = BOOT;
struct Config system_config = {100, 1, 255, 255, 255, 255};

bool setup_done = false;
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

IPAddress apIP(10, 66, 0, 1);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

// ------ Setup functions ------

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
  if (!load_config(&system_config))
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

  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, 0);
  Serial.println("Inbuilt led setup \n");

  pinMode(pyro0_led_pin, OUTPUT);
  digitalWrite(pyro0_led_pin, 0);
  Serial.println("Pyro0 led setup \n");

  pinMode(pyro1_led_pin, OUTPUT);
  digitalWrite(pyro1_led_pin, 0);
  Serial.println("Pyro1 led setup \n");

  // setup the status led and set it to white
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

void on_ws_event(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  // return json object and string
  StaticJsonDocument<1024> return_doc;
  String output;

  if (type == WS_EVT_CONNECT)
  {
    // debug message
    Serial.printf("ws[Server: %s][ClientID: %u] WSClient connected\n", server->url(), client->id());

    // send the current config
    return_doc["config"]["voltage_threshold"] = system_config.voltage_threshold;
    return_doc["config"]["dmx_address"] = system_config.dmx_address;
    return_doc["config"]["dmx_pyro0_auth"] = system_config.dmx_pyro0_auth;
    return_doc["config"]["dmx_pyro1_auth"] = system_config.dmx_pyro1_auth;
    return_doc["config"]["dmx_pyro0_fire"] = system_config.dmx_pyro0_fire;
    return_doc["config"]["dmx_pyro1_fire"] = system_config.dmx_pyro1_fire;
    return_doc["status"] = "Config sent";
    serializeJson(return_doc, output);
    client->text(output);
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    // debug message
    Serial.printf("ws[Server: %s][ClientID: %u] WSClient disconnected\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    // debug message
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
        StaticJsonDocument<1024> input_doc;
        DeserializationError error = deserializeJson(input_doc, data);

        // check if there is an error
        if (error)
        {
          // send json error
          return_doc["status"] = "Could not decode json";
          serializeJson(return_doc, output);
          client->text(output);

          return;
        }

        // check if the message is config
        if (input_doc.containsKey("config"))
        {
          // get the config
          JsonObject config = input_doc["config"];

          // check if the config is valid
          if (check_config(config))
          {

            // save the config
            if (save_config(config))
            {

              // load the config
              if (load_config(&system_config))
              {
                // send the new config
                return_doc["config"]["voltage_threshold"] = system_config.voltage_threshold;
                return_doc["config"]["dmx_address"] = system_config.dmx_address;
                return_doc["config"]["dmx_pyro0_auth"] = system_config.dmx_pyro0_auth;
                return_doc["config"]["dmx_pyro1_auth"] = system_config.dmx_pyro1_auth;
                return_doc["config"]["dmx_pyro0_fire"] = system_config.dmx_pyro0_fire;
                return_doc["config"]["dmx_pyro1_fire"] = system_config.dmx_pyro1_fire;
                return_doc["status"] = "Config saved and loaded";
                serializeJson(return_doc, output);
                client->text(output);
              }
              else
              {
                // send json status
                return_doc["status"] = "Failed to load config";
                serializeJson(return_doc, output);
                client->text(output);
              }
            }
            else
            {
              // send json status
              return_doc["status"] = "Could not save config";
              serializeJson(return_doc, output);
              client->text(output);
            }
          }
          else
          {
            // send json status
            return_doc["status"] = "Invalid config";
            serializeJson(return_doc, output);
            client->text(output);
          }
        }
        else
        {
          // debug message
          Serial.println("Invalid message \n");

          // send json error
          return_doc["status"] = "Invalid message";
          serializeJson(return_doc, output);
          client->text(output);
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
  dnsServer.start(DNS_PORT, "picopyro", apIP);
  dnsServer.start(DNS_PORT, "picopyro", apIP);
  Serial.println("DNS server started \n");

  // Setup the webserver
  ws.onEvent(on_ws_event);
  server.addHandler(&ws);
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");
  server.begin();
  Serial.println("Webserver Setup \n");

  // Set done to true
  setup_settings_done = true;
}

void setup()
{
  // setup serial
  Serial.begin(115200);
  Serial.println("Serial started \n");

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
  setup_done = true;

  // print version message
  Serial.println("Nathan Rignall - Pico Pyro - v1.0.0 \n");

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

void loop()
{
  // main control loop for core0
  // all processing should be done here

  // local vars
  DMX dmx = {0};
  Inputs inputs = {0, 0};
  Outputs outputs = {0, 0, LED_FAILED};
  Pyro pyro = {0, 0};

  // control loop
  switch (system_state)
  {
  case SETTINGS:

    loop_dmx(&dmx);
    loop_inputs(&inputs);

    loop_settings();
    proccess_logic(system_state, dmx, inputs, &outputs, &pyro);

    loop_outputs(outputs);

    break;

  case ACTIVE:

    loop_dmx(&dmx);
    loop_inputs(&inputs);

    proccess_logic(system_state, dmx, inputs, &outputs, &pyro);

    loop_outputs(outputs);
    loop_pyro(pyro);

    break;

  case FAILED:

    proccess_logic(system_state, dmx, inputs, &outputs, &pyro);

    loop_outputs(outputs);

    break;
  }
}
