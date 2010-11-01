// Virvo - Virtual Reality Volume Rendering
// Copyright (C) 1999-2003 University of Stuttgart, 2004-2005 Brown University
// Contact: Jurgen P. Schulze, jschulze@ucsd.edu
//
// This file is part of Virvo.
//
// Virvo is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library (see license.txt); if not, write to the 
// Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

#include <iostream>
using std::cerr;
using std::endl;

#ifdef _WIN32
#include <windows.h>
#elif __linux
#include <string.h>
#endif

#ifndef VV_REMOTE_RENDERING
#include "vvopengl.h"
#endif

#include <assert.h>
#include <math.h>
#include "vvdebugmsg.h"
#include "vvvecmath.h"
#include "vvsoftimg.h"
#include "vvsoftvr.h"
#include "vvstopwatch.h"
#include "vvimage.h"

//----------------------------------------------------------------------------
/// Constructor.
vvSoftVR::vvSoftVR(vvVolDesc* vd, vvRenderState rs) : vvRenderer(vd, rs)
{
   int i;

   vvDebugMsg::msg(1, "vvSoftVR::vvSoftVR()");

   // Initialize variables:
   xClipNormal.set(0.0f, 0.0f, 1.0f);
   xClipDist = 0.0f;
   numProc = vvToolshed::getNumProcessors();
   len[0] = len[1] = len[2] = 0;
   compression = false;
   multiprocessing = false;
   preIntegration = false;
   sliceInterpol = true;
   warpInterpol = true;
   sliceBuffer = true;
   bilinLookup = false;
   opCorr = false;
   earlyRayTermination = 0;
   _timing = false;
   _size = vd->getSize();

   //  setWarpMode(SOFTWARE);     // initialize warp mode
   setWarpMode(TEXTURE);

   // Create intermediate image:
   intImg = new vvSoftImg(0,0);

   // Create output image size:
   outImg = NULL;
   vWidth = vHeight = -1;
   setOutputImageSize();

   // Generate x and y axis representations:
   for (i=0; i<3; ++i)
   {
      raw[i] = NULL;
      rle[i] = NULL;
      rleStart[i] = NULL;
   }
   findAxisRepresentations();
   //  encodeRLE();

   if (vd->getBPV() != 1)
   {
      cerr << "Shear-warp renderer can only display 8 bit scalar datasets." << endl;
   }

   // Generate color LUTs:
   updateTransferFunction();
}


//----------------------------------------------------------------------------
/// Destructor.
vvSoftVR::~vvSoftVR()
{
   int i;

   vvDebugMsg::msg(1, "vvSoftVR::~vvSoftVR()");

   delete outImg;
   delete intImg;
   for (i=0; i<3; ++i)
      delete[] raw[i];
}


//----------------------------------------------------------------------------
// See description in superclass.
void vvSoftVR::renderVolumeGL()
{
#ifndef VV_REMOTE_RENDERING
   GLint matrixMode;                              // current OpenGL matrix mode
   vvStopwatch* sw = NULL;                        // stop watch
                                                  // rendering times
   float preparation=0.0f, compositing=0.0f, warp=0.0f, total=0.0f;
   bool result;

   vvDebugMsg::msg(3, "vvSoftPer::renderVolumeGL()");

   if (vd->getBPV() != 1) return;                      // TODO: should work with all color depths

   if (_timing)
   {
      sw = new vvStopwatch();
      sw->start();
   }

   // Memorize current OpenGL matrix mode and modelview matrix
   // because prepareRendering modifies the modelview matrix:
   glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();

   result = prepareRendering();

   // Undo translation:
   glMatrixMode(GL_MODELVIEW);
   glPopMatrix();
   glMatrixMode(matrixMode);                      // restore matrix mode

   if (!result)
   {
      delete sw;
      return;
   }

   if (_timing)
   {
      preparation = sw->getTime();
   }

   compositeVolume();

   if (_timing)
   {
      compositing = sw->getTime() - preparation;
   }

   if (vvDebugMsg::isActive(3))
      compositeOutline();

   if (_renderState._boundaries)
                                                  // draw back boundaries
      drawBoundingBox(&_size, &vd->pos, _renderState._boundColor /*FIXME:, false*/);

   if (warpMode==SOFTWARE)
   {
      outImg->warp(&ivWarp, intImg);
      if (vvDebugMsg::isActive(3))
         outImg->overlay(intImg);
      outImg->draw();
   }
   else
      intImg->warpTex(&iwWarp);

   if (_renderState._boundaries)
                                                  // draw front boundaries
      drawBoundingBox(&_size, &vd->pos, _renderState._boundColor /*FIXME:, true*/);

   if (_timing)
   {
      total = sw->getTime();
      warp = total - compositing - preparation;
      cerr << "Times [ms]: prep=" << (preparation*1000.0f) <<
         ", comp=" << (compositing*1000.0f) << ", warp=" <<
         (warp * 1000.0f) << ", total=" << (total*1000.0f) << endl;
      delete sw;
   }

   vvRenderer::renderVolumeGL();                  // draw coordinate axes
#endif
}


//----------------------------------------------------------------------------
/** Render the outline of the volume to the intermediate image.
  The shear transformation matrices have to be computed before calling this method.
*/
void vvSoftVR::compositeOutline()
{
   vvVector3 vertex[8];
   int vert[12][2] =                              // volume edge point indices
   {
      {
         0,1
      }
      ,
      {
         1,2
      }
      ,
      {
         2,3
      }
      ,
      {
         3,0
      },
      {4,5},
      {
         5,6
      }
      ,
      {
         6,7
      }
      ,
      {
         7,4
      },
      {0,4},
      {
         1,5
      }
      ,
      {
         2,6
      }
      ,
      {
         3,7
      }
   };
   uchar col[12][3] =
   {
      {
         255,0,0
      }
      ,
      {
         0,255,0
      }
      ,
      {
         0,0,255
      },
      {0,255,255},
      {
         255,0,255
      }
      ,
      {
         255,255,0
      },
      {127,0,0},
      {
         0,127,0
      }
      ,
      {
         0,0,127
      },
      {                                           // color components (RGB) for lines
         0,127,127
      }
      ,
      {
         127,0,127
      }
      ,
      {
         127,127,0
      }
   };
   int i;
   int x1,y1,x,y;

   vvDebugMsg::msg(3, "vvSoftPar::compositeOutline()");

   // Compute vertices:
   for (i=0; i<8; ++i)
   {
      // Generate volume corners:
      vertex[i][0] = (float)(((i+1)/2) % 2);
      vertex[i][1] = (float)((i/2) % 2);
      vertex[i][2] = (float)((i/4) % 2);
      vertex[i].sub(0.5f);                        // vertices become -0.5 or +0.5
      vertex[i].scale(&_size);                     // vertices are scaled to correct object space coordinates
      vertex[i].multiply(&oiShear);               // shear and project vertex to intermediate image space
   }

   // Draw lines:
   for (i=0; i<12; ++i)
   {
      x  = (int)vertex[vert[i][0]].e[0];
      y  = (int)vertex[vert[i][0]].e[1];
      x1 = (int)vertex[vert[i][1]].e[0];
      y1 = (int)vertex[vert[i][1]].e[1];
      intImg->drawLine(x, y, x1, y1, col[i][0],col[i][1],col[i][2]);
   }
}


//----------------------------------------------------------------------------
/// Set new values for output image if necessary
void vvSoftVR::setOutputImageSize()
{
#ifndef VV_REMOTE_RENDERING
   GLint viewport[4];                             // OpenGL viewport information (position and size)

   vvDebugMsg::msg(3, "vvSoftVR::setOutputImageSize()");
   glGetIntegerv(GL_VIEWPORT, viewport);

   if (vWidth>0 && vHeight>0 &&
      vWidth==viewport[2] && vHeight==viewport[3])// already done?
      return;
   vWidth = viewport[2];
   vHeight = viewport[3];
   vvDebugMsg::msg(1, "Window dimensions: ", vWidth, vHeight);
   if (vWidth<1 || vHeight<1) vWidth = vHeight = 1;
   if (outImg != NULL) delete outImg;
   outImg = new vvSoftImg(vWidth, vHeight);

   findViewportMatrix(vWidth, vHeight);
#endif
}


//----------------------------------------------------------------------------
/// Gets the volume dimensions to standard object space
void vvSoftVR::findVolumeDimensions()
{
   vvDebugMsg::msg(3, "vvSoftVR::findVolumeDimensions()");

   switch (principal)
   {
      case X_AXIS:
         len[0] = vd->vox[1];
         len[1] = vd->vox[2];
         len[2] = vd->vox[0];
         break;
      case Y_AXIS:
         len[0] = vd->vox[2];
         len[1] = vd->vox[0];
         len[2] = vd->vox[1];
         break;
      case Z_AXIS:
      default:
         len[0] = vd->vox[0];
         len[1] = vd->vox[1];
         len[2] = vd->vox[2];
         break;
   }

   vvDebugMsg::msg(3, "Permuted volume dimensions are: ", len[0], len[1], len[2]);
}


//----------------------------------------------------------------------------
/** Generate raw volume data for the principal axes.
  @param data uchar data array of scalar values (need to be copied)
*/
void vvSoftVR::findAxisRepresentations()
{
   int frameSize;                                 // number of bytes per frame
   int sliceVoxels;                               // number of voxels per slice
   int i, x, y, z, c;
   int offset;                                    // unit: voxels
   int srcIndex;                                  // unit: bytes
   uchar* data;

   vvDebugMsg::msg(3, "vvSoftVR::findAxisRepresentations()");

   frameSize    = vd->getFrameBytes();
   sliceVoxels  = vd->getSliceVoxels();
   data = vd->getRaw();

   // Raw data for z axis view:
   delete[] raw[2];
   raw[2] = new uchar[frameSize];
   memcpy(raw[2], data, frameSize);

   // Raw data for x axis view:
   delete[] raw[0];
   raw[0] = new uchar[frameSize];
   i=0;
   for (x=vd->vox[0]-1; x>=0; --x)                // counts slices in x axis view
      for (z=0; z<vd->vox[2]; ++z)                // counts height in x axis view
   {
      offset = z * sliceVoxels + x;
      for (y=vd->vox[1]-1; y>=0; --y)
      {
         srcIndex = (y * vd->vox[0] + offset) * vd->getBPV();
         for (c=0; c<vd->getBPV(); ++c)
         {
            raw[0][i] = data[srcIndex + c];
            ++i;
         }
      }
   }

   // Raw data for y axis view:
   if (raw[1]!=NULL) delete[] raw[1];
   raw[1] = new uchar[frameSize];
   i=0;
   for (y=0; y<vd->vox[1]; ++y)
      for (x=vd->vox[0]-1; x>=0; --x)
   {
      offset = x + y * vd->vox[0];
      for (z=vd->vox[2]-1; z>=0; --z)
      {
         srcIndex = (offset + z * sliceVoxels) * vd->getBPV();
         for (c=0; c<vd->getBPV(); ++c)
         {
            raw[1][i] = data[srcIndex + c];
            ++i;
         }
      }
   }
}


//----------------------------------------------------------------------------
/** Run length encode the volume data.
  Encoding scheme: X is first byte.<UL>
  <LI>if X>0: copy next X voxels</LI>
  <LI>if X<0: repeat next voxel X times</LI>
  <LI>if X=0: done</LI></UL>
  Runs of same voxels must contain at least 3 voxels.
*/
void vvSoftVR::encodeRLE()
{
   int lineSize;                                  // number of bytes per line
   uchar* src;                                    // pointer to unencoded array
   uchar* dst;                                    // pointer to encoded array
   int i,j,a,b;
   int len;                                       // length of encoded data [bytes]
   int rest;                                      // number of bytes remaining in buffer
   int numvox[3];                                 // object dimensions (x,y,z) [voxels]

   vvDebugMsg::msg(1, "vvSoftVR::encodeRLE()");

   if (vd->getBPV() != 1) return;                      // TODO: enhance for other data types

   numvox[0] = vd->vox[0];
   numvox[1] = vd->vox[1];
   numvox[2] = vd->vox[2];

   vvDebugMsg::msg(1, "Original volume size: ", vd->getFrameBytes());

   // Prepare 3 sets of compressed data, one for each principal axis:
   for (i=0; i<3; ++i)
   {
      delete[] rleStart[i];
      delete[] rle[i];
      rle[i] = new uchar[vd->getFrameBytes()];     // reserve as much RAM as in uncompressed case
      rleStart[i] = new uchar*[numvox[i] * numvox[(i+2)%3]];
   }

   // Now compress the data:
   for (i=0; i<3; ++i)
   {
      src = raw[i];
      dst = rle[i];
      rest = vd->getFrameBytes();
      lineSize = numvox[(i+1)%3];

      for (a=0; a<numvox[i]; ++a)
         for (b=0; b<numvox[(i+2)%3]; ++b)
      {
         rleStart[i][a * numvox[(i+2)%3] + b] = dst;
         len   = vvToolshed::encodeRLE(dst, src, numvox[(i+1)%3], vd->getBPV(), rest);
         dst  += len;
         rest -= len;
         src  += lineSize;
      }
      if (vvDebugMsg::isActive(1))
      {
         cerr << "Compressed size: " << dst-rle[i] << " = " << (dst-rle[i])*100.0/vd->getFrameBytes() << " %" << endl;
         cerr << "rest = " << rest << endl;
      }
      if (rest<=0)
      {
         for (j=0; j<3; ++j)
         {
            delete[] rleStart[j];
            rleStart[j] = NULL;
         }
         vvDebugMsg::msg(1, "RLE compression ineffective");
         return;
      }
   }
}


//----------------------------------------------------------------------------
// See parent for comments.
void vvSoftVR::updateTransferFunction()
{
   int i, c;
   float* rgba;
   int lutEntries;

   vvDebugMsg::msg(1, "vvSoftVR::updateTransferFunction()");

   lutEntries = getLUTSize();
   rgba = new float[4 * lutEntries];

   vd->computeTFTexture(lutEntries, 1, 1, rgba);

   // Copy RGBA values to internal array:
   for (i=0; i<lutEntries; ++i)
      for (c=0; c<4; ++c)
         rgbaConv[i][c] = (uchar)(rgba[i*4+c] * 255.0f);
   delete[] rgba;

   // Make pre-integrated LUT:
   if (preIntegration)
   {
      makeLookupTextureOptimized(1.0f);           // use this line for fast pre-integration LUT
      //makeLookupTextureCorrect(1.0f);   // use this line for slow but more correct pre-integration LUT
   }
}


//----------------------------------------------------------------------------
/** Creates the look-up table for pre-integrated rendering.
  This version of the code runs rather slow compared to
  makeLookupTextureOptimized because it does a correct applications of
  the volume rendering integral.
  This method is
 * Copyright (C) 2001  Klaus Engel   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
@author Klaus Engel

I would like to thank Martin Kraus who helped in the adaptation
of the pre-integration method to Virvo.
@param thickness  distance of two volume slices in the direction
of the principal viewing axis (defaults to 1.0)
*/
void vvSoftVR::makeLookupTextureCorrect(float thickness)
{
   vd->tf.makePreintLUTCorrect(PRE_INT_TABLE_SIZE, &preIntTable[0][0][0], thickness);
}


//----------------------------------------------------------------------------
/** Creates the look-up table for pre-integrated rendering.
  This version of the code runs much faster than makeLookupTextureCorrect
  due to some minor simplifications of the volume rendering integral.
  This method is
 * Copyright (C) 2001  Klaus Engel   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
@author Klaus Engel

I would like to thank Martin Kraus who helped in the adaptation
of the pre-integration method to Virvo.
@param thickness  distance of two volume slices in the direction
of the principal viewing axis (defaults to 1.0)
*/
void vvSoftVR::makeLookupTextureOptimized(float thickness)
{
   vd->tf.makePreintLUTOptimized(PRE_INT_TABLE_SIZE, &preIntTable[0][0][0], thickness);
}


//----------------------------------------------------------------------------
// See parent for comments.
void vvSoftVR::updateVolumeData()
{
   findAxisRepresentations();
}


//----------------------------------------------------------------------------
// See parent for comments
bool vvSoftVR::instantClassification() const
{
   vvDebugMsg::msg(3, "vvSoftVR::instantClassification()");
   return true;
}


//----------------------------------------------------------------------------
/** Compute number of entries in RGBA LUT.
  @return the number of entries in the RGBA lookup table.
*/
int vvSoftVR::getLUTSize()
{
   vvDebugMsg::msg(2, "vvSoftVR::getLUTSize()");
   return (vd->getBPV()==2) ? 4096 : 256;
}


//----------------------------------------------------------------------------
/** Compute view matrix.
  owView = glPM x glMV
*/
void vvSoftVR::findViewMatrix()
{
   vvMatrix mv;                                  // modelview matrix
   vvMatrix pm;                                  // projection matrix

   vvDebugMsg::msg(3, "vvSoftVR::findViewMatrix()");

   getModelviewMatrix(&mv);
   getProjectionMatrix(&pm);

   // Compute view matrix:
   owView.copy(&mv);
   owView.multiplyPost(&pm);
   if (vvDebugMsg::isActive(3)) owView.print("owView");
}


//----------------------------------------------------------------------------
/** Generates the permutation matrix which transforms object space
  into standard object space.
  The permutation matrix is chosen among three choices, depending on the
  current principal viewing axis.
*/
void vvSoftVR::findPermutationMatrix()
{
   vvDebugMsg::msg(3, "vvSoftVR::findPermutationMatrix()");

   osPerm.zero();
   switch (principal)
   {
      case X_AXIS:
         osPerm.e[0][1] = 1.0f;
         osPerm.e[1][2] = 1.0f;
         osPerm.e[2][0] = 1.0f;
         osPerm.e[3][3] = 1.0f;
         break;
      case Y_AXIS:
         osPerm.e[0][2] = 1.0f;
         osPerm.e[1][0] = 1.0f;
         osPerm.e[2][1] = 1.0f;
         osPerm.e[3][3] = 1.0f;
         break;
      case Z_AXIS:
      default:
         osPerm.e[0][0] = 1.0f;
         osPerm.e[1][1] = 1.0f;
         osPerm.e[2][2] = 1.0f;
         osPerm.e[3][3] = 1.0f;
         break;
   }
   if (vvDebugMsg::isActive(3)) osPerm.print("osPerm");
}


//----------------------------------------------------------------------------
/** Compute conversion matrix from world space to OpenGL viewport space.
  This method only requires the size of the OpenGL viewport and
  can thus be called only when the user resizes the OpenGL window.
  Goal: invert y coordinate, scale, and translate origin to image center.<BR>
  Required 2D matrix:
  <PRE>
    w/2  0   0   w/2
    0   h/2  0   h/2
    0    0   1    0
    0    0   0    1
  </PRE>
*/
void vvSoftVR::findViewportMatrix(int w, int h)
{
   vvDebugMsg::msg(2, "vvSoftVR::findViewportMatrix()");

   wvConv.identity();
   wvConv.e[0][0] = (float)(w / 2);
   wvConv.e[0][3] = (float)(w / 2);
   wvConv.e[1][1] = (float)(h / 2);
   wvConv.e[1][3] = (float)(h / 2);

   if (vvDebugMsg::isActive(3)) wvConv.print("wvConv");

}


//----------------------------------------------------------------------------
/** Determine intermediate image positions of bottom left and top right
  corner of a slice.
  @param slice  current slice index
  @param start  bottom left corner
  @param end    top right corner (pass NULL if not required)
*/
void vvSoftVR::findSlicePosition(int slice, vvVector3* start, vvVector3* end)
{
   // Determine voxel coordinates in object space:
   switch (principal)
   {
      case X_AXIS:
         start->e[0] =  0.5f * _size[0] - (float)slice / (float)len[2] * _size[0];
         start->e[1] = -0.5f * _size[1];
         start->e[2] = -0.5f * _size[2];
         if (end)
         {
            end->e[0]   =  start->e[0];
            end->e[1]   = -start->e[1];
            end->e[2]   = -start->e[2];
         }
         break;
      case Y_AXIS:
         start->e[0] = -0.5f * _size[0];
         start->e[1] =  0.5f * _size[1] - (float)slice / (float)len[2] * _size[1];
         start->e[2] = -0.5f * _size[2];
         if (end)
         {
            end->e[0]   = -start->e[0];
            end->e[1]   =  start->e[1];
            end->e[2]   = -start->e[2];
         }
         break;
      case Z_AXIS:
         start->e[0] = -0.5f * _size[0];
         start->e[1] = -0.5f * _size[1];
         start->e[2] =  0.5f * _size[2] - (float)slice / (float)len[2] * _size[2];
         if (end)
         {
            end->e[0]   = -start->e[0];
            end->e[1]   = -start->e[1];
            end->e[2]   =  start->e[2];
         }
         break;
   }

   // Project bottom left voxel of this slice to intermediate image:
   start->multiply(&oiShear);
   if (end)
      end->multiply(&oiShear);
}


//----------------------------------------------------------------------------
/** Compute the clipping plane equation in the permuted voxel coordinate system.
 */
void vvSoftVR::findClipPlaneEquation()
{
   vvMatrix oxConv;                              // conversion matrix from object space to permuted voxel space
   vvMatrix xxPerm;                              // coordinate permutation matrix
   vvVector3 planePoint;                          // point on clipping plane = starting point of normal
   vvVector3 normalPoint;                         // end point of normal

   vvDebugMsg::msg(3, "vvSoftVR::findClipPlaneEquation()");

   // Compute conversion matrix:
   oxConv.identity();
   oxConv.e[0][0] =  (float)vd->vox[0]  / _size[0];
   oxConv.e[1][1] = -(float)vd->vox[1] / _size[1]; // negate because y coodinate points down
   oxConv.e[2][2] = -(float)vd->vox[2] / _size[2]; // negate because z coordinate points back
   oxConv.e[0][3] =  (float)vd->vox[0]  / 2.0f;
   oxConv.e[1][3] =  (float)vd->vox[1] / 2.0f;
   oxConv.e[2][3] =  (float)vd->vox[2] / 2.0f;

   // Find coordinate permutation matrix:
   xxPerm.zero();
   switch (principal)
   {
      case X_AXIS:
         xxPerm.e[0][1] =-1.0f;
         xxPerm.e[0][3] = (float)vd->vox[1];
         xxPerm.e[1][2] = 1.0f;
         xxPerm.e[2][0] =-1.0f;
         xxPerm.e[2][3] = (float)vd->vox[0];
         xxPerm.e[3][3] = 1.0f;
         break;
      case Y_AXIS:
         xxPerm.e[0][2] =-1.0f;
         xxPerm.e[0][3] = (float)vd->vox[2];
         xxPerm.e[1][0] =-1.0f;
         xxPerm.e[1][3] = (float)vd->vox[0];
         xxPerm.e[2][1] = 1.0f;
         xxPerm.e[3][3] = 1.0f;
         break;
      case Z_AXIS:
      default:
         xxPerm.e[0][0] = 1.0f;
         xxPerm.e[1][1] = 1.0f;
         xxPerm.e[2][2] = 1.0f;
         xxPerm.e[3][3] = 1.0f;
         break;
   }

   // Find two points determining the plane:
   planePoint.copy(&_renderState._clipPoint);
   normalPoint.copy(&_renderState._clipPoint);
   normalPoint.add(&_renderState._clipNormal);

   // Transfer points to voxel coordinate system:
   planePoint.multiply(&oxConv);
   normalPoint.multiply(&oxConv);

   // Permute the points:
   planePoint.multiply(&xxPerm);
   normalPoint.multiply(&xxPerm);

   // Compute plane equation:
   xClipNormal.copy(&normalPoint);
   xClipNormal.sub(&planePoint);
   xClipNormal.normalize();
   xClipDist = xClipNormal.dot(&planePoint);
}


//----------------------------------------------------------------------------
/** Tests if a voxel [permuted voxel space] is clipped by the clipping plane.
  @param x,y,z  voxel coordinates
  @returns true if voxel is clipped, false if it is visible
*/
bool vvSoftVR::isVoxelClipped(int x, int y, int z)
{
   vvDebugMsg::msg(3, "vvSoftVR::isClipped()");

   if (!_renderState._clipMode) return false;

   if (xClipNormal[0] * (float)x + xClipNormal[1] *
      (float)y + xClipNormal[2] * (float)z > xClipDist)
      return true;
   else
      return false;
}


//----------------------------------------------------------------------------
/** Set warp mode.
  @param warpMode find valid warp modes in enum WarpType
*/
void vvSoftVR::setWarpMode(WarpType wm)
{
   vvDebugMsg::msg(3, "vvSoftVR::setWarpMode()");
   warpMode = wm;
}


//----------------------------------------------------------------------------
/** Get curernt warp mode.
  @return current warp mode
*/
vvSoftVR::WarpType vvSoftVR::getWarpMode()
{
   vvDebugMsg::msg(3, "vvSoftVR::getWarpMode()");
   return warpMode;
}


//----------------------------------------------------------------------------
/** Set new frame number.
  Additionally to the call to the superclass, the axis representations
  have to be re-computed.
  @see vvRenderer#setCurrentFrame(int)
*/
void vvSoftVR::setCurrentFrame(int index)
{
   vvDebugMsg::msg(3, "vvSoftVR::setCurrentFrame()");
   vvRenderer::setCurrentFrame(index);
   findAxisRepresentations();
}


//----------------------------------------------------------------------------
/** Return intermediate image in vvImage format.
  @param img returned intermediate image
*/
void vvSoftVR::getIntermediateImage(vvImage* image)
{
   vvDebugMsg::msg(3, "vvSoftVR::getIntermediateImage()", intImg->width, intImg->height);

   image->setNewImage(short(intImg->width), short(intImg->height), intImg->data);
}


//----------------------------------------------------------------------------
/** Return warp matrix iwWarp (intermediate image space to world space).
  @param iwWarp matrix which will be set to the warp matrix
*/
void vvSoftVR::getWarpMatrix(vvMatrix* warp)
{
   warp->copy(&iwWarp);
}


//----------------------------------------------------------------------------
/** Compute the bounding box of the slice data on the intermediate image.
  @param xmin,xmax returned minimum and maximum pixel index horizontally [0..image_width-1]
  @param ymin,ymax returned minimum and maximum pixel index vertically [0..image_height-1]
*/
void vvSoftVR::getIntermediateImageExtent(int* xmin, int* xmax, int* ymin, int* ymax)
{
   vvVector3 corner[4];                           // corners of first and last voxel slice on intermediate image
   int i;

   findSlicePosition(0, &corner[0], &corner[1]);
   findSlicePosition(len[2], &corner[2], &corner[3]);

   *xmin = (int)corner[0].e[0];
   *ymin = (int)corner[0].e[1];
   *xmax = int(corner[0].e[0]) + 1;
   *ymax = int(corner[0].e[1]) + 1;

   for (i=1; i<4; ++i)                            // loop thru rest of array
   {
      *xmin = ts_min(*xmin, (int)corner[i].e[0]);
      *ymin = ts_min(*ymin, (int)corner[i].e[1]);
      *xmax = ts_max(*xmax, int(corner[i].e[0]) + 1);
      *ymax = ts_max(*ymax, int(corner[i].e[1]) + 1);
   }
   *xmin = ts_clamp(*xmin, 0, intImg->width-1);
   *xmax = ts_clamp(*xmax, 0, intImg->width-1);
   *ymin = ts_clamp(*ymin, 0, intImg->height-1);
   *ymax = ts_clamp(*ymax, 0, intImg->height-1);
}


//----------------------------------------------------------------------------
/** Prepare the rendering of the intermediate image: check projection type,
    factor view matrix, etc.
    @return true if preparation was ok, false if an error occurred
*/
bool vvSoftVR::prepareRendering()
{
   vvMatrix mv;                                  // modelview matrix
   vvMatrix pm;                                  // projection matrix
   vvMatrix trans;                               // translation matrix

   vvDebugMsg::msg(3, "vvSoftVR::prepareRendering()");

   // Translate object by its position:
   trans.identity();
   trans.translate(vd->pos[0], vd->pos[1], vd->pos[2]);
   getModelviewMatrix(&mv);
   mv.multiplyPre(&trans);
   setModelviewMatrix(&mv);

   // Make sure a parallel projection matrix is used:
   getProjectionMatrix(&pm);
   if (rendererType==SOFTPAR && !pm.isProjOrtho())
   {
      vvDebugMsg::msg(1, "Parallel projection matrix expected! Rendering aborted.");
      return false;
   }
   else if (rendererType==SOFTPER && pm.isProjOrtho())
   {
      vvDebugMsg::msg(1, "Perspective projection matrix expected! Rendering aborted.");
      return false;
   }

   if (rendererType==SOFTPER)
   {
      // Cull object if behind viewer:
      if (getCullingStatus(pm.getNearPlaneZ()) == -1)
      {
         cerr << "culled" << endl;
         return false;
      }
   }

   setOutputImageSize();
   findViewMatrix();
   factorViewMatrix();                            // do the factorization
   findVolumeDimensions();                        // precompute the permuted volume dimensions
   if (_renderState._clipMode) findClipPlaneEquation();         // prepare clipping plane processing

   // Set interpolation types:
   intImg->setWarpInterpolation(warpInterpol);

   // Validate computed matrices:
   if (vvDebugMsg::isActive(3))
   {
      vvMatrix ovTest;
      vvMatrix ovView;
      ovTest.copy(&oiShear);
      ovTest.multiplyPost(&ivWarp);
      ovTest.print("ovTest = ivWarp x siShear x osPerm");
      ovView.copy(&owView);
      ovView.multiplyPost(&wvConv);
      ovView.print("ovView = wvConv x owView");
   }
   return true;
}


//----------------------------------------------------------------------------
/** Get the volume object's culling status respective to near plane.
  A boundary sphere is used for the test, thus it is not 100% exact.
  @param nearPlaneZ  z coordinate of near plane
  @return <UL>
          <LI> 1  object is entirely in front of the near plane (=visible)</LI>
          <LI> 0  object is partly in front and partly behind the near plane</LI>
          <LI>-1  object is entirely behind the near plane (=invisible)</LI>
          </UL>
*/
int vvSoftVR::getCullingStatus(float nearPlaneZ)
{
   vvDebugMsg::msg(3, "vvSoftVR::getCullingStatus()");

   vvVector3 nearNormal;                          // normal vector of near plane
   vvVector3 volPos;                              // volume position
   float     radius;                              // bounding sphere radius
   float     volDist;                             // distance of volume from near plane
   vvMatrix mv;                                  // modelview matrix

   // Generate plane equation for near plane (distance value = nearPlaneZ):
   nearNormal.set(0.0f, 0.0f, 1.0f);

   // Find bounding sphere radius:
   radius = _size.length() / 2.0f;

   // Find volume midpoint location:
   getModelviewMatrix(&mv);
   volPos.zero();
   volPos.multiply(&mv);

   // Apply plane equation to volume midpoint:
   volDist = nearNormal.dot(&volPos) - nearPlaneZ;

   if (fabs(volDist) < radius) return 0;
   else if (volDist < 0) return 1;
   else return -1;
}


//----------------------------------------------------------------------------
// see parent
void vvSoftVR::setParameter(ParameterType param, float newValue, char*)
{
   vvDebugMsg::msg(3, "vvSoftVR::setParameter()");
   switch (param)
   {
      case vvRenderer::VV_SLICEINT:
         sliceInterpol = (newValue == 0.0f) ? false : true;
         break;
#if 0
      case vvRenderer::VV_WARPINT:
         warpInterpol = (newValue == 0.0f) ? false : true;
         break;
      case vvRenderer::VV_COMPRESS:
         compression = (newValue == 0.0f) ? false : true;
         break;
      case vvRenderer::VV_MULTIPROC:
         multiprocessing = (newValue == 0.0f) ? false : true;
         break;
      case vvRenderer::VV_SLICEBUF:
         sliceBuffer = (newValue == 0.0f) ? false : true;
         cerr << "sliceBuffer set to " << int(sliceBuffer) << endl;
         break;
      case vvRenderer::VV_LOOKUP:
         bilinLookup = (newValue == 0.0f) ? false : true;
         cerr << "bilinLookup set to " << int(bilinLookup) << endl;
         break;
 #endif
      case vvRenderer::VV_PREINT:
         preIntegration = (newValue == 0.0f) ? false : true;
         if (preIntegration) updateTransferFunction();
         cerr << "preIntegration set to " << int(preIntegration) << endl;
         break;
     case vvRenderer::VV_OPCORR:
         opCorr = (newValue == 0.0f) ? false : true;
         cerr << "opCorr set to " << int(opCorr) << endl;
         break;
      default: break;
   }
}


//----------------------------------------------------------------------------
// see parent
float vvSoftVR::getParameter(ParameterType param, char*) const
{
   vvDebugMsg::msg(3, "vvSoftVR::getParameter()");

   switch (param)
   {
      case vvRenderer::VV_SLICEINT:
         return (sliceInterpol) ? 1.0f : 0.0f;
#if 0
      case vvRenderer::VV_WARPINT:
         return (warpInterpol) ? 1.0f : 0.0f;
      case vvRenderer::VV_COMPRESS:
         return (compression) ? 1.0f : 0.0f;
      case vvRenderer::VV_MULTIPROC:
         return (multiprocessing) ? 1.0f : 0.0f;
      case vvRenderer::VV_SLICEBUF:
         return (sliceBuffer) ? 1.0f : 0.0f;
      case vvRenderer::VV_LOOKUP:
         return (bilinLookup) ? 1.0f : 0.0f;
      case vvRenderer::VV_OPCORR:
         return (opCorr) ? 1.0f : 0.0f;
#endif
      case vvRenderer::VV_PREINT:
         return (preIntegration) ? 1.0f : 0.0f;
      default: return vvRenderer::getParameter(param);
   }
}


//============================================================================
// End of File
//============================================================================
