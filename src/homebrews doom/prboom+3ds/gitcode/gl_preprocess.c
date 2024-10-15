/* Emacs style mode select   -*- C++ -*- 
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
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

#include <math.h>

#include "gl_opengl.h"

#include "z_zone.h"
#include "doomstat.h"
#include "gl_intern.h"
#include "gl_struct.h"
#include "p_maputl.h"
#include "r_main.h"
#include "am_map.h"
#include "lprintf.h"

#include "m_io.h"

#ifdef GL_DOOM

static FILE *levelinfo;

static int gld_max_vertexes = 0;
static int gld_num_vertexes = 0;

static int triangulate_subsectors = 0;

// this is the list for all sectors to the loops
GLSector *sectorloops;

// this is the list for all subsectors to the loops
// uses by textured automap
GLMapSubsector *subsectorloops;

static void gld_AddGlobalVertexes(int count)
{
  if ((gld_num_vertexes+count)>=gld_max_vertexes)
  {
    gld_max_vertexes+=count+1024;
    flats_vbo = Z_Realloc(flats_vbo, gld_max_vertexes * sizeof(flats_vbo[0]), PU_STATIC, 0);
  }
}

/*****************************
 *
 * FLATS
 *
 *****************************/

/* proff - 05/15/2000
 * The idea and algorithm to compute the flats with nodes and subsectors is
 * originaly from JHexen. I have redone it.
 */

#define FIX2DBL(x)    ((double)(x))
#define MAX_CC_SIDES  128

static dboolean gld_PointOnSide(vertex_t *p, divline_t *d)
{
  // We'll return false if the point c is on the left side.
  return ((FIX2DBL(d->y)-FIX2DBL(p->y))*FIX2DBL(d->dx)-(FIX2DBL(d->x)-FIX2DBL(p->x))*FIX2DBL(d->dy) >= 0);
}

// Lines start-end and fdiv must intersect.
static void gld_CalcIntersectionVertex(vertex_t *s, vertex_t *e, divline_t *d, vertex_t *i)
{
  double ax = FIX2DBL(s->x), ay = FIX2DBL(s->y), bx = FIX2DBL(e->x), by = FIX2DBL(e->y);
  double cx = FIX2DBL(d->x), cy = FIX2DBL(d->y), dx = cx+FIX2DBL(d->dx), dy = cy+FIX2DBL(d->dy);
  double r = ((ay-cy)*(dx-cx)-(ax-cx)*(dy-cy)) / ((bx-ax)*(dy-cy)-(by-ay)*(dx-cx));
  i->x = (fixed_t)((double)s->x + r*((double)e->x-(double)s->x));
  i->y = (fixed_t)((double)s->y + r*((double)e->y-(double)s->y));
}

#undef FIX2DBL

// Returns a pointer to the list of points. It must be used.
//
static vertex_t *gld_FlatEdgeClipper(int *numpoints, vertex_t *points, int numclippers, divline_t *clippers)
{
  unsigned char sidelist[MAX_CC_SIDES];
  int       i, k, num = *numpoints;

  // We'll clip the polygon with each of the divlines. The left side of
  // each divline is discarded.
  for(i=0; i<numclippers; i++)
  {
    divline_t *curclip = &clippers[i];

    // First we'll determine the side of each vertex. Points are allowed
    // to be on the line.
    for(k=0; k<num; k++)
      sidelist[k] = gld_PointOnSide(&points[k], curclip);

    for(k=0; k<num; k++)
    {
      int startIdx = k, endIdx = k+1;
      // Check the end index.
      if(endIdx == num) endIdx = 0; // Wrap-around.
      // Clipping will happen when the ends are on different sides.
      if(sidelist[startIdx] != sidelist[endIdx])
      {
        vertex_t newvert;

        gld_CalcIntersectionVertex(&points[startIdx], &points[endIdx], curclip, &newvert);

        // Add the new vertex. Also modify the sidelist.
        points = (vertex_t*)Z_Realloc(points,(++num)*sizeof(vertex_t),PU_LEVEL,0);
        if(num >= MAX_CC_SIDES)
          I_Error("gld_FlatEdgeClipper: Too many points in carver");

        // Make room for the new vertex.
        memmove(&points[endIdx+1], &points[endIdx],
          (num - endIdx-1)*sizeof(vertex_t));
        memcpy(&points[endIdx], &newvert, sizeof(newvert));

        memmove(&sidelist[endIdx+1], &sidelist[endIdx], num-endIdx-1);
        sidelist[endIdx] = 1;

        // Skip over the new vertex.
        k++;
      }
    }

    // Now we must discard the points that are on the wrong side.
    for(k=0; k<num; k++)
      if(!sidelist[k])
      {
        memmove(&points[k], &points[k+1], (num - k-1)*sizeof(vertex_t));
        memmove(&sidelist[k], &sidelist[k+1], num - k-1);
        num--;
        k--;
      }
  }
  // Screen out consecutive identical points.
  for(i=0; i<num; i++)
  {
    int previdx = i-1;
    if(previdx < 0) previdx = num - 1;
    if(points[i].x == points[previdx].x
      && points[i].y == points[previdx].y)
    {
      // This point (i) must be removed.
      memmove(&points[i], &points[i+1], sizeof(vertex_t)*(num-i-1));
      num--;
      i--;
    }
  }
  *numpoints = num;
  return points;
}

static void gld_FlatConvexCarver(int ssidx, int num, divline_t *list)
{
  subsector_t *ssec=&subsectors[ssidx];
  int numclippers = num+ssec->numlines;
  divline_t *clippers;
  int i, numedgepoints;
  vertex_t *edgepoints;

  clippers=(divline_t*)Z_Malloc(numclippers*sizeof(divline_t),PU_LEVEL,0);
  if (!clippers)
    return;
  for(i=0; i<num; i++)
  {
    clippers[i].x = list[num-i-1].x;
    clippers[i].y = list[num-i-1].y;
    clippers[i].dx = list[num-i-1].dx;
    clippers[i].dy = list[num-i-1].dy;
  }
  for(i=num; i<numclippers; i++)
  {
    seg_t *seg = &segs[ssec->firstline+i-num];
    clippers[i].x = seg->v1->x;
    clippers[i].y = seg->v1->y;
    clippers[i].dx = seg->v2->x-seg->v1->x;
    clippers[i].dy = seg->v2->y-seg->v1->y;
  }

  // Setup the 'worldwide' polygon.
  numedgepoints = 4;
  edgepoints = (vertex_t*)Z_Malloc(numedgepoints*sizeof(vertex_t),PU_LEVEL,0);

  edgepoints[0].x = INT_MIN;
  edgepoints[0].y = INT_MAX;

  edgepoints[1].x = INT_MAX;
  edgepoints[1].y = INT_MAX;

  edgepoints[2].x = INT_MAX;
  edgepoints[2].y = INT_MIN;

  edgepoints[3].x = INT_MIN;
  edgepoints[3].y = INT_MIN;

  // Do some clipping, <snip> <snip>
  edgepoints = gld_FlatEdgeClipper(&numedgepoints, edgepoints, numclippers, clippers);

  if(!numedgepoints)
  {
    if (levelinfo) fprintf(levelinfo, "All carved away: subsector %li - sector %i\n", ssec-subsectors, ssec->sector->iSectorID);
  }
  else
  {
    if(numedgepoints >= 3)
    {
      gld_AddGlobalVertexes(numedgepoints);
      if (flats_vbo)
      {
        int currentsector=ssec->sector->iSectorID;
        GLLoopDef **loop;
        int *loopcount;

        if (triangulate_subsectors)
        {
          loop = &subsectorloops[ ssidx ].loops;
          loopcount = &subsectorloops[ ssidx ].loopcount;
        }
        else
        {
          loop = &sectorloops[ currentsector ].loops;
          loopcount = &sectorloops[ currentsector ].loopcount;
        }

        (*loopcount)++;
        (*loop) = Z_Realloc((*loop), sizeof(GLLoopDef)*(*loopcount), PU_STATIC, 0);
        ((*loop)[(*loopcount) - 1]).index = ssidx;
        ((*loop)[(*loopcount) - 1]).mode = GL_TRIANGLE_FAN;
        ((*loop)[(*loopcount) - 1]).vertexcount = numedgepoints;
        ((*loop)[(*loopcount) - 1]).vertexindex = gld_num_vertexes;

        for(i = 0;  i < numedgepoints; i++)
        {
          flats_vbo[gld_num_vertexes].u = ( (float)edgepoints[i].x/(float)FRACUNIT)/64.0f;
          flats_vbo[gld_num_vertexes].v = (-(float)edgepoints[i].y/(float)FRACUNIT)/64.0f;
          flats_vbo[gld_num_vertexes].x = -(float)edgepoints[i].x/MAP_SCALE;
          flats_vbo[gld_num_vertexes].y = 0.0f;
          flats_vbo[gld_num_vertexes].z =  (float)edgepoints[i].y/MAP_SCALE;
          gld_num_vertexes++;
        }
      }
    }
  }
  // We're done, free the edgepoints memory.
  Z_Free(edgepoints);
  Z_Free(clippers);
}

static void gld_CarveFlats(int bspnode, int numdivlines, divline_t *divlines)
{
  node_t    *nod;
  divline_t *childlist, *dl;
  int     childlistsize = numdivlines+1;

  // If this is a subsector we are dealing with, begin carving with the
  // given list.
  if(bspnode & NF_SUBSECTOR)
  {
    // We have arrived at a subsector. The divline list contains all
    // the partition lines that carve out the subsector.
    // special case for trivial maps (no nodes, single subsector)
    int ssidx = (numnodes != 0) ? bspnode & (~NF_SUBSECTOR) : 0;

    if (!(subsectors[ssidx].sector->flags & SECTOR_IS_CLOSED) || triangulate_subsectors)
      gld_FlatConvexCarver(ssidx, numdivlines, divlines);
    return;
  }

  // Get a pointer to the node.
  nod = nodes + bspnode;

  // Allocate a new list for each child.
  childlist = (divline_t*)Z_Malloc(childlistsize*sizeof(divline_t),PU_LEVEL,0);

  // Copy the previous lines.
  if(divlines) memcpy(childlist,divlines,numdivlines*sizeof(divline_t));

  dl = childlist + numdivlines;
  dl->x = nod->x;
  dl->y = nod->y;
  // The right child gets the original line (LEFT side clipped).
  dl->dx = nod->dx;
  dl->dy = nod->dy;
  gld_CarveFlats(nod->children[0],childlistsize,childlist);

  // The left side. We must reverse the line, otherwise the wrong
  // side would get clipped.
  dl->dx = -nod->dx;
  dl->dy = -nod->dy;
  gld_CarveFlats(nod->children[1],childlistsize,childlist);

  // We are finishing with this node, free the allocated list.
  Z_Free(childlist);
}

/********************************************
 * Name     : gld_GetSubSectorVertices      *
 * created  : 08/13/00                      *
 * modified : 09/18/00, adapted for PrBoom  *
 * author   : figgi                         *
 * what     : prepares subsectorvertices    *
 *            (glnodes only)                *
 ********************************************/

static void gld_GetSubSectorVertices(void)
{
  int      i, j;
  int      numedgepoints;
  subsector_t* ssector;

  for(i = 0; i < numsubsectors; i++)
  {
    ssector = &subsectors[i];

    if ((ssector->sector->flags & SECTOR_IS_CLOSED) && !triangulate_subsectors)
      continue;

    numedgepoints  = ssector->numlines;

    gld_AddGlobalVertexes(numedgepoints);

    if (flats_vbo)
    {
      int currentsector = ssector->sector->iSectorID;
      GLLoopDef **loop;
      int *loopcount;

      if (triangulate_subsectors)
      {
        loop = &subsectorloops[ i ].loops;
        loopcount = &subsectorloops[ i ].loopcount;
      }
      else
      {
        loop = &sectorloops[ currentsector ].loops;
        loopcount = &sectorloops[ currentsector ].loopcount;
      }

      (*loopcount)++;
      (*loop) = Z_Realloc((*loop), sizeof(GLLoopDef)*(*loopcount), PU_STATIC, 0);
      ((*loop)[(*loopcount) - 1]).index = i;
      ((*loop)[(*loopcount) - 1]).mode = GL_TRIANGLE_FAN;
      ((*loop)[(*loopcount) - 1]).vertexcount = numedgepoints;
      ((*loop)[(*loopcount) - 1]).vertexindex = gld_num_vertexes;

      for(j = 0;  j < numedgepoints; j++)
      {
        flats_vbo[gld_num_vertexes].u =( (float)(segs[ssector->firstline + j].v1->x)/FRACUNIT)/64.0f;
        flats_vbo[gld_num_vertexes].v =(-(float)(segs[ssector->firstline + j].v1->y)/FRACUNIT)/64.0f;
        flats_vbo[gld_num_vertexes].x = -(float)(segs[ssector->firstline + j].v1->x)/MAP_SCALE;
        flats_vbo[gld_num_vertexes].y = 0.0f;
        flats_vbo[gld_num_vertexes].z =  (float)(segs[ssector->firstline + j].v1->y)/MAP_SCALE;
        gld_num_vertexes++;
      }
    }
  }
}

// gld_PreprocessLevel
//
// this checks all sectors if they are closed and calls gld_PrecalculateSector to
// calculate the loops for every sector
// the idea to check for closed sectors is from DEU. check next commentary
/*
      Note from RQ:
      This is a very simple idea, but it works!  The first test (above)
      checks that all Sectors are closed.  But if a closed set of LineDefs
      is moved out of a Sector and has all its "external" SideDefs pointing
      to that Sector instead of the new one, then we need a second test.
      That's why I check if the SideDefs facing each other are bound to
      the same Sector.

      Other note from RQ:
      Nowadays, what makes the power of a good editor is its automatic tests.
      So, if you are writing another Doom editor, you will probably want
      to do the same kind of tests in your program.  Fine, but if you use
      these ideas, don't forget to credit DEU...  Just a reminder... :-)
*/
// so I credited DEU

//
// e6y
// All sectors which are inside 64x64 square of map grid should be marked here.
// GL_CLAMP instead of GL_REPEAT should be used for them for avoiding seams
//

static void gld_MarkSectorsForClamp(void)
{
  int i;

  for (i = 0; i < numsectors; i++)
  {
    int loopnum; // current loop number
    GLLoopDef *currentloop; // the current loop
    float minu, maxu, minv, maxv;
    dboolean fail;

    minu = minv = 65535;
    maxu = maxv = -65535;
    fail = false;

    for (loopnum = 0; !fail && loopnum < sectorloops[i].loopcount; loopnum++)
    {
      int vertexnum;
      // set the current loop
      currentloop = &sectorloops[i].loops[loopnum];
      if (!currentloop)
        continue;
      for (vertexnum = currentloop->vertexindex; !fail && vertexnum<(currentloop->vertexindex+currentloop->vertexcount); vertexnum++)
      {
        vbo_xyz_uv_t *vbo;
        vbo = &flats_vbo[vertexnum];

        if (vbo->u < minu) minu = (float)floor(vbo->u);
        if (vbo->v < minv) minv = (float)floor(vbo->v);
        if (vbo->u > maxu) maxu = vbo->u;
        if (vbo->v > maxv) maxv = vbo->v;
        
        fail = (maxu - minu > 1.0f || maxv - minv > 1.0f);
      }
    }

    if (!fail)
    {
      sectorloops[i].flags = SECTOR_CLAMPXY;

      for (loopnum=0; loopnum<sectorloops[i].loopcount; loopnum++)
      {
        int vertexnum;
        // set the current loop
        currentloop = &sectorloops[i].loops[loopnum];
        if (!currentloop)
          continue;
        for (vertexnum = currentloop->vertexindex; vertexnum < (currentloop->vertexindex+currentloop->vertexcount); vertexnum++)
        {
          flats_vbo[vertexnum].u = flats_vbo[vertexnum].u - minu;
          flats_vbo[vertexnum].v = flats_vbo[vertexnum].v - minv;
        }
      }
    }
  }
}

static void gld_PreprocessSectors(void)
{
#ifdef PRBOOM_DEBUG
  levelinfo=M_fopen("levelinfo.txt","a");
  if (levelinfo)
  {
    if (gamemode==commercial)
      fprintf(levelinfo,"MAP%02i\n",gamemap);
    else
      fprintf(levelinfo,"E%iM%i\n",gameepisode,gamemap);
  }
#endif

  if (numsectors)
  {
    sectorloops=Z_Malloc(sizeof(GLSector)*numsectors,PU_STATIC,0);
    if (!sectorloops)
      I_Error("gld_PreprocessSectors: Not enough memory for array sectorloops");
    memset(sectorloops, 0, sizeof(GLSector)*numsectors);
  }

  if (numsubsectors)
  {
    subsectorloops=Z_Malloc(sizeof(GLMapSubsector)*numsubsectors,PU_STATIC,0);
    if (!subsectorloops)
      I_Error("gld_PreprocessSectors: Not enough memory for array subsectorloops");
    memset(subsectorloops, 0, sizeof(GLMapSubsector)*numsubsectors);
  }

  if (numsegs)
  {
    segrendered=calloc(numsegs, sizeof(byte));
    if (!segrendered)
      I_Error("gld_PreprocessSectors: Not enough memory for array segrendered");
  }

  if (numlines)
  {
    linerendered[0]=calloc(numlines, sizeof(byte));
    linerendered[1]=calloc(numlines, sizeof(byte));
    if (!linerendered[0] || !linerendered[1])
      I_Error("gld_PreprocessSectors: Not enough memory for array linerendered");
  }

  flats_vbo = NULL;
  gld_max_vertexes=0;
  gld_num_vertexes=0;
  if (numvertexes)
  {
    gld_AddGlobalVertexes(numvertexes*2);
  }

  // figgi -- adapted for glnodes
  if (numnodes)
  {
    if (nodesVersion == 0)
      gld_CarveFlats(numnodes-1, 0, 0);
    else
      gld_GetSubSectorVertices();
  }

  gld_ProcessTexturedMap();

  if (levelinfo) fclose(levelinfo);

  //e6y: for seamless rendering
  gld_MarkSectorsForClamp();
}

static void gld_PreprocessSegs(void)
{
  int i;

  gl_segs=Z_Malloc(numsegs*sizeof(GLSeg),PU_STATIC,0);
  for (i=0; i<numsegs; i++)
  {
    gl_segs[i].x1=-(float)segs[i].v1->x/(float)MAP_SCALE;
    gl_segs[i].z1= (float)segs[i].v1->y/(float)MAP_SCALE;
    gl_segs[i].x2=-(float)segs[i].v2->x/(float)MAP_SCALE;
    gl_segs[i].z2= (float)segs[i].v2->y/(float)MAP_SCALE;
  }

  gl_lines=Z_Malloc(numlines*sizeof(GLSeg),PU_STATIC,0);
  for (i=0; i<numlines; i++)
  {
    gl_lines[i].x1=-(float)lines[i].v1->x/(float)MAP_SCALE;
    gl_lines[i].z1= (float)lines[i].v1->y/(float)MAP_SCALE;
    gl_lines[i].x2=-(float)lines[i].v2->x/(float)MAP_SCALE;
    gl_lines[i].z2= (float)lines[i].v2->y/(float)MAP_SCALE;
  }
}

void gld_PreprocessLevel(void)
{
  // e6y: speedup of level reloading
  // Do not preprocess GL data twice for same level
  if (!gl_preprocessed)
  {
    int i;
    static int numsectors_prev = 0;
    static int numsubsectors_prev = 0;

    free(gl_segs);
    free(gl_lines);

    free(flats_vbo);
    flats_vbo = NULL;

    free(segrendered);
    free(linerendered[0]);
    free(linerendered[1]);
    
    for (i = 0; i < numsectors_prev; i++)
    {
      free(sectorloops[i].loops);
    }
    free(sectorloops);
    for (i = 0; i < numsubsectors_prev; i++)
    {
      free(subsectorloops[i].loops);
    }
    free(subsectorloops);

    gld_Precache();
    gld_PreprocessSectors();
    gld_PreprocessFakeSectors();
    gld_PreprocessSegs();

    numsectors_prev = numsectors;
    numsubsectors_prev = numsubsectors;
  }
  else
  {
    gld_PreprocessFakeSectors();

    memset(segrendered, 0, numsegs*sizeof(segrendered[0]));
    memset(linerendered[0], 0, numlines*sizeof(linerendered[0][0]));
    memset(linerendered[1], 0, numlines*sizeof(linerendered[1][0]));
  }

  gld_ResetTexturedAutomap();

  gld_FreeDrawInfo();

  //e6y
  gld_InitVertexData();

  gl_preprocessed = true;
}

/*****************************
 *
 * AUTOMAP
 *
 *****************************/

void gld_ProcessTexturedMap(void)
{
  if (map_textured && subsectorloops && subsectorloops[0].loops == NULL)
  {
    triangulate_subsectors = 1;
    if (nodesVersion == 0)
      gld_CarveFlats(numnodes-1, 0, 0);
    else
      gld_GetSubSectorVertices();
    triangulate_subsectors = 0;
  }
}

#endif
