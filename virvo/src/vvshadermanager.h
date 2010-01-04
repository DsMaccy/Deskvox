// Virvo - Virtual Reality Volume Rendering
// Contact: Stefan Zellmann, zellmans@uni-koeln.de
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

#ifndef _VV_SHADERMANAGER_H_
#define _VV_SHADERMANAGER_H_

#include "vvvecmath.h"

#include <iostream>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;

class VIRVOEXPORT vvShaderManager
{
public:
  // Type definitions.

  /*!
   * \brief         Type of shader program.
   *
   *                Specify whether the program represents a
   *                fragment, geometry or vertex shader, so
   *                that different implemenations may react
   *                appropriatly.
   */
  enum ShaderType
  {
    VV_FRAG_SHD = 0,
    VV_GEOM_SHD,
    VV_VERT_SHD
  };

  // Public interface.

  vvShaderManager();
  virtual ~vvShaderManager();

  /*!
   * \brief         Load a shader program given a file name.
   *
   *                Implement this method in your specific implementation.
   *                The provided file name is intended to be an absolute
   *                path to the file containing the shader source.
   * \param         shaderFileName Absolute path to the shader file.
   * \param         shaderType Type of shader (fragment, geometry, vertex).
   */
  virtual void loadShader(const char* shaderFileName, const ShaderType& shaderType) = 0;
  /*!
   * \brief         Load a shader from a source string.
   *
   *                Rather than constructing a shader from a source file,
   *                this method should be used to initialize it from
   *                the character array containing the source itself.
   * \param         shaderString The source code of the shader.
   * \param         shaderType Type of shader (fragment, geometry, vertex).
   */
  virtual void loadShaderByString(const char* shaderString, const ShaderType& shaderType) = 0;
  /*!
   * \brief         Enable a loaded shader.
   *
   *                Enable a shader from a list of shader programs.
   *                When shader programs are created, these are pushed
   *                on top of a stack. An index addresses these programs
   *                in the order they were created.
   * \param         index An index into the program stack.
   */
  virtual void enableShader(const int index) = 0;
  /*!
   * \brief         Disable a loaded shader.
   *
   *                Disable a shader from a list of shader programs.
   *                When shader programs are created, these are pushed
   *                on top of a stack. An index addresses these programs
   *                in the order they were created.
   * \param         index An index into the program stack.
   */
  virtual void disableShader(const int index) = 0;

  // If you desire any of the following functions, implement them in your specific implementation.
  // Note that using the methods of this (abstract) base class will result in thrown exceptions.
  // The functions aren't defined to be = 0 so that inherited classes may, but don't necessarily
  // need to implement all of the functions below.
  virtual void setParameter1f(const int programIndex, const char* parameterName,
                              const float& f1);
  virtual void setParameter2f(const int programIndex, const char* parameterName,
                              const float& f1, const float& f2);
  virtual void setParameter3f(const int programIndex, const char* parameterName,
                              const float& f1, const float& f2, const float& f3);
  virtual void setParameter4f(const int programIndex, const char* parameterName,
                              const float& f1, const float& f2, const float& f3, const float& f4);

  virtual void setParameter1i(const int programIndex, const char* parameterName,
                              const int& i1);
  virtual void setParameter2i(const int programIndex, const char* parameterName,
                              const int& i1, const int& i2);
  virtual void setParameter3i(const int programIndex, const char* parameterName,
                              const int& i1, const int& i2, const int& i3);
  virtual void setParameter4i(const int programIndex, const char* parameterName,
                              const int& i1, const int& i2, const int& i3, const int& i4);

  virtual void setParameter1fv(const int programIndex, const char* parameterName,
                               const float*& fv);
  virtual void setParameter2fv(const int programIndex, const char* parameterName,
                               const float*& fv);
  virtual void setParameter3fv(const int programIndex, const char* parameterName,
                               const float*& fv);
  virtual void setParameter4fv(const int programIndex, const char* parameterName,
                               const float*& fv);

  virtual void setParameter1iv(const int programIndex, const char* parameterName,
                               const int*& iv);
  virtual void setParameter2iv(const int programIndex, const char* parameterName,
                               const int*& iv);
  virtual void setParameter3iv(const int programIndex, const char* parameterName,
                               const int*& iv);
  virtual void setParameter4iv(const int programIndex, const char* parameterName,
                               const int*& iv);

  virtual void setArrayParameter1f(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const float& f1);
  virtual void setArrayParameter2f(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const float& f1, const float& f2);
  virtual void setArrayParameter3f(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const float& f1, const float& f2, const float& f3);
  virtual void setArrayParameter4f(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const float& f1, const float& f2, const float& f3, const float& f4);

  virtual void setArrayParameter1i(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const int& i1);
  virtual void setArrayParameter2i(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const int& i1, const int& i2);
  virtual void setArrayParameter3i(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const int& i1, const int& i2, const int& i3);
  virtual void setArrayParameter4i(const int programIndex, const char* parameterName, const int arrayIndex,
                                   const int& i1, const int& i2, const int& i3, const int& i4);
protected:
  // Non-public data.

  std::vector<const char*> _shaderFileNames;
  std::vector<ShaderType> _shaderTypes;
  std::vector<bool> _parametersInitialized;
private:
  void init();
};

#endif // _VV_SHADERMANAGER_H_

//============================================================================
// End of File
//============================================================================

