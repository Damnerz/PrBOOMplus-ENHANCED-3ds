/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2006 by
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
 *  DOOM graphics stuff for SDL
 *
 *-----------------------------------------------------------------------------
 */

#include <strings.h>
#include <stdlib.h>
#include <assert.h>

#include <SDL/SDL.h>

#include "m_argv.h"
#include "doomstat.h"
#include "doomdef.h"
#include "doomtype.h"
#include "v_video.h"
#include "r_draw.h"
#include "r_things.h"
#include "r_plane.h"
#include "r_main.h"
#include "f_wipe.h"
#include "d_main.h"
#include "d_event.h"
#include "d_deh.h"
#include "i_joy.h"
#include "i_video.h"
#include "z_zone.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "st_stuff.h"
#include "am_map.h"
#include "g_game.h"
#include "lprintf.h"
#include "i_system.h"

#ifdef GL_DOOM
#include "gl_struct.h"
#endif

#include "e6y.h"//e6y
#include "i_main.h"

#include <3ds.h>

// 0 = mouse, 1 = keyboard
static int ctr_input_mode = 0;

// 3DS touchpad (mouse)
static int ctr_mouse_pos[2] = { 0, 0 };

static int ctr_touch_time_base = 0;
static int ctr_dbl_tch_time_base = 0;

static int is_ctr_double_touch = 0;
static int is_ctr_double_held = 0;
static int is_ctr_touch_down = 0;

static int ctr_key_down = 0;

static int mouse_currently_grabbed = true;

static void ActivateMouse(void);
static void DeactivateMouse(void);
//static int AccelerateMouse(int val);
static void I_ReadMouse(void);
static dboolean MouseShouldBeGrabbed();

extern void M_QuitDOOM(int choice);

int vanilla_keymap;
static void *screen = NULL;

////////////////////////////////////////////////////////////////////////////
// Input code
static int             leds_always_off = 0; // Expected by m_misc, not relevant

// Mouse handling
extern int     usemouse;        // config file var
static dboolean mouse_enabled; // usemouse, but can be overriden by -nomouse

video_mode_t I_GetModeFromString(const char *modestr);

/////////////////////////////////////////////////////////////////////////////////
// Keyboard handling

static char ctr_kb_map[40*14] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x1B,0x00,0x00,0xBB,0x00,0xBC,0x00,0xBD,0x00,0xBE,0x00,0x00,0xBF,0x00,0xC0,0x00,0xC1,0x00,0xC2,0x00,0x00,0xC3,0x00,0xC4,0x00,0xD7,0x00,0xD8,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0xC6,0x00,0xff,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x60,0x00,0x31,0x00,0x32,0x00,0x33,0x00,0x34,0x00,0x35,0x00,0x36,0x00,0x37,0x00,0x38,0x00,0x39,0x00,0x30,0x00,0x2D,0x00,0x3D,0x00,0x7F,0x7F,0x7F,0x00,0x00,0xD2,0x00,0xC7,0x00,0xC9,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x09,0x09,0x09,0x00,0x00,0x71,0x00,0x77,0x00,0x65,0x00,0x72,0x00,0x74,0x00,0x79,0x00,0x75,0x00,0x69,0x00,0x6F,0x00,0x70,0x00,0x5B,0x00,0x5D,0x00,0x0D,0x0D,0x00,0x00,0xC8,0x00,0xCF,0x00,0xD1,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0xBA,0xBA,0xBA,0xBA,0x00,0x00,0x61,0x00,0x73,0x00,0x64,0x00,0x66,0x00,0x67,0x00,0x68,0x00,0x6A,0x00,0x6B,0x00,0x6C,0x00,0x3B,0x00,0x27,0x00,0x23,0x00,0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0xB6,0xB6,0xB6,0xB6,0x00,0x5C,0x00,0x7A,0x00,0x78,0x00,0x63,0x00,0x76,0x00,0x62,0x00,0x6E,0x00,0x6D,0x00,0x2C,0x00,0x2E,0x00,0x2F,0x00,0xB6,0xB6,0xB6,0xB6,0x00,0x00,0x00,0x00,0xad,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x9D,0x9D,0x00,0xB8,0xB8,0x00,0xB8,0xB8,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0xB8,0xB8,0x00,0x00,0x00,0x00,0xB8,0xB8,0x00,0xB8,0xB8,0x00,0x00,0xac,0x00,0xaf,0x00,0xae,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

/////////////////////////////////////////////////////////////////////////////////
// Main input code

static int I_IsPointInRect(int px, int py, int x1, int y1, int x2, int y2)
{
  return (px >= x1) && (px <= x2) && (py >= y1) && (py <= y2);
}

static void I_GetEvent(void)
{
  event_t event;

  u32 keys_down = hidKeysDown();
  u32 keys_up = hidKeysUp();

  touchPosition touch;
  hidTouchRead(&touch);

  if((keys_down & KEY_TOUCH) && I_IsPointInRect(touch.px, touch.py, 32, 208, 120, 240))
  {
    ctr_input_mode = !ctr_input_mode;
    return;
  }

  if(ctr_input_mode) // Keyboard
  {
    for(int y = 0; y < 14; y++)
    {
      for(int x = 0; x < 40; x++)
      {
        if((keys_down & KEY_TOUCH) && I_IsPointInRect(touch.px, touch.py, (x*8), (y*8), (x*8) + 8, (y*8) + 8))
        {
          int translate_key = ctr_kb_map[(y*40) + x];

          if(translate_key != 0)
          {
            ctr_key_down = translate_key;

            event.type = ev_keydown;
            event.data1 = translate_key;
            D_PostEvent(&event);
          }
        }
      }
    }

    if((keys_up & KEY_TOUCH) && ctr_key_down != 0)
    {
      event.type = ev_keyup;
      event.data1 = ctr_key_down;
      D_PostEvent(&event);
    }
  }
  else // Mouse
  {
    if(mouse_currently_grabbed)
    {
      // Reset mouse buttons state
      event.type = ev_mouse;
      event.data1 = is_ctr_double_held ? (1 << 0) : 0;
      event.data2 = event.data3 = 0;
      D_PostEvent(&event);

      if(keys_down & KEY_TOUCH)
      {
        if(is_ctr_double_touch)
          is_ctr_double_held = (I_GetTime_MS() <= ctr_dbl_tch_time_base + 200) ? 1 : 0;

        // Set the new relative mouse origin
        ctr_mouse_pos[0] = touch.px;
        ctr_mouse_pos[1] = touch.py;

        is_ctr_touch_down = 1;
        ctr_touch_time_base = I_GetTime_MS();
      }

      if(keys_up & KEY_TOUCH)
      {
        // Treat a quick tap on the screen as a left mouse click
        if(I_GetTime_MS() <= ctr_touch_time_base + 200)
        {
          event.type = ev_mouse;
          event.data1 = (1 << 0);
          event.data2 = event.data3 = 0;
          D_PostEvent(&event);

          is_ctr_double_touch = 1;
          ctr_dbl_tch_time_base = I_GetTime_MS();
        }
        else
        {
          is_ctr_double_touch = 0;
        }

        is_ctr_double_held = 0;
        is_ctr_touch_down = 0;
      }
    }
    else
    {
      // Reset mouse buttons state
      event.type = ev_mouse;
      event.data1 = event.data2 = event.data3 = 0;
      D_PostEvent(&event);
    }
  }
}

// These are just raw BGR8 bitmap images
extern const unsigned char _acbottom_off[];
extern const unsigned char _acbottom_on[];

//
// I_DrawBottomScreen
//
static void I_DrawBottomScreen (void)
{
  u8 *framebuf = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
  memcpy(framebuf, ctr_input_mode ? _acbottom_on : _acbottom_off, 240*320*3);
}

//
// I_StartTic
//

void I_StartTic (void)
{
  I_GetEvent();

  I_ReadMouse();

  I_PollJoystick();
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
}

//
// I_InitInputs
//

static void I_InitInputs(void)
{
  static Uint8 empty_cursor_data = 0;

  int nomouse_parm = M_CheckParm("-nomouse");

  // check if the user wants to use the mouse
  mouse_enabled = usemouse && !nomouse_parm;
  
  // SDL_PumpEvents();

  // Save the default cursor so it can be recalled later
  //cursors[0] = SDL_GetCursor();
  // Create an empty cursor
  //cursors[1] = SDL_CreateCursor(&empty_cursor_data, &empty_cursor_data, 8, 1, 0, 0);

  if (mouse_enabled)
  {
    MouseAccelChanging();
  }

  I_InitJoystick();
}
/////////////////////////////////////////////////////////////////////////////

// I_SkipFrame
//
// Returns true if it thinks we can afford to skip this frame

inline static dboolean I_SkipFrame(void)
{
  static int frameno;

  frameno++;
  switch (gamestate) {
  case GS_LEVEL:
    if (!paused)
      return false;
  default:
    // Skip odd frames
    return (frameno & 1) ? true : false;
  }
}

//////////////////////////////////////////////////////////////////////////////
// Graphics API

void I_SwapBuffers(void)
{
  gl_wrapper_swap_buffers();
}

void I_ShutdownGraphics(void)
{
  // SDL_FreeCursor(cursors[1]);
  DeactivateMouse();
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
}

static inline u16 argb1555_2_rgba5551(u16 x) {
  return (x << 1) | ((x & 0x8000) >> 15);
}

static inline u32 argb8_2_rgba8(u32 x) {
  return (x << 8) | ((x & 0xff000000) >> 24);
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
  //e6y: new mouse code
  UpdateGrab();

  I_DrawBottomScreen();

  // The screen wipe following pressing the exit switch on a level
  // is noticably jerkier with I_SkipFrame
  // if (I_SkipFrame())return;

#ifdef GL_DOOM
  if (V_GetMode() == VID_MODEGL) {
    // proff 04/05/2000: swap OpenGL buffers
    gld_Finish();
    return;
  }
#endif

  switch(V_GetMode()) {
  case VID_MODE15:
  {
    u16* dst_ptr = (u16*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL) + 239;
    u16* src_ptr = (u16*)screens[0].data;

    for(int y = 0; y < 240; y++) {
      for(int x = 0; x < 400; x++) {
        *dst_ptr = argb1555_2_rgba5551(*src_ptr);

        dst_ptr += 240;
        src_ptr++;
      }
      dst_ptr -= (400 * 240) + 1;
    }
    break;
  }
  case VID_MODE16:
  {
    u16* dst_ptr = (u16*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL) + 239;
    u16* src_ptr = (u16*)screens[0].data;

    for(int y = 0; y < 240; y++) {
      for(int x = 0; x < 400; x++) {
        *dst_ptr = *src_ptr;

        dst_ptr += 240;
        src_ptr++;
      }
      dst_ptr -= (400 * 240) + 1;
    }
    break;
  }
  default: // VID_MODE32
  {
    u32* dst_ptr = (u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL) + 239;
    u32* src_ptr = (u32*)screens[0].data;

    for(int y = 0; y < 240; y++) {
      for(int x = 0; x < 400; x++) {
        *dst_ptr = argb8_2_rgba8(*src_ptr);

        dst_ptr += 240;
        src_ptr++;
      }
      dst_ptr -= (400 * 240) + 1;
    }
    break;
  }
  }

  // Draw!

  // Flush and swap framebuffers
  gfxFlushBuffers();
  gfxSwapBuffers();

  gspWaitForVBlank();
}

static void I_ShutdownSDL(void)
{
  gfxExit();
}

void I_PreInitGraphics(void)
{
  gfxInitDefault();
  gfxSetDoubleBuffering(GFX_BOTTOM, false);

  I_AtExit(I_ShutdownSDL, true);
}

// e6y: resolution limitation is removed
static void I_InitBuffersRes(void)
{
  R_InitMeltRes();
  R_InitSpritesRes();
  R_InitBuffersRes();
  R_InitPlanesRes();
  R_InitVisplanesRes();
}

//
// I_GetScreenResolution
// Always set 400x240 for 3DS
//
static void I_GetScreenResolution(void)
{
  desired_screenwidth = 400;
  desired_screenheight = 240;
}

// e6y
// It is a simple test of CPU cache misses.
static unsigned int I_TestCPUCacheMisses(int width, int height, unsigned int mintime)
{
  int i, k;
  char *s, *d, *ps, *pd;
  unsigned int tickStart;
  
  s = (char*)malloc(width * height);
  d = (char*)malloc(width * height);

  tickStart = I_GetTime_MS();
  k = 0;
  do
  {
    ps = s;
    pd = d;
    for(i = 0; i < height; i++)
    {
      pd[0] = ps[0];
      pd += width;
      ps += width;
    }
    k++;
  }
  while (I_GetTime_MS() - tickStart < mintime);

  free(d);
  free(s);

  return k;
}

// CPhipps -
// I_CalculateRes
// Calculates the screen resolution, possibly using the supplied guide
static void I_CalculateRes(int width, int height)
{
// e6y
// GLBoom will try to set the closest supported resolution 
// if the requested mode can't be set correctly.
// For example glboom.exe -geom 1025x768 -nowindow will set 1024x768.
// It affects only fullscreen modes.
  if (V_GetMode() == VID_MODEGL) {
    SCREENWIDTH = width;
    SCREENHEIGHT = height;
    SCREENPITCH = SCREENWIDTH;
  }
  else {
    unsigned int count1, count2;
    int pitch1, pitch2;

    SCREENWIDTH = width; //(width+15) & ~15;
    SCREENHEIGHT = height;

    // e6y
    // Trying to optimise screen pitch for reducing of CPU cache misses.
    // It is extremally important for wiping in software.
    // I have ~20x improvement in speed with using 1056 instead of 1024 on Pentium4
    // and only ~10% for Core2Duo
    if (1)
    {
      unsigned int mintime = 100;
      int w = (width+15) & ~15;
      pitch1 = w * V_GetPixelDepth();
      pitch2 = w * V_GetPixelDepth() + 32;

      count1 = I_TestCPUCacheMisses(pitch1, SCREENHEIGHT, mintime);
      count2 = I_TestCPUCacheMisses(pitch2, SCREENHEIGHT, mintime);

      lprintf(LO_INFO, "I_CalculateRes: trying to optimize screen pitch\n");
      lprintf(LO_INFO, " test case for pitch=%d is processed %d times for %d msec\n", pitch1, count1, mintime);
      lprintf(LO_INFO, " test case for pitch=%d is processed %d times for %d msec\n", pitch2, count2, mintime);

      SCREENPITCH = (count2 > count1 ? pitch2 : pitch1);

      lprintf(LO_INFO, " optimized screen pitch is %d\n", SCREENPITCH);
    }
    else
    {
      SCREENPITCH = SCREENWIDTH * V_GetPixelDepth();
    }
  }
}

// CPhipps -
// I_InitScreenResolution
// Sets the screen resolution
void I_InitScreenResolution(void)
{
  int i, p, w, h;
  char c, x;
  video_mode_t mode;

  I_GetScreenResolution();

  if (!screen && !gl_wrapper_is_initialized())
  {
    // e6y
    // change the screen size for the current session only
    // syntax: -geom WidthxHeight[w|f]
    // examples: -geom 320x200f, -geom 640x480w, -geom 1024x768
    w = desired_screenwidth;
    h = desired_screenheight;

    if (!(p = M_CheckParm("-geom")))
      p = M_CheckParm("-geometry");

    if (p && p + 1 < myargc)
    {
      int count = sscanf(myargv[p+1], "%d%c%d%c", &w, &x, &h, &c);

      // at least width and height must be specified
      // restoring original values if not
      if (count < 3 || tolower(x) != 'x')
      {
        w = desired_screenwidth;
        h = desired_screenheight;
      }
    }
  }
  else
  {
    w = desired_screenwidth;
    h = desired_screenheight;
  }

  mode = (video_mode_t)I_GetModeFromString(default_videomode);
  if ((i=M_CheckParm("-vidmode")) && i<myargc-1)
  {
    mode = (video_mode_t)I_GetModeFromString(myargv[i+1]);
  }
#ifndef GL_DOOM
  if (mode == VID_MODEGL)
  {
    mode = (video_mode_t)I_GetModeFromString(default_videomode = "32bit");
  }
#endif
  
  V_InitMode(mode);

  I_CalculateRes(w, h);
  V_DestroyUnusedTrueColorPalettes();
  V_FreeScreens();

  // set first three to standard values
  for (i=0; i<3; i++) {
    screens[i].width = SCREENWIDTH;
    screens[i].height = SCREENHEIGHT;
    screens[i].byte_pitch = SCREENPITCH;
    screens[i].short_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE16);
    screens[i].int_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE32);
  }

  // statusbar
  screens[4].width = SCREENWIDTH;
  screens[4].height = SCREENHEIGHT;
  screens[4].byte_pitch = SCREENPITCH;
  screens[4].short_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE16);
  screens[4].int_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE32);

  I_InitBuffersRes();

  lprintf(LO_INFO,"I_InitScreenResolution: Using resolution %dx%d\n", SCREENWIDTH, SCREENHEIGHT);
}

void I_InitGraphics(void)
{
  static int    firsttime=1;

  if (firsttime)
  {
    firsttime = 0;

    I_AtExit(I_ShutdownGraphics, true);
    lprintf(LO_INFO, "I_InitGraphics: %dx%d\n", SCREENWIDTH, SCREENHEIGHT);

    /* Set the video mode */
    I_UpdateVideoMode();

    /* Initialize the input system */
    I_InitInputs();

    //e6y: new mouse code
    UpdateGrab();
  }
}

video_mode_t I_GetModeFromString(const char *modestr)
{
  video_mode_t mode;

  if (!strcasecmp(modestr,"15")) {
    mode = VID_MODE15;
  } else if (!strcasecmp(modestr,"15bit")) {
    mode = VID_MODE15;
  } else if (!strcasecmp(modestr,"16")) {
    mode = VID_MODE16;
  } else if (!strcasecmp(modestr,"16bit")) {
    mode = VID_MODE16;
  } else if (!strcasecmp(modestr,"gl")) {
    mode = VID_MODEGL;
  } else if (!strcasecmp(modestr,"OpenGL")) {
    mode = VID_MODEGL;
  } else {
    mode = VID_MODE32;
  }

  return mode;
}

void I_UpdateVideoMode(void)
{
  if (V_GetMode() != VID_MODEGL)
  {
    GSPGPU_FramebufferFormat top_fmt;
    int screen_pitch;

    switch(V_GetNumPixelBits())
    {
      case 15:
        top_fmt = GSP_RGB5_A1_OES;
        screen_pitch = 400 * 2;
        break;
      case 16:
        top_fmt = GSP_RGB565_OES;
        screen_pitch = 400 * 2;
        break;
      default:
        top_fmt = GSP_RGBA8_OES;
        screen_pitch = 400 * 4;
        break;
    }

    gfxSetScreenFormat(GFX_TOP, top_fmt);
    gfxSet3D(false);

    screen = malloc(screen_pitch * 240);

    screens[0].not_on_heap = true;
    screens[0].data = (unsigned char *) (screen);
    screens[0].byte_pitch = screen_pitch;
    screens[0].short_pitch = screen_pitch / V_GetModePixelDepth(VID_MODE16);
    screens[0].int_pitch = screen_pitch / V_GetModePixelDepth(VID_MODE32);

    V_AllocScreens();

    R_InitBuffer(SCREENWIDTH, SCREENHEIGHT);
  }
  else
  {
    gfxSetScreenFormat(GFX_TOP, GSP_RGBA8_OES);
    gfxSet3D(true);

    gl_wrapper_init();
  }

  // e6y: wide-res
  // Need some initialisations before level precache
  R_ExecuteSetViewSize();

  V_SetPalette(0);

  ST_SetResolution();
  AM_SetResolution();

#ifdef GL_DOOM
  if (V_GetMode() == VID_MODEGL)
  {
    gld_Init(SCREENWIDTH, SCREENHEIGHT);

    M_ChangeFOV();
    deh_changeCompTranslucency();
  }
#endif
}

static void ActivateMouse(void)
{
  //SDL_WM_GrabInput(SDL_GRAB_ON);
  //SDL_ShowCursor(SDL_DISABLE);
  //SDL_GetRelativeMouseState(NULL, NULL);
}

static void DeactivateMouse(void)
{
  //SDL_WM_GrabInput(SDL_GRAB_OFF);
  //SDL_ShowCursor(SDL_ENABLE);
}

//
// Read the change in mouse state to generate mouse motion events
//
// This is to combine all mouse movement for a tic into one mouse
// motion event.

static void SmoothMouse(int* x, int* y)
{
    static int x_remainder_old = 0;
    static int y_remainder_old = 0;

    int x_remainder, y_remainder;
    fixed_t correction_factor;

    const fixed_t fractic = I_TickElapsedTime();

    *x += x_remainder_old;
    *y += y_remainder_old;

    correction_factor = FixedDiv(fractic, FRACUNIT + fractic);

    x_remainder = FixedMul(*x, correction_factor);
    *x -= x_remainder;
    x_remainder_old = x_remainder;

    y_remainder = FixedMul(*y, correction_factor);
    *y -= y_remainder;
    y_remainder_old = y_remainder;
}

static void I_ReadMouse(void)
{
  if (mouse_enabled && is_ctr_touch_down)
  {
    touchPosition touch;
    hidTouchRead(&touch);

    int x = (int)touch.px - ctr_mouse_pos[0];
    int y = (int)touch.py - ctr_mouse_pos[1];

    ctr_mouse_pos[0] = touch.px;
    ctr_mouse_pos[1] = touch.py;

    SmoothMouse(&x, &y);

    if (x != 0 || y != 0)
    {
      event_t event;
      event.type = ev_mousemotion;
      event.data1 = 0;
      event.data2 = x << 6;
      event.data3 = -y << 6;

      D_PostEvent(&event);
    }
  }

  if (!usemouse)
    return;

  if (!MouseShouldBeGrabbed())
  {
    mouse_currently_grabbed = false;
    return;
  }

  if (!mouse_currently_grabbed)
  {
    mouse_currently_grabbed = true;
  }
}

static dboolean MouseShouldBeGrabbed()
{
  // if we specify not to grab the mouse, never grab
  if (!mouse_enabled)
    return false;

  // always grab the mouse in camera mode when playing levels 
  // and menu is not active
  if (walkcamera.type)
    return (demoplayback && gamestate == GS_LEVEL && !menuactive);

  // when menu is active or game is paused, release the mouse 
  if (menuactive || paused)
    return false;

  // only grab mouse when playing levels (but not demos)
  return (gamestate == GS_LEVEL) && !demoplayback;
}

void UpdateGrab(void)
{
  static dboolean currently_grabbed = false;
  dboolean grab;

  grab = MouseShouldBeGrabbed();

  if (grab && !currently_grabbed)
  {
    ActivateMouse();
  }

  if (!grab && currently_grabbed)
  {
    DeactivateMouse();
  }

  currently_grabbed = grab;
}
