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

//e6y: new mouse code
static SDL_Cursor* cursors[2] = {NULL, NULL};

static int mouse_currently_grabbed = true;

static void ActivateMouse(void);
static void DeactivateMouse(void);
//static int AccelerateMouse(int val);
static void I_ReadMouse(void);
static dboolean MouseShouldBeGrabbed();

extern void M_QuitDOOM(int choice);

int vanilla_keymap;
static SDL_Surface *screen;

////////////////////////////////////////////////////////////////////////////
// Input code
static int             leds_always_off = 0; // Expected by m_misc, not relevant

// Mouse handling
extern int     usemouse;        // config file var
static dboolean mouse_enabled; // usemouse, but can be overriden by -nomouse

video_mode_t I_GetModeFromString(const char *modestr);

/////////////////////////////////////////////////////////////////////////////////
// Keyboard handling

//
//  Translates the key currently in key
//
static int I_TranslateKey(SDL_keysym* key)
{
  int rc = 0;

  switch (key->sym) {
  case SDLK_LEFT: rc = KEYD_LEFTARROW;  break;
  case SDLK_RIGHT:  rc = KEYD_RIGHTARROW; break;
  case SDLK_DOWN: rc = KEYD_DOWNARROW;  break;
  case SDLK_UP:   rc = KEYD_UPARROW;  break;
  case SDLK_ESCAPE: rc = KEYD_ESCAPE; break;
  case SDLK_RETURN: rc = KEYD_ENTER;  break;
  case SDLK_TAB:  rc = KEYD_TAB;    break;
  case SDLK_F1:   rc = KEYD_F1;   break;
  case SDLK_F2:   rc = KEYD_F2;   break;
  case SDLK_F3:   rc = KEYD_F3;   break;
  case SDLK_F4:   rc = KEYD_F4;   break;
  case SDLK_F5:   rc = KEYD_F5;   break;
  case SDLK_F6:   rc = KEYD_F6;   break;
  case SDLK_F7:   rc = KEYD_F7;   break;
  case SDLK_F8:   rc = KEYD_F8;   break;
  case SDLK_F9:   rc = KEYD_F9;   break;
  case SDLK_F10:  rc = KEYD_F10;    break;
  case SDLK_F11:  rc = KEYD_F11;    break;
  case SDLK_F12:  rc = KEYD_F12;    break;
  case SDLK_BACKSPACE:  rc = KEYD_BACKSPACE;  break;
  case SDLK_DELETE: rc = KEYD_DEL;  break;
  case SDLK_INSERT: rc = KEYD_INSERT; break;
  case SDLK_PAGEUP: rc = KEYD_PAGEUP; break;
  case SDLK_PAGEDOWN: rc = KEYD_PAGEDOWN; break;
  case SDLK_HOME: rc = KEYD_HOME; break;
  case SDLK_END:  rc = KEYD_END;  break;
  case SDLK_PAUSE:  rc = KEYD_PAUSE;  break;
  case SDLK_EQUALS: rc = KEYD_EQUALS; break;
  case SDLK_MINUS:  rc = KEYD_MINUS;  break;
  case SDLK_KP0:  rc = KEYD_KEYPAD0;  break;
  case SDLK_KP1:  rc = KEYD_KEYPAD1;  break;
  case SDLK_KP2:  rc = KEYD_KEYPAD2;  break;
  case SDLK_KP3:  rc = KEYD_KEYPAD3;  break;
  case SDLK_KP4:  rc = KEYD_KEYPAD4;  break;
  case SDLK_KP5:  rc = KEYD_KEYPAD5;  break;
  case SDLK_KP6:  rc = KEYD_KEYPAD6;  break;
  case SDLK_KP7:  rc = KEYD_KEYPAD7;  break;
  case SDLK_KP8:  rc = KEYD_KEYPAD8;  break;
  case SDLK_KP9:  rc = KEYD_KEYPAD9;  break;
  case SDLK_KP_PLUS:  rc = KEYD_KEYPADPLUS; break;
  case SDLK_KP_MINUS: rc = KEYD_KEYPADMINUS;  break;
  case SDLK_KP_DIVIDE:  rc = KEYD_KEYPADDIVIDE; break;
  case SDLK_KP_MULTIPLY: rc = KEYD_KEYPADMULTIPLY; break;
  case SDLK_KP_ENTER: rc = KEYD_KEYPADENTER;  break;
  case SDLK_KP_PERIOD:  rc = KEYD_KEYPADPERIOD; break;
  case SDLK_LSHIFT:
  case SDLK_RSHIFT: rc = KEYD_RSHIFT; break;
  case SDLK_LCTRL:
  case SDLK_RCTRL:  rc = KEYD_RCTRL;  break;
  case SDLK_LALT:
  case SDLK_LSUPER:
  case SDLK_RALT:
  case SDLK_RSUPER:  rc = KEYD_RALT;   break;
  case SDLK_CAPSLOCK: rc = KEYD_CAPSLOCK; break;
  case SDLK_PRINT: rc = KEYD_PRINTSC; break;
  case SDLK_SCROLLOCK: rc = KEYD_SCROLLLOCK; break;
  default:    rc = key->sym;    break;
  }

  return rc;

}

/////////////////////////////////////////////////////////////////////////////////
// Main input code

/* cph - pulled out common button code logic */
//e6y static 
static int I_SDLtoDoomMouseState(Uint32 buttonstate)
{
  return 0
      | (buttonstate & SDL_BUTTON(1) ? 1 : 0)
      | (buttonstate & SDL_BUTTON(2) ? 2 : 0)
      | (buttonstate & SDL_BUTTON(3) ? 4 : 0)
      | (buttonstate & SDL_BUTTON(6) ? 8 : 0)
      | (buttonstate & SDL_BUTTON(7) ? 16 : 0)
      | (buttonstate & SDL_BUTTON(4) ? 32 : 0)
      | (buttonstate & SDL_BUTTON(5) ? 64 : 0)
      | (buttonstate & SDL_BUTTON(8) ? 128 : 0)
      ;
}

static void I_GetEvent(void)
{
  event_t event;

  SDL_Event SDLEvent;
  SDL_Event *Event = &SDLEvent;

  static int mwheeluptic = 0, mwheeldowntic = 0;

  while (SDL_PollEvent(Event))
  {
    switch (Event->type) {
    case SDL_KEYDOWN:
      if (Event->key.keysym.mod & KMOD_LALT)
      {
        // Prevent executing action on Alt-Tab
        if (Event->key.keysym.sym == SDLK_TAB)
        {
          break;
        }
        // Immediately exit on Alt+F4 ("Boss Key")
        else if (Event->key.keysym.sym == SDLK_F4)
        {
          I_SafeExit(0);
          break;
        }
      }
      event.type = ev_keydown;
      event.data1 = I_TranslateKey(&Event->key.keysym);
      D_PostEvent(&event);
      break;

    case SDL_KEYUP:
    {
      event.type = ev_keyup;
      event.data1 = I_TranslateKey(&Event->key.keysym);
      D_PostEvent(&event);
    }
    break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    if (mouse_enabled)
    {
      if (Event->button.button == SDL_BUTTON_WHEELUP)
      {
        event.type = ev_keydown;
        event.data1 = KEYD_MWHEELUP;
        mwheeluptic = gametic;
        D_PostEvent(&event);
      }
      else if (Event->button.button == SDL_BUTTON_WHEELDOWN)
      {
        event.type = ev_keydown;
        event.data1 = KEYD_MWHEELDOWN;
        mwheeldowntic = gametic;
        D_PostEvent(&event);
      }
      else
      {
        event.type = ev_mouse;
        event.data1 = I_SDLtoDoomMouseState(SDL_GetMouseState(NULL, NULL));
        event.data2 = event.data3 = 0;
        D_PostEvent(&event);
      }
    }
    break;

    case SDL_QUIT:
      S_StartSound(NULL, sfx_swtchn);
      M_QuitDOOM(0);

    default:
      break;
    }
  }

  if(mwheeluptic && mwheeluptic + 1 < gametic)
  {
    event.type = ev_keyup;
    event.data1 = KEYD_MWHEELUP;
    D_PostEvent(&event);
    mwheeluptic = 0;
  }

  if(mwheeldowntic && mwheeldowntic + 1 < gametic)
  {
    event.type = ev_keyup;
    event.data1 = KEYD_MWHEELDOWN;
    D_PostEvent(&event);
    mwheeldowntic = 0;
  }
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
  
  SDL_PumpEvents();

  // Save the default cursor so it can be recalled later
  cursors[0] = SDL_GetCursor();
  // Create an empty cursor
  cursors[1] = SDL_CreateCursor(&empty_cursor_data, &empty_cursor_data, 8, 1, 0, 0);

  if (mouse_enabled)
  {
    MouseAccelChanging();
  }

  I_InitJoystick();
}

//////////////////////////////////////////////////////////////////////////////
// Graphics API

void I_SwapBuffers(void)
{
  SDL_GL_SwapBuffers();
}

void I_ShutdownGraphics(void)
{
  SDL_FreeCursor(cursors[1]);
  DeactivateMouse();
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
  //e6y: new mouse code
  UpdateGrab();

#ifdef GL_DOOM
  if (V_GetMode() == VID_MODEGL) {
    // proff 04/05/2000: swap OpenGL buffers
    gld_Finish();
    return;
  }
#endif

  if (SDL_MUSTLOCK(screen)) {
      int h;
      byte *src;
      byte *dest;

      if (SDL_LockSurface(screen) < 0) {
        lprintf(LO_INFO,"I_FinishUpdate: %s\n", SDL_GetError());
        return;
      }

      dest=(byte*)screen->pixels;
      src=screens[0].data;
      h=screen->h;
      for (; h>0; h--)
      {
        memcpy(dest,src,SCREENWIDTH*V_GetPixelDepth()); //e6y
        dest+=screen->pitch;
        src+=screens[0].byte_pitch;
      }

      SDL_UnlockSurface(screen);
  }

  // Draw!
  SDL_Flip(screen);
}

// I_PreInitGraphics

static void I_ShutdownSDL(void)
{
  if (screen) SDL_FreeSurface(screen);

  SDL_Quit();
  return;
}

void I_PreInitGraphics(void)
{
  int p;

  // Initialize SDL
  p = SDL_InitSubSystem(SDL_INIT_VIDEO);
  if (p < 0)
  {
    I_Error("Could not initialize SDL [%s]", SDL_GetError());
  }

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
  int init = (screen == NULL);

  I_GetScreenResolution();

  if (init)
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
  int init_flags = 0;

  if(screen)
  {
#ifdef GL_DOOM
    if (V_GetMode() == VID_MODEGL)
    {
      gld_CleanMemory();
      // hires patches
      gld_CleanStaticMemory();
    }
#endif

    I_InitScreenResolution();

    if (screen) SDL_FreeSurface(screen);

    screen = NULL;
  }

#ifdef GL_DOOM
  if (V_GetMode() == VID_MODEGL)
  {
    init_flags |= SDL_OPENGL;
  }
#endif

  // Set window title
  SDL_WM_SetCaption(PACKAGE_NAME " " PACKAGE_VERSION, "game");

  screen = SDL_SetVideoMode(SCREENWIDTH, SCREENHEIGHT, V_GetNumPixelBits(), init_flags);

  if(screen == NULL) {
    I_Error("Couldn't set %dx%d video mode [%s]", SCREENWIDTH, SCREENHEIGHT, SDL_GetError());
  }

  if (V_GetMode() != VID_MODEGL)
  {
    lprintf(LO_INFO, "I_UpdateVideoMode: 0x%x, %s, %s\n", init_flags, screen && screen->pixels ? "SDL buffer" : "own buffer", screen && SDL_MUSTLOCK(screen) ? "lock-and-copy": "direct access");

    // Get the info needed to render to the display
    if (!SDL_MUSTLOCK(screen))
    {
      screens[0].not_on_heap = true;
      screens[0].data = (unsigned char *) (screen->pixels);
      screens[0].byte_pitch = screen->pitch;
      screens[0].short_pitch = screen->pitch / V_GetModePixelDepth(VID_MODE16);
      screens[0].int_pitch = screen->pitch / V_GetModePixelDepth(VID_MODE32);
    }
    else
    {
      screens[0].not_on_heap = false;
    }

    V_AllocScreens();

    R_InitBuffer(SCREENWIDTH, SCREENHEIGHT);
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
  SDL_WM_GrabInput(SDL_GRAB_ON);
  SDL_ShowCursor(SDL_DISABLE);
  SDL_GetRelativeMouseState(NULL, NULL);
}

static void DeactivateMouse(void)
{
  SDL_WM_GrabInput(SDL_GRAB_OFF);
  SDL_ShowCursor(SDL_ENABLE);
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
  if (mouse_enabled)
  {
    int x, y;

    SDL_GetRelativeMouseState(&x, &y);
    SmoothMouse(&x, &y);

    if (x != 0 || y != 0)
    {
      event_t event;
      event.type = ev_mousemotion;
      event.data1 = 0;
      event.data2 = x << 4;
      event.data3 = -y << 4;

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
