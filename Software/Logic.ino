/* Copyright (C) 2022 Nathan Rignall
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Last modified 2022-11
 */

#include "software.h"

void proccess_logic(State state, DMX dmx, Inputs inputs, Outputs *outputs, Pyro *pyro)
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
  if (state == FAILED)
  {
    outputs->pyro0_led = 0;
    outputs->pyro1_led = 0;
    pyro->pyro0_fire = 0;
    pyro->pyro1_fire = 0;
  }

  // check system state and set status led
  if (state == ACTIVE)
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
  else if (state == SETTINGS)
  {
    outputs->status_led = LED_SETUP;
  }
  else
  {
    outputs->status_led = LED_FAILED;
  }
}