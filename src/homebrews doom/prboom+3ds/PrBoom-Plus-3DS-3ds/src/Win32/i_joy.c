/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *   Joystick handling for Linux
 *
 *-----------------------------------------------------------------------------
 */

#ifndef lint
#endif /* lint */

#include <stdlib.h>

#include "doomdef.h"
#include "doomtype.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_joy.h"
#include "lprintf.h"
#include "i_system.h"

#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <xinput.h>

static XINPUT_STATE joystick;

void I_PollJoystick(void)
{
  event_t ev;
  short axis_value;

  ev.type = ev_joystick;
  ev.data1 = 0;

  if(XInputGetState(0, &joystick) == ERROR_DEVICE_NOT_CONNECTED)
    memset(&joystick, 0, sizeof(joystick));

  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_A)
    ev.data1 |= (1 << 0);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_B)
    ev.data1 |= (1 << 1);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_X)
    ev.data1 |= (1 << 2);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
    ev.data1 |= (1 << 3);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
    ev.data1 |= (1 << 4);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
    ev.data1 |= (1 << 5);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_START)
    ev.data1 |= (1 << 6);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
    ev.data1 |= (1 << 7);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
    ev.data1 |= (1 << 8);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
    ev.data1 |= (1 << 9);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
    ev.data1 |= (1 << 10);
  if (joystick.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
    ev.data1 |= (1 << 11);

  axis_value = joystick.Gamepad.sThumbLX / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data2 = axis_value;

  axis_value = joystick.Gamepad.sThumbLY / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data3 = axis_value;

  axis_value = joystick.Gamepad.sThumbRX / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data4 = axis_value;

  axis_value = joystick.Gamepad.sThumbRY / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data5 = axis_value;

  D_PostEvent(&ev);
}

void I_InitJoystick(void)
{
}
