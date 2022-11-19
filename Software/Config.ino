/* Copyright (C) 2022 Nathan Rignall
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Last modified 2022-11
 */

#include "ArduinoJson.h"
#include "LittleFS.h"

#include "software.h"

// check the provided json document for correct format
bool check_config(StaticJsonDocument<1024> document)
{

  // check all keys are present
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

  // save config to temp_config to check if it is valid
  struct Config temp_config;
  temp_config.voltage_threshold = document["voltage_threshold"];
  temp_config.dmx_address = document["dmx_address"];
  temp_config.dmx_pyro0_auth = document["dmx_pyro0_auth"];
  temp_config.dmx_pyro1_auth = document["dmx_pyro1_auth"];
  temp_config.dmx_pyro0_fire = document["dmx_pyro0_fire"];
  temp_config.dmx_pyro1_fire = document["dmx_pyro1_fire"];

  // check config is in bounds
  if (temp_config.voltage_threshold < 0 || temp_config.voltage_threshold > 1023)
  {
    Serial.println("Config voltage_threshold is out of bounds \n");
    return false;
  }

  if (temp_config.dmx_address < 1 || temp_config.dmx_address > 251)
  {
    Serial.println("Config dmx_address is out of bounds \n");
    return false;
  }

  if (temp_config.dmx_pyro0_auth < 0 || temp_config.dmx_pyro0_auth > 255)
  {
    Serial.println("Config dmx_pyro0_auth is out of bounds \n");
    return false;
  }

  if (temp_config.dmx_pyro1_auth < 0 || temp_config.dmx_pyro1_auth > 255)
  {
    Serial.println("Config dmx_pyro1_auth is out of bounds \n");
    return false;
  }

  if (temp_config.dmx_pyro0_fire < 0 || temp_config.dmx_pyro0_fire > 255)
  {
    Serial.println("Config dmx_pyro0_fire is out of bounds \n");
    return false;
  }

  if (temp_config.dmx_pyro1_fire < 0 || temp_config.dmx_pyro1_fire > 255)
  {
    Serial.println("Config dmx_pyro1_fire is out of bounds \n");
    return false;
  }

  Serial.println("Config checked \n");

  return true;
}

// save the provided json document to the config file
bool save_config(StaticJsonDocument<1024> document)
{

  File file = LittleFS.open("/config.json", "w");

  if (!file)
  {
    Serial.println("Could not open config for writing \n");
    return false;
  }

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

// load the config file into config struct
bool load_config(Config *config)
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

  config->voltage_threshold = document["voltage_threshold"];
  config->dmx_address = document["dmx_address"];
  config->dmx_pyro0_auth = document["dmx_pyro0_auth"];
  config->dmx_pyro1_auth = document["dmx_pyro1_auth"];
  config->dmx_pyro0_fire = document["dmx_pyro0_fire"];
  config->dmx_pyro1_fire = document["dmx_pyro1_fire"];

  // print system config to serial
  Serial.print("voltage_threshold: ");
  Serial.println(config->voltage_threshold);
  Serial.print("dmx_address: ");
  Serial.println(config->dmx_address);
  Serial.print("dmx_pyro0_auth: ");
  Serial.println(config->dmx_pyro0_auth);
  Serial.print("dmx_pyro1_auth: ");
  Serial.println(config->dmx_pyro1_auth);
  Serial.print("dmx_pyro0_fire: ");
  Serial.println(config->dmx_pyro0_fire);
  Serial.print("dmx_pyro1_fire: ");
  Serial.println(config->dmx_pyro1_fire);

  file.close();

  Serial.println("Config loaded \n");

  return true;
}