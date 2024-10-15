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
 *
 *---------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl_opengl.h"

#ifdef _MSC_VER
//#include <ddraw.h> /* needed for DirectX's DDSURFACEDESC2 structure definition */
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>
#include "doomstat.h"
#include "v_video.h"
#include "gl_intern.h"
#include "i_system.h"
#include "w_wad.h"
#include "lprintf.h"
#include "i_video.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "r_main.h"
#include "r_sky.h"
#include "m_argv.h"
#include "m_misc.h"
#include "e6y.h"

#include "m_io.h"

#ifdef GL_DOOM

unsigned int gl_has_hires = 0;
int gl_texture_external_hires = -1;
int gl_texture_internal_hires = -1;
int gl_hires_override_pwads;
const char *gl_texture_hires_dir = NULL;
int gl_hires_24bit_colormap = false;

static GLuint progress_texid = 0;
static unsigned int lastupdate = 0;

int gld_ProgressStart(void)
{
  if (!progress_texid)
  {
    progress_texid = CaptureScreenAsTexID();
    lastupdate = I_GetTime_MS() - 100;
    return true;
  }

  return false;
}

int gld_ProgressRestoreScreen(void)
{
  int total_w, total_h;
  float fU1, fU2, fV1, fV2;

  if (progress_texid)
  {
    total_w = gld_GetTexDimension(SCREENWIDTH);
    total_h = gld_GetTexDimension(SCREENHEIGHT);
    
    fU1 = 0.0f;
    fV1 = (float)SCREENHEIGHT / (float)total_h;
    fU2 = (float)SCREENWIDTH / (float)total_w;
    fV2 = 0.0f;
    
    gld_EnableTexture2D(true);
    
    glBindTexture(GL_TEXTURE_2D, progress_texid);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    glBegin(GL_TRIANGLE_STRIP);
    {
      glTexCoord2f(fU1, fV1); glVertex2f(0.0f, 0.0f);
      glTexCoord2f(fU1, fV2); glVertex2f(0.0f, (float)SCREENHEIGHT);
      glTexCoord2f(fU2, fV1); glVertex2f((float)SCREENWIDTH, 0.0f);
      glTexCoord2f(fU2, fV2); glVertex2f((float)SCREENWIDTH, (float)SCREENHEIGHT);
    }
    glEnd();

    return true;
  }

  return false;
}

int gld_ProgressEnd(void)
{
  if (progress_texid != 0)
  {
    gld_ProgressRestoreScreen();
    I_FinishUpdate();
    gld_ProgressRestoreScreen();
    glDeleteTextures(1, &progress_texid);
    progress_texid = 0;
    return true;
  }

  return false;
}

void gld_ProgressUpdate(const char * text, int progress, int total)
{
  int len;
  static char last_text[32] = {0};
  unsigned int tic;

  if (!progress_texid)
    return;

  // do not do it often
  tic = I_GetTime_MS();
  if (tic - lastupdate < 100)
    return;
  lastupdate = tic;

  if ((text) && (strlen(text) > 0) && strcmp((last_text[0] ? last_text : ""), text))
  {
    const char *s;
    strcpy(last_text, text);

    if (!w_precache.f)
      HU_Start();

    HUlib_clearTextLine(&w_precache);
    s = text;
    while (*s)
      HUlib_addCharToTextLine(&w_precache, *(s++));
    HUlib_setTextXCenter(&w_precache);
  }

  gld_ProgressRestoreScreen();
  HUlib_drawTextLine(&w_precache, false);
  
  len = MIN(SCREENWIDTH, (int)((int_64_t)SCREENWIDTH * progress / total));
  V_FillRect(0, 0, SCREENHEIGHT - 4, len - 0, 4, 4);
  if (len > 4)
  {
    V_FillRect(0, 2, SCREENHEIGHT - 3, len - 4, 2, 31);
  }

  I_FinishUpdate();
}

#endif
