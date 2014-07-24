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

#ifndef VV_TEXREND_H
#define VV_TEXREND_H

#include <vector>

// Virvo:
#include "vvexport.h"
#include "vvrenderer.h"
#include "vvopengl.h"

class vvShaderFactory;
class vvShaderProgram;
class vvVolDesc;

//============================================================================
// Class Definitions
//============================================================================

/** Volume rendering engine using a texture-based algorithm.
  Textures can be drawn as planes or spheres. In planes mode a rendering
  quality can be given (determining the number of texture slices used), and
  the texture normal can be set according to the application's needs.<P>
  The data points are located at the grid as follows:<BR>
  The outermost data points reside at the very edge of the drawn object,
  the other values are evenly distributed inbetween.
  Make sure you define HAVE_CG in your compiler if you want to use Nvidia Cg.
  @author Juergen Schulze (schulze@cs.brown.de)
  @author Martin Aumueller
  @author Stefan Zellmann
  @see vvRenderer
*/

class VIRVOEXPORT vvTexRend : public vvRenderer
{
  public:
    enum ErrorType                                /// Error Codes
    {
      OK = 0,                                     ///< no error
      TRAM_ERROR,                                 ///< not enough texture memory
      TEX_SIZE_UNKNOWN,                           ///< size of 3D texture is unknown
      NO3DTEX,                                    ///< 3D textures not supported on this hardware
      UNSUPPORTED                                 ///< general error code
    };
    enum VoxelType                                /// Internal data type used in textures
    {
      VV_BEST = 0,                                ///< choose best
      VV_RGBA,                                    ///< transfer function look-up done in software
      VV_PIX_SHD,                                 ///< Fragment program (Cg or GLSL)
      VV_FRG_PRG                                  ///< ARB fragment program
    };
    enum FeatureType                              /// Rendering features
    {
      VV_MIP                                      ///< maximum intensity projection
    };
    enum SliceOrientation                         /// Slice orientation for planar 3D textures
    {
      VV_VARIABLE = 0,                            ///< choose automatically
      VV_VIEWPLANE,                               ///< parallel to view plane
      VV_CLIPPLANE,                               ///< parallel to clip plane
      VV_VIEWDIR,                                 ///< perpendicular to viewing direction
      VV_OBJECTDIR,                               ///< perpendicular to line eye-object
      VV_ORTHO                                    ///< as in orthographic projection
    };

    static const int NUM_PIXEL_SHADERS;           ///< number of pixel shaders used
  private:
    enum FragmentProgram
    {
      VV_FRAG_PROG_3D = 0,
      VV_FRAG_PROG_PREINT,
      VV_FRAG_PROG_MAX                            // has always to be last in list
    };
    std::vector<std::vector<float> > rgbaTF;      ///< density to RGBA conversion table, as created by TF [0..1]
    std::vector<std::vector<uint8_t> > rgbaLUT;   ///< final RGBA conversion table, as transferred to graphics hardware (includes opacity and gamma correction)
    uint8_t* preintTable;                         ///< lookup table for pre-integrated rendering, as transferred to graphics hardware
    float  lutDistance;                           ///< slice distance for which LUT was computed
    vvsize3   texels;                             ///< width, height and depth of volume, including empty space [texels]
    float texMin[3];                              ///< minimum texture value of object [0..1] (to prevent border interpolation)
    float texMax[3];                              ///< maximum texture value of object [0..1] (to prevent border interpolation)
    size_t   textures;                            ///< number of textures stored in TRAM
    size_t   texelsize;                           ///< number of bytes/voxel transferred to OpenGL (depending on rendering mode)
    GLint internalTexFormat;                      ///< internal texture format (parameter for glTexImage...)
    GLenum texFormat;                             ///< texture format (parameter for glTexImage...)
    GLuint* texNames;                             ///< names of texture slices stored in TRAM
    std::vector<GLuint> pixLUTName;               ///< names for transfer function textures
    GLuint fragProgName[VV_FRAG_PROG_MAX];        ///< names for fragment programs (for applying transfer function)
    VoxelType voxelType;                          ///< voxel type actually used
    bool extTex3d;                                ///< true = 3D texturing supported
    bool extNonPower2;                            ///< true = NonPowerOf2 textures supported
    bool extMinMax;                               ///< true = maximum/minimum intensity projections supported
    bool extPixShd;                               ///< true = Nvidia pixel shader support (requires GeForce FX)
    bool extBlendEquation;                        ///< true = support for blend equation extension
    bool arbFrgPrg;                               ///< true = ARB fragment program support
    bool arbMltTex;                               ///< true = ARB multitexture support
    bool usePreIntegration;                       ///< true = pre-integrated rendering is actually used
    ptrdiff_t minSlice, maxSlice;                 ///< min/maximum slice to render [0..numSlices-1], -1 for no slice constraints
    SliceOrientation _sliceOrientation;           ///< slice orientation for planar 3d textures
    size_t _lastFrame;                            ///< last frame rendered

    vvShaderFactory* _shaderFactory;              ///< Factory for shader-creation
    vvShaderProgram* _shader;                     ///< shader performing intersection test on gpu

    int _currentShader;                           ///< ID of currently used fragment shader
    int _previousShader;                          ///< ID of previous shader

    vvVector3 _eye;                               ///< the current eye position

    void setVoxelType(VoxelType vt);
    void makeLUTTexture() const;
    ErrorType makeTextures(bool newTex);

    void initClassificationStage();
    void freeClassificationStage();
    void enableShader (vvShaderProgram* shader) const;
    void disableShader(vvShaderProgram* shader) const;

    void enableLUTMode() const;
    void disableLUTMode() const;

    vvShaderProgram* initShader();

    void removeTextures();
    ErrorType updateTextures3D(ssize_t, ssize_t, ssize_t, ssize_t, ssize_t, ssize_t, bool);
    void setGLenvironment() const;
    void unsetGLenvironment() const;
    void renderTex3DPlanar(virvo::mat4 const& mv);
    VoxelType findBestVoxelType(VoxelType) const;
    void updateLUT(float dist);
    size_t getLUTSize(vvsize3& size) const;
    size_t getPreintTableSize() const;
    void enableFragProg() const;
    void disableFragProg() const;
    void enableTexture(GLenum target) const;
    void disableTexture(GLenum target) const;
    void evaluateLocalIllumination(vvShaderProgram* pixelShader, const vvVector3& normal);

    int  getCurrentShader() const;
    void setCurrentShader(int);
    size_t getTextureSize(size_t sz) const;
  public:
    vvTexRend(vvVolDesc*, vvRenderState, VoxelType=VV_BEST);
    virtual ~vvTexRend();
    void  setVolDesc(vvVolDesc* vd);
    void  renderVolumeGL();
    void  updateTransferFunction();
    void  updateVolumeData();
    void  updateVolumeData(size_t, size_t, size_t, size_t, size_t, size_t);
    void  activateClippingPlane();
    void  deactivateClippingPlane();
    void  setNumLights(int);
    bool  instantClassification() const;
    void  setViewingDirection(virvo::vec3f const& vd);
    void  setObjectDirection(virvo::vec3f const& od);
    bool checkParameter(ParameterType param, vvParam const& value) const;
    virtual void setParameter(ParameterType param, const vvParam& value);
    virtual vvParam getParameter(ParameterType param) const;
    static bool isSupported(VoxelType);
    bool isSupported(FeatureType) const;
    VoxelType getVoxelType() const;
    void renderQualityDisplay() const;
    void printLUT(size_t chan=0) const;
    void setTexMemorySize(size_t);
    size_t getTexMemorySize() const;
    uint8_t* getHeightFieldData(float[4][3], size_t&, size_t&);
    float getManhattenDist(float[3], float[3]) const;
};
#endif

//============================================================================
// End of File
//============================================================================
// vim: sw=2:expandtab:softtabstop=2:ts=2:cino=\:0g0t0
