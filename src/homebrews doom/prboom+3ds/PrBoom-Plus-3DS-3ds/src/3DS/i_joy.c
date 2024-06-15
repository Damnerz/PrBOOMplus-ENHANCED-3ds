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

#include <3ds.h>

void I_PollJoystick(void)
{
  event_t ev;
  short axis_value;

  ev.type = ev_joystick;
  ev.data1 = 0;

  hidScanInput();

  unsigned int kHeld = hidKeysHeld();

  if (kHeld & KEY_A)
    ev.data1 |= (1 << 0);
  if (kHeld & KEY_B)
    ev.data1 |= (1 << 1);
  if (kHeld & KEY_X)
    ev.data1 |= (1 << 2);
  if (kHeld & KEY_Y)
    ev.data1 |= (1 << 3);
  if (kHeld & KEY_L)
    ev.data1 |= (1 << 4);
  if (kHeld & KEY_R)
    ev.data1 |= (1 << 5);
  if (kHeld & KEY_START)
    ev.data1 |= (1 << 6);
  if (kHeld & KEY_SELECT)
    ev.data1 |= (1 << 7);
  if (kHeld & KEY_DUP)
    ev.data1 |= (1 << 8);
  if (kHeld & KEY_DDOWN)
    ev.data1 |= (1 << 9);
  if (kHeld & KEY_DLEFT)
    ev.data1 |= (1 << 10);
  if (kHeld & KEY_DRIGHT)
    ev.data1 |= (1 << 11);

  circlePosition cpos;
  hidCircleRead(&cpos);

  axis_value = (cpos.dx * 192) / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data2 = axis_value;

  axis_value = (cpos.dy * 192) / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data3 = axis_value;

  circlePosition cstickpos;
  irrstCstickRead(&cstickpos);

  axis_value = (cstickpos.dx * 192) / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data4 = axis_value;

  axis_value = (cstickpos.dy * 192) / 3000;
  if (abs(axis_value)<2) axis_value=0;
  ev.data5 = axis_value;

  D_PostEvent(&ev);
}

void I_InitJoystick(void)
{
}
