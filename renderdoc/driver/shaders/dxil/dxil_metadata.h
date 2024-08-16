/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "driver/dx/official/d3dcommon.h"
#include "driver/shaders/dxbc/dxbc_common.h"
#include "dxil_common.h"

namespace DXBC
{
enum class ShaderType : uint8_t;
enum class GlobalShaderFlags : int64_t;
};

namespace DXIL
{

struct PSVData0
{
  struct VSInfo
  {
    bool SVPositionOutput;
  };

  struct HSInfo
  {
    uint32_t inputCPs;
    uint32_t outputCPs;
    DXBC::TessellatorDomain tessDomain;
    DXBC::TessellatorOutputPrimitive outPrim;
  };

  struct DSInfo
  {
    uint32_t inputCPs;
    bool SVPositionOutput;
    DXBC::TessellatorDomain tessDomain;
  };

  struct GSInfo
  {
    DXBC::PrimitiveType inputPrim;
    D3D_PRIMITIVE_TOPOLOGY outputTopo;
    uint32_t outputStreams;
    bool SVPositionOutput;
  };

  struct PSInfo
  {
    bool SVDepthOutput;
    bool SampleRate;
  };

  struct ASInfo
  {
    uint32_t payloadBytes;
  };

  struct MSInfo
  {
    uint32_t groupsharedBytes;
    uint32_t groupsharedViewIDDepBytes;
    uint32_t payloadBytes;
    uint16_t maxVerts;
    uint16_t maxPrims;
  };

  union
  {
    VSInfo vs;
    HSInfo hs;
    DSInfo ds;
    GSInfo gs;
    PSInfo ps;
    ASInfo as;
    MSInfo ms;
  };
  uint32_t minWaveCount;
  uint32_t maxWaveCount;

  static const size_t ExpectedSize = sizeof(uint32_t) * 6;
};

struct PSVData1 : public PSVData0
{
  static const uint32_t NumOutputStreams = 4;

  DXBC::ShaderType shaderType;
  bool useViewID;

  union
  {
    struct
    {
      uint16_t maxVerts;
    } gs1;

    struct
    {
      uint8_t sigPatchConstVectors;
    } hs, gs;

    struct
    {
      uint8_t sigPrimVectors;
      DXBC::TessellatorDomain topology;
    } ms;
  };

  uint8_t inputSigElems;
  uint8_t outputSigElems;
  uint8_t patchConstPrimSigElems;

  uint8_t inputSigVectors;
  uint8_t outputSigVectors[NumOutputStreams];    // one per geometry stream

  static const size_t ExpectedSize = sizeof(PSVData0) + sizeof(uint16_t) + 10 * sizeof(uint8_t);
};

struct PSVData2 : public PSVData1
{
  uint32_t threadCount[3];

  static const size_t ExpectedSize = sizeof(PSVData1) + 3 * sizeof(uint32_t);
};

struct PSVResource0
{
  DXILResourceType type;
  uint32_t space;       // register space
  uint32_t regStart;    // start register (inclusive - for single register bind it's == reg)
  uint32_t regEnd;      // end register (inclusive - for single register bind it's == reg)
};

enum class PSVResourceFlags
{
  None = 0x0,
  Atomic64 = 0x1,
};

struct PSVResource1 : public PSVResource0
{
  DXIL::ResourceKind kind;
  PSVResourceFlags flags;
};

using PSVResource = PSVResource1;

struct PSVSignature0
{
  rdcstr name;
  rdcarray<uint32_t> semIndices;
  uint8_t rows;
  uint8_t firstRow;
  uint8_t cols;        // : 4
  uint8_t startCol;    // :2
  uint8_t alloc;       // :2
  SigSemantic semantic;
  DXBC::SigCompType compType;
  DXBC::InterpolationMode interpMode;
  uint8_t dynamicMask;    // :4
  uint8_t stream;         // :2
  uint8_t padding;
};

using PSVSignature = PSVSignature0;

inline const uint32_t VectorBitmaskBitSize(uint32_t numVectors)
{
  return AlignUp(numVectors * 4, 64U);
}

struct PSVData : public PSVData2
{
  enum class Version
  {
    Version0 = 0,
    Version1,
    Version2,
    VersionLatest = Version2,
  } version = Version::VersionLatest;

  rdcarray<PSVResource> resources;

  // stringtable
  // semanticindexs

  rdcarray<PSVSignature> inputSig;
  rdcarray<PSVSignature> outputSig;
  rdcarray<PSVSignature> patchConstPrimSig;

  // in the tables below we assume no more than 64 dwords in any signature (16 vectors) so store
  // bitmasks as 64-bit.

  // if view ID is used, a bitmask per output stream, the bitmask containing one bit per dword as
  // in PSVData1::outputSigVectors indicating if that output vector depends on view ID
  struct
  {
    uint64_t outputMask;
    uint64_t patchConstMask;
  } viewIDAffects[PSVData1::NumOutputStreams];

  // for each stream, a bitmask for each input vector with the bitmask containing which output
  // vectors have a dependency on the input vector.
  struct
  {
    rdcarray<uint64_t> dependentOutputsForInput;
  } IODependencies[PSVData1::NumOutputStreams];

  // same as above, but for patch constant outputs on inputs - HS only
  struct
  {
    rdcarray<uint64_t> dependentPCOutputsForInput;
  } PCOutDependencies;

  // same as above, but for outputs on patch constant inputs - DS only
  struct
  {
    rdcarray<uint64_t> dependentOutputsForPCInput;
  } PCInDependencies;
};

struct RDATData
{
  enum
  {
    Version1_0 = 0x10
  };

  enum class Part
  {
    Invalid = 0,
    StringBuffer = 1,
    IndexArrays = 2,
    ResourceTable = 3,
    FunctionTable = 4,
    RawBytes = 5,
    SubobjectTable = 6,
  };

  enum class ResourceFlags
  {
    None = 0x0,
    GloballyCoherent = 0x1,
    HasCounter = 0x2,
    ROV = 0x4,
    // unused dynamic indexing flag? 0x8
    Atomic64 = 0x10,
  };

  // name arbitrarily chosen to avoid the extremely generic "shader flags" naming
  enum class ShaderBehaviourFlags : uint16_t
  {
    None = 0x0,
    NodeProgramEntry = 0x1,
    SVPositionOutput = 0x2,
    SVDepthOutput = 0x4,
    SampleRate = 0x8,
    ViewID = 0x10,
  };

  struct ResourceInfo
  {
    ResourceClass nspace;    // SRV, UAV, Sampler, CB
    ResourceKind kind;       // texture type (2D, 3D, etc) or other resource binding type
    uint32_t resourceIndex;    // the 0-based ID of this resource within the class namespace (SRV, UAV, etc).
    uint32_t space;       // register space
    uint32_t regStart;    // start register (inclusive - for single register bind it's == reg)
    uint32_t regEnd;      // end register (inclusive - for single register bind it's == reg)
    rdcstr name;
    ResourceFlags flags;

    bool operator==(const ResourceInfo &o)
    {
      // use namespace and linear ID to look up resources
      return nspace == o.nspace && resourceIndex == o.resourceIndex;
    }

    bool operator==(const rdcpair<DXIL::ResourceClass, uint32_t> &o)
    {
      return nspace == o.first && resourceIndex == o.second;
    }
  };

  struct FunctionInfo
  {
    rdcstr name;
    rdcstr unmangledName;
    rdcarray<rdcpair<ResourceClass, uint32_t>> globalResources;
    rdcarray<rdcstr> functionDependencies;
    DXBC::ShaderType type;
    uint32_t payloadBytes;
    uint32_t attribBytes;
    DXBC::GlobalShaderFlags featureFlags;
    uint32_t shaderCompatMask;    // bitmask based on DXBC::ShaderType enum of stages this function could be used with.
    uint16_t minShaderModel;
    uint16_t minType;    // looks to always be equal to type above
  };

  struct FunctionInfo2 : public FunctionInfo
  {
    FunctionInfo2(FunctionInfo &info) : FunctionInfo(info)
    {
      minWaveCount = maxWaveCount = 0;
      shaderBehaviourFlags = ShaderBehaviourFlags::None;
      extraInfoRef = ~0U;
    }

    uint8_t minWaveCount;
    uint8_t maxWaveCount;
    ShaderBehaviourFlags shaderBehaviourFlags;

    // below here is a stage-specific set of data containing e.g. signature elements. Currently
    // DXC does not emit RDAT except for in library targets, so this will be unused. It would be an
    // index into a table elsewhere of VSInfo, PSInfo, etc.
    uint32_t extraInfoRef;
  };

  enum class StateObjectFlags : uint32_t
  {
    None = 0x0,
    LocalDepsOnExternals = 0x1,
    ExternalDepsOnLocals = 0x2,
    AllowAdditions = 0x4,
  };

  enum class HitGroupType : uint32_t
  {
    Triangle = 0,
    ProceduralPrimitive = 1,
  };

  enum class RTPipeFlags : uint32_t
  {
    None = 0x0,
    SkipTriangles = 0x100,
    SkipProcedural = 0x200,
  };

  struct SubobjectInfo
  {
    // values match D3D12_STATE_SUBOBJECT_TYPE
    enum class SubobjectType : uint32_t
    {
      StateConfig = 0,
      GlobalRS = 1,
      LocalRS = 2,
      // missing enum values
      SubobjectToExportsAssoc = 8,
      RTShaderConfig = 9,
      RTPipeConfig = 10,
      Hitgroup = 11,
      RTPipeConfig1 = 12,
    } type;
    rdcstr name;

    // we union members where possible but several contain arrays/strings which can't be unioned.

    struct StateConfig
    {
      StateObjectFlags flags;
    };

    struct RTShaderConfig
    {
      uint32_t maxPayloadBytes;
      uint32_t maxAttribBytes;
    };

    struct RTPipeConfig1
    {
      uint32_t maxRecursion;
      RTPipeFlags flags;
    };

    union
    {
      StateConfig config;
      RTShaderConfig rtshaderconfig;
      RTPipeConfig1 rtpipeconfig;
    };

    struct RootSig
    {
      bytebuf data;
    } rs;

    struct Assoc
    {
      rdcstr subobject;
      rdcarray<rdcstr> exports;
    } assoc;

    struct Hitgroup
    {
      HitGroupType type;
      rdcstr anyHit;
      rdcstr closestHit;
      rdcstr intersection;
    } hitgroup;
  };

  enum class FunctionInfoVersion
  {
    Version1 = 1,
    Version2,
    VersionLatest = Version2,
  } functionVersion = FunctionInfoVersion::VersionLatest;

  rdcarray<ResourceInfo> resourceInfo;
  rdcarray<FunctionInfo2> functionInfo;
  rdcarray<SubobjectInfo> subobjectsInfo;
};

};

BITMASK_OPERATORS(DXIL::PSVResourceFlags);
BITMASK_OPERATORS(DXIL::RDATData::ResourceFlags);
BITMASK_OPERATORS(DXIL::RDATData::ShaderBehaviourFlags);
