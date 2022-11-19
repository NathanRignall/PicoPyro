/* Copyright (C) 2022 Nathan Rignall
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Last modified 2022-11
 */

#ifndef SOFYWARE_H
#define SOFYWARE_H

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

#endif /* SOFYWARE_H */