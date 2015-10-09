// Copyright (c) 2015 The Khronos Group Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and/or associated documentation files (the
// "Materials"), to deal in the Materials without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Materials, and to
// permit persons to whom the Materials are furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Materials.
//
// MODIFICATIONS TO THIS FILE MAY MEAN IT NO LONGER ACCURATELY REFLECTS
// KHRONOS STANDARDS. THE UNMODIFIED, NORMATIVE VERSIONS OF KHRONOS
// SPECIFICATIONS AND HEADER INFORMATION ARE LOCATED AT
//    https://www.khronos.org/registry/
//
// THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.

#include "operand.h"

#include <assert.h>
#include <string.h>

static const spv_operand_desc_t sourceLanguageEntries[] = {
    {"Unknown",
     SourceLanguageUnknown,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"ESSL",
     SourceLanguageESSL,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"GLSL",
     SourceLanguageGLSL,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"OpenCL",
     SourceLanguageOpenCL,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t executionModelEntries[] = {
    {"Vertex",
     ExecutionModelVertex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"TessellationControl",
     ExecutionModelTessellationControl,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"TessellationEvaluation",
     ExecutionModelTessellationEvaluation,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"Geometry",
     ExecutionModelGeometry,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry),
     {SPV_OPERAND_TYPE_NONE}},
    {"Fragment",
     ExecutionModelFragment,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"GLCompute",
     ExecutionModelGLCompute,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Kernel",
     ExecutionModelKernel,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t addressingModelEntries[] = {
    {"Logical",
     AddressingModelLogical,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Physical32",
     AddressingModelPhysical32,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityAddresses),
     {SPV_OPERAND_TYPE_NONE}},
    {"Physical64",
     AddressingModelPhysical64,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityAddresses),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t memoryModelEntries[] = {
    {"Simple",
     MemoryModelSimple,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"GLSL450",
     MemoryModelGLSL450,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"OpenCL",
     MemoryModelOpenCL,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

// Execution mode requiring the given capability and having no operands.
#define ExecMode0(mode, cap)                                 \
  #mode, ExecutionMode##mode, SPV_OPCODE_FLAGS_CAPABILITIES, \
      SPV_CAPABILITY_AS_MASK(Capability##cap), {             \
    SPV_OPERAND_TYPE_NONE                                    \
  }
// Execution mode requiring the given capability and having one literal number
// operand.
#define ExecMode1(mode, cap)                                 \
  #mode, ExecutionMode##mode, SPV_OPCODE_FLAGS_CAPABILITIES, \
      SPV_CAPABILITY_AS_MASK(Capability##cap), {             \
    SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE   \
  }
static const spv_operand_desc_t executionModeEntries[] = {
    {ExecMode1(Invocations, Geometry)},
    {ExecMode0(SpacingEqual, Tessellation)},
    {ExecMode0(SpacingFractionalEven, Tessellation)},
    {ExecMode0(SpacingFractionalOdd, Tessellation)},
    {ExecMode0(VertexOrderCw, Tessellation)},
    {ExecMode0(VertexOrderCcw, Tessellation)},
    {ExecMode0(PixelCenterInteger, Shader)},
    {ExecMode0(OriginUpperLeft, Shader)},
    {ExecMode0(OriginLowerLeft, Shader)},
    {ExecMode0(EarlyFragmentTests, Shader)},
    {ExecMode0(PointMode, Tessellation)},
    {ExecMode0(Xfb, TransformFeedback)},
    {ExecMode0(DepthReplacing, Shader)},
    {ExecMode0(DepthGreater, Shader)},
    {ExecMode0(DepthLess, Shader)},
    {ExecMode0(DepthUnchanged, Shader)},
    {"LocalSize",
     ExecutionModeLocalSize,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"LocalSizeHint",
     ExecutionModeLocalSizeHint,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {ExecMode0(InputPoints, Geometry)},
    {ExecMode0(InputLines, Geometry)},
    {ExecMode0(InputLinesAdjacency, Geometry)},
    {"InputTriangles",
     ExecutionModeInputTriangles,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry) |
         SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {ExecMode0(InputTrianglesAdjacency, Geometry)},
    {ExecMode0(InputQuads, Tessellation)},
    {ExecMode0(InputIsolines, Tessellation)},
    {"OutputVertices",
     ExecutionModeOutputVertices,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry) |
         SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {ExecMode0(OutputPoints, Geometry)},
    {ExecMode0(OutputLineStrip, Geometry)},
    {ExecMode0(OutputTriangleStrip, Geometry)},
    {ExecMode1(VecTypeHint, Kernel)},
    {ExecMode0(ContractionOff, Kernel)},
    {ExecMode0(IndependentForwardProgress, Kernel)},
};
#undef ExecMode0
#undef ExecMode1

static const spv_operand_desc_t storageClassEntries[] = {
    // TODO(dneto): There are more storage classes in Rev32 and later.
    {"UniformConstant",
     StorageClassUniformConstant,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Input",
     StorageClassInput,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Uniform",
     StorageClassUniform,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Output",
     StorageClassOutput,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkgroupLocal",
     StorageClassWorkgroupLocal,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkgroupGlobal",
     StorageClassWorkgroupGlobal,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"PrivateGlobal",
     StorageClassPrivateGlobal,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Function",
     StorageClassFunction,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Generic",
     StorageClassGeneric,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"PushConstant",
     StorageClassPushConstant,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"AtomicCounter",
     StorageClassAtomicCounter,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityAtomicStorage),
     {SPV_OPERAND_TYPE_NONE}},
    {"Image",
     StorageClassImage,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t dimensionalityEntries[] = {
    {"1D",
     Dim1D,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilitySampled1D),
     {SPV_OPERAND_TYPE_NONE}},
    {"2D", Dim2D, SPV_OPCODE_FLAGS_NONE, 0, {SPV_OPERAND_TYPE_NONE}},
    {"3D", Dim3D, SPV_OPCODE_FLAGS_NONE, 0, {SPV_OPERAND_TYPE_NONE}},
    {"Cube",
     DimCube,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Rect",
     DimRect,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilitySampledRect),
     {SPV_OPERAND_TYPE_NONE}},
    {"Buffer",
     DimBuffer,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilitySampledBuffer),
     {SPV_OPERAND_TYPE_NONE}},
    {"InputTarget",
     DimInputTarget,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityInputTarget),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t samplerAddressingModeEntries[] = {
    {"None",
     SamplerAddressingModeNone,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"ClampToEdge",
     SamplerAddressingModeClampToEdge,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Clamp",
     SamplerAddressingModeClamp,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Repeat",
     SamplerAddressingModeRepeat,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"RepeatMirrored",
     SamplerAddressingModeRepeatMirrored,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t samplerFilterModeEntries[] = {
    {"Nearest",
     SamplerFilterModeNearest,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Linear",
     SamplerFilterModeLinear,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t samplerImageFormatEntries[] = {
#define CASE0(NAME)                                           \
  {                                                           \
    #NAME, ImageFormat##NAME, 0, 0, { SPV_OPERAND_TYPE_NONE } \
  }
#define CASE(NAME, CAP)                                      \
  {                                                          \
    #NAME, ImageFormat##NAME, SPV_OPCODE_FLAGS_CAPABILITIES, \
        SPV_CAPABILITY_AS_MASK(Capability##CAP), {           \
      SPV_OPERAND_TYPE_NONE                                  \
    }                                                        \
  }
    // clang-format off
  CASE0(Unknown),
  CASE(Rgba32f, Shader),
  CASE(Rgba16f, Shader),
  CASE(R32f, Shader),
  CASE(Rgba8, Shader),
  CASE(Rgba8Snorm, Shader),
  CASE(Rg32f, AdvancedFormats),
  CASE(Rg16f, AdvancedFormats),
  CASE(R11fG11fB10f, AdvancedFormats),
  CASE(R16f, AdvancedFormats),
  CASE(Rgba16, AdvancedFormats),
  CASE(Rgb10A2, AdvancedFormats),
  CASE(Rg16, AdvancedFormats),
  CASE(Rg8, AdvancedFormats),
  CASE(R16, AdvancedFormats),
  CASE(R8, AdvancedFormats),
  CASE(Rgba16Snorm, AdvancedFormats),
  CASE(Rg16Snorm, AdvancedFormats),
  CASE(Rg8Snorm, AdvancedFormats),
  CASE(R16Snorm, AdvancedFormats),
  CASE(R8Snorm, AdvancedFormats),
  CASE(Rgba32i, Shader),
  CASE(Rgba16i, Shader),
  CASE(Rgba8i, Shader),
  CASE(R32i, Shader),
  CASE(Rg32i, AdvancedFormats),
  CASE(Rg16i, AdvancedFormats),
  CASE(Rg8i, AdvancedFormats),
  CASE(R16i, AdvancedFormats),
  CASE(R8i, AdvancedFormats),
  CASE(Rgba32ui, Shader),
  CASE(Rgba16ui, Shader),
  CASE(Rgba8ui, Shader),
  CASE(R32ui, Shader),
  CASE(Rgb10a2ui, AdvancedFormats),
  CASE(Rg32ui, AdvancedFormats),
  CASE(Rg16ui, AdvancedFormats),
  CASE(Rg8ui, AdvancedFormats),
  CASE(R16ui, AdvancedFormats),
  CASE(R8ui, AdvancedFormats),
// clang-format on
#undef CASE
};

// All image channel orders depend on the Kernel capability.
#define CASE(NAME)                                                 \
  {                                                                \
    #NAME, ImageChannelOrder##NAME, SPV_OPCODE_FLAGS_CAPABILITIES, \
        SPV_CAPABILITY_AS_MASK(CapabilityKernel), {                \
      SPV_OPERAND_TYPE_NONE                                        \
    }                                                              \
  }
static const spv_operand_desc_t imageChannelOrderEntries[] = {
  CASE(R),
  CASE(A),
  CASE(RG),
  CASE(RA),
  CASE(RGB),
  CASE(RGBA),
  CASE(BGRA),
  CASE(ARGB),
  CASE(Intensity),
  CASE(Luminance),
  CASE(Rx),
  CASE(RGx),
  CASE(RGBx),
  CASE(Depth),
  CASE(DepthStencil),
  CASE(sRGB),
  CASE(sRGBx),
  CASE(sRGBA),
  CASE(sBGRA),
};
#undef CASE

// All image channel data types depend on the Kernel capability.
#define CASE(NAME)                                                 \
  {                                                                \
    #NAME, ImageChannelDataType##NAME, SPV_OPCODE_FLAGS_CAPABILITIES, \
        SPV_CAPABILITY_AS_MASK(CapabilityKernel), {                \
      SPV_OPERAND_TYPE_NONE                                        \
    }                                                              \
  }
static const spv_operand_desc_t imageChannelDataTypeEntries[] = {
  CASE(SnormInt8),
  CASE(SnormInt16),
  CASE(UnormInt8),
  CASE(UnormInt16),
  CASE(UnormShort565),
  CASE(UnormShort555),
  CASE(UnormInt101010),
  CASE(SignedInt8),
  CASE(SignedInt16),
  CASE(SignedInt32),
  CASE(UnsignedInt8),
  CASE(UnsignedInt16),
  CASE(UnsignedInt32),
  CASE(HalfFloat),
  CASE(Float),
  CASE(UnormInt24),
};
#undef CASE

// Image operand definitions.  Each enum value is a mask.  When that mask
// bit is set, the instruction should have further ID operands.
// Some mask values depend on a capability.
static const spv_operand_desc_t imageOperandEntries[] = {
// Rev32 and later adds many more enums.
#define CASE(NAME) \
  #NAME, spv::ImageOperands##NAME##Mask, SPV_OPCODE_FLAGS_NONE, 0
#define CASE_CAP(NAME, CAP) \
  #NAME, spv::ImageOperands##NAME##Mask, SPV_OPCODE_FLAGS_CAPABILITIES, CAP
#define ID SPV_OPERAND_TYPE_ID
#define NONE SPV_OPERAND_TYPE_NONE
    {"None", spv::ImageOperandsMaskNone, SPV_OPCODE_FLAGS_NONE, 0, {NONE}},
    {CASE_CAP(Bias, SPV_CAPABILITY_AS_MASK(CapabilityShader)), {ID, NONE}},
    {CASE(Lod), {ID, NONE}},
    {CASE(Grad), {ID, ID, NONE}},
    {CASE(ConstOffset), {ID, NONE}},
    {CASE_CAP(Offset, SPV_CAPABILITY_AS_MASK(CapabilityImageGatherExtended)),
     {ID, NONE}},
    {CASE(ConstOffsets), {ID, NONE}},
    {CASE(Sample), {ID, NONE}},
    {CASE_CAP(MinLod, SPV_CAPABILITY_AS_MASK(CapabilityMinLod)), {ID, NONE}},
#undef CASE
#undef CASE_CAP
#undef ID
#undef NONE
};

static const spv_operand_desc_t fpFastMathModeEntries[] = {
    {"None",
     FPFastMathModeMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"NotNaN",
     FPFastMathModeNotNaNMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NotInf",
     FPFastMathModeNotInfMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NSZ",
     FPFastMathModeNSZMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"AllowRecip",
     FPFastMathModeAllowRecipMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Fast",
     FPFastMathModeFastMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t fpRoundingModeEntries[] = {
    {"RTE",
     FPRoundingModeRTE,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"RTZ",
     FPRoundingModeRTZ,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"RTP",
     FPRoundingModeRTP,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"RTN",
     FPRoundingModeRTN,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t linkageTypeEntries[] = {
    {"Export",
     LinkageTypeExport,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityLinkage),
     {SPV_OPERAND_TYPE_NONE}},
    {"Import",
     LinkageTypeImport,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityLinkage),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t accessQualifierEntries[] = {
    {"ReadOnly",
     AccessQualifierReadOnly,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"WriteOnly",
     AccessQualifierWriteOnly,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"ReadWrite",
     AccessQualifierReadWrite,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t functionParameterAttributeEntries[] = {
    {"Zext",
     FunctionParameterAttributeZext,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Sext",
     FunctionParameterAttributeSext,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"ByVal",
     FunctionParameterAttributeByVal,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Sret",
     FunctionParameterAttributeSret,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NoAlias",
     FunctionParameterAttributeNoAlias,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NoCapture",
     FunctionParameterAttributeNoCapture,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NoWrite",
     FunctionParameterAttributeNoWrite,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NoReadWrite",
     FunctionParameterAttributeNoReadWrite,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t decorationEntries[] = {
    {"RelaxedPrecision",
     DecorationRelaxedPrecision,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {
        "SpecId",
        DecorationSpecId,
        SPV_OPCODE_FLAGS_CAPABILITIES,
        SPV_CAPABILITY_AS_MASK(CapabilityShader),
        {SPV_OPERAND_TYPE_LITERAL_NUMBER},
    },
    {"Block",
     DecorationBlock,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"BufferBlock",
     DecorationBufferBlock,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"RowMajor",
     DecorationRowMajor,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityMatrix),
     {SPV_OPERAND_TYPE_NONE}},
    {"ColMajor",
     DecorationColMajor,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityMatrix),
     {SPV_OPERAND_TYPE_NONE}},
    {"ArrayStride",
     DecorationArrayStride,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"MatrixStride",
     DecorationMatrixStride,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"GLSLShared",
     DecorationGLSLShared,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"GLSLPacked",
     DecorationGLSLPacked,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"CPacked",
     DecorationCPacked,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"BuiltIn",
     DecorationBuiltIn,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_BUILT_IN, SPV_OPERAND_TYPE_NONE}},
    {"Smooth",
     DecorationSmooth,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"NoPerspective",
     DecorationNoPerspective,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Flat",
     DecorationFlat,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Patch",
     DecorationPatch,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"Centroid",
     DecorationCentroid,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Sample",
     DecorationSample,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Invariant",
     DecorationInvariant,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"Restrict",
     DecorationRestrict,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Aliased",
     DecorationAliased,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Volatile",
     DecorationVolatile,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Constant",
     DecorationConstant,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Coherent",
     DecorationCoherent,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"NonWritable",
     DecorationNonWritable,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"NonReadable",
     DecorationNonReadable,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Uniform",
     DecorationUniform,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"SaturatedConversion",
     DecorationSaturatedConversion,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"Stream",
     DecorationStream,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"Location",
     DecorationLocation,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"Component",
     DecorationComponent,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"Index",
     DecorationIndex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"Binding",
     DecorationBinding,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"DescriptorSet",
     DecorationDescriptorSet,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"Offset",
     DecorationOffset,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"XfbBuffer",
     DecorationXfbBuffer,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTransformFeedback),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"XfbStride",
     DecorationXfbStride,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTransformFeedback),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
    {"FuncParamAttr",
     DecorationFuncParamAttr,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_FUNCTION_PARAMETER_ATTRIBUTE, SPV_OPERAND_TYPE_NONE}},
    {"FPRoundingMode",
     DecorationFPRoundingMode,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_FP_ROUNDING_MODE, SPV_OPERAND_TYPE_NONE}},
    {"FPFastMathMode",
     DecorationFPFastMathMode,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_FP_FAST_MATH_MODE, SPV_OPERAND_TYPE_NONE}},
    {"LinkageAttributes",
     DecorationLinkageAttributes,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityLinkage),
     {SPV_OPERAND_TYPE_LITERAL_STRING, SPV_OPERAND_TYPE_LINKAGE_TYPE,
      SPV_OPERAND_TYPE_NONE}},
    {"NoContraction",
     DecorationNoContraction,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"InputTargetIndex",
     DecorationInputTargetIndex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),  // TODO(dneto): Should this be
                                                // CapabilityInputTarget?
     {SPV_OPERAND_TYPE_NONE}},  // TODO(dneto): Should this have a literal
                                // number argument?
    {"Alignment",
     DecorationAlignment,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t builtInEntries[] = {
    {"Position",
     BuiltInPosition,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"PointSize",
     BuiltInPointSize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"ClipDistance",
     BuiltInClipDistance,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"CullDistance",
     BuiltInCullDistance,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"VertexId",
     BuiltInVertexId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"InstanceId",
     BuiltInInstanceId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"PrimitiveId",
     BuiltInPrimitiveId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry) |
         SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"InvocationId",
     BuiltInInvocationId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry) |
         SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"Layer",
     BuiltInLayer,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry),
     {SPV_OPERAND_TYPE_NONE}},
    {"ViewportIndex",
     BuiltInViewportIndex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityGeometry),
     {SPV_OPERAND_TYPE_NONE}},
    {"TessLevelOuter",
     BuiltInTessLevelOuter,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"TessLevelInner",
     BuiltInTessLevelInner,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"TessCoord",
     BuiltInTessCoord,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"PatchVertices",
     BuiltInPatchVertices,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityTessellation),
     {SPV_OPERAND_TYPE_NONE}},
    {"FragCoord",
     BuiltInFragCoord,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"PointCoord",
     BuiltInPointCoord,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"FrontFacing",
     BuiltInFrontFacing,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"SampleId",
     BuiltInSampleId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"SamplePosition",
     BuiltInSamplePosition,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"SampleMask",
     BuiltInSampleMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"FragColor",
     BuiltInFragColor,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"FragDepth",
     BuiltInFragDepth,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"HelperInvocation",
     BuiltInHelperInvocation,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"NumWorkgroups",
     BuiltInNumWorkgroups,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkgroupSize",
     BuiltInWorkgroupSize,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkgroupId",
     BuiltInWorkgroupId,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"LocalInvocationId",
     BuiltInLocalInvocationId,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"GlobalInvocationId",
     BuiltInGlobalInvocationId,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"LocalInvocationIndex",
     BuiltInLocalInvocationIndex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkDim",
     BuiltInWorkDim,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"GlobalSize",
     BuiltInGlobalSize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"EnqueuedWorkgroupSize",
     BuiltInEnqueuedWorkgroupSize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"GlobalOffset",
     BuiltInGlobalOffset,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"GlobalLinearId",
     BuiltInGlobalLinearId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkgroupLinearId",
     BuiltInWorkgroupLinearId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"SubgroupSize",
     BuiltInSubgroupSize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"SubgroupMaxSize",
     BuiltInSubgroupMaxSize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NumSubgroups",
     BuiltInNumSubgroups,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"NumEnqueuedSubgroups",
     BuiltInNumEnqueuedSubgroups,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"SubgroupId",
     BuiltInSubgroupId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"SubgroupLocalInvocationId",
     BuiltInSubgroupLocalInvocationId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"VertexIndex",
     BuiltInVertexIndex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"InstanceIndex",
     BuiltInInstanceIndex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t selectionControlEntries[] = {
    {"None",
     SelectionControlMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Flatten",
     SelectionControlFlattenMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"DontFlatten",
     SelectionControlDontFlattenMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t loopControlEntries[] = {
    {"None",
     LoopControlMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Unroll",
     LoopControlUnrollMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"DontUnroll",
     LoopControlDontUnrollMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t functionControlEntries[] = {
    {"None",
     FunctionControlMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Inline",
     FunctionControlInlineMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"DontInline",
     FunctionControlDontInlineMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Pure",
     FunctionControlPureMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Const",
     FunctionControlConstMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t memorySemanticsEntries[] = {
    // "Relaxed" should be a synonym for "None".
    // Put the Relaxed entry first so that the disassembler
    // will prefer to emit "Relaxed".
    {"Relaxed",
     MemorySemanticsMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"None",
     MemorySemanticsMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"SequentiallyConsistent",
     MemorySemanticsSequentiallyConsistentMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Acquire",
     MemorySemanticsAcquireMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Release",
     MemorySemanticsReleaseMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"UniformMemory",
     MemorySemanticsUniformMemoryMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {"SubgroupMemory",
     MemorySemanticsSubgroupMemoryMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkgroupLocalMemory",
     MemorySemanticsWorkgroupLocalMemoryMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"WorkgroupGlobalMemory",
     MemorySemanticsWorkgroupGlobalMemoryMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"AtomicCounterMemory",
     MemorySemanticsAtomicCounterMemoryMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityShader),
     {SPV_OPERAND_TYPE_NONE}},
    {
        "ImageMemory",
        MemorySemanticsImageMemoryMask,
        SPV_OPCODE_FLAGS_NONE,
        0,
        {SPV_OPERAND_TYPE_NONE},
    },
};

static const spv_operand_desc_t memoryAccessEntries[] = {
    {"None",
     MemoryAccessMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Volatile",
     MemoryAccessVolatileMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {
        "Aligned",
        MemoryAccessAlignedMask,
        SPV_OPCODE_FLAGS_NONE,
        0,
        {SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_NONE},
    },
    {"Nontemporal",
     MemoryAccessNontemporalMask,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t scopeEntries[] = {
    {"CrossDevice",
     ScopeCrossDevice,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Device", ScopeDevice, SPV_OPCODE_FLAGS_NONE, 0, {SPV_OPERAND_TYPE_NONE}},
    {"Workgroup",
     ScopeWorkgroup,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"Subgroup",
     ScopeSubgroup,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {
        "Invocation",
        ScopeInvocation,
        SPV_OPCODE_FLAGS_NONE,
        0,
        {SPV_OPERAND_TYPE_NONE},
    },
};

static const spv_operand_desc_t groupOperationEntries[] = {
    {"Reduce",
     GroupOperationReduce,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"InclusiveScan",
     GroupOperationInclusiveScan,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"ExclusiveScan",
     GroupOperationExclusiveScan,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t kernelKernelEnqueueFlagssEntries[] = {
    {"NoWait",
     KernelEnqueueFlagsNoWait,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"WaitKernel",
     KernelEnqueueFlagsWaitKernel,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
    {"WaitWorkGroup",
     KernelEnqueueFlagsWaitWorkGroup,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

static const spv_operand_desc_t kernelProfilingInfoEntries[] = {
    {"None",
     KernelProfilingInfoMaskNone,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_NONE}},
    {"CmdExecTime",
     KernelProfilingInfoCmdExecTimeMask,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     SPV_CAPABILITY_AS_MASK(CapabilityKernel),
     {SPV_OPERAND_TYPE_NONE}},
};

// A macro for defining a capability that doesn't depend on another capability.
#define CASE(NAME)                                       \
  {                                                      \
    #NAME, Capability##NAME, SPV_OPCODE_FLAGS_NONE, 0, { \
      SPV_OPERAND_TYPE_NONE                              \
    }                                                    \
  }

// A macro for defining a capability that depends on another.
#define CASE_CAP(NAME, CAP)                                 \
  {                                                         \
    #NAME, Capability##NAME, SPV_OPCODE_FLAGS_CAPABILITIES, \
        SPV_CAPABILITY_AS_MASK(Capability##CAP), {          \
      SPV_OPERAND_TYPE_NONE                                 \
    }                                                       \
  }

static const spv_operand_desc_t capabilityInfoEntries[] = {
    CASE(Matrix),
    CASE_CAP(Shader, Matrix),
    CASE_CAP(Geometry, Shader),
    CASE_CAP(Tessellation, Shader),
    CASE(Addresses),
    CASE(Linkage),
    CASE(Kernel),
    CASE_CAP(Vector16, Kernel),
    CASE_CAP(Float16Buffer, Kernel),
    CASE_CAP(Float16, Float16Buffer),
    CASE(Float64),
    CASE(Int64),
    CASE_CAP(Int64Atomics, Int64),
    CASE_CAP(ImageBasic, Kernel),
    CASE_CAP(ImageReadWrite, Kernel),
    CASE_CAP(ImageMipmap, Kernel),
    CASE_CAP(ImageSRGBWrite, Kernel),
    CASE_CAP(Pipes, Kernel),
    CASE(Groups),
    CASE_CAP(DeviceEnqueue, Kernel),
    CASE_CAP(LiteralSampler, Kernel),
    CASE_CAP(AtomicStorage, Shader),
    CASE(Int16),
    CASE_CAP(TessellationPointSize, Tessellation),
    CASE_CAP(GeometryPointSize, Geometry),
    CASE_CAP(ImageGatherExtended, Shader),
    CASE_CAP(StorageImageExtendedFormats, Shader),
    CASE_CAP(StorageImageMultisample, Shader),
    CASE_CAP(UniformBufferArrayDynamicIndexing, Shader),
    CASE_CAP(SampledImageArrayDynamicIndexing, Shader),
    CASE_CAP(StorageBufferArrayDynamicIndexing, Shader),
    CASE_CAP(StorageImageArrayDynamicIndexing, Shader),
    CASE_CAP(ClipDistance, Shader),
    CASE_CAP(CullDistance, Shader),
    CASE_CAP(ImageCubeArray, SampledCubeArray),
    CASE_CAP(SampleRateShading, Shader),
    CASE_CAP(ImageRect, SampledRect),
    CASE_CAP(SampledRect, Shader),
    CASE_CAP(GenericPointer, Addresses),
    CASE_CAP(Int8, Kernel),
    CASE_CAP(InputTarget, Shader),
    CASE_CAP(SparseResidency, Shader),
    CASE_CAP(MinLod, Shader),
    CASE_CAP(Sampled1D, Shader),
    CASE_CAP(Image1D, Sampled1D),
    CASE_CAP(SampledCubeArray, Shader),
    CASE_CAP(SampledBuffer, Shader),
    CASE_CAP(ImageBuffer, SampledBuffer),
    CASE_CAP(ImageMSArray, Shader),
    CASE_CAP(AdvancedFormats, Shader),
    CASE_CAP(ImageQuery, Shader),
    CASE_CAP(DerivativeControl, Shader),
    CASE_CAP(InterpolationFunction, Shader),
    CASE_CAP(TransformFeedback, Shader),
};
#undef CASE
#undef CASE_CAP

static const spv_operand_desc_group_t opcodeEntryTypes[] = {
    {SPV_OPERAND_TYPE_SOURCE_LANGUAGE,
     sizeof(sourceLanguageEntries) / sizeof(spv_operand_desc_t),
     sourceLanguageEntries},
    {SPV_OPERAND_TYPE_EXECUTION_MODEL,
     sizeof(executionModelEntries) / sizeof(spv_operand_desc_t),
     executionModelEntries},
    {SPV_OPERAND_TYPE_ADDRESSING_MODEL,
     sizeof(addressingModelEntries) / sizeof(spv_operand_desc_t),
     addressingModelEntries},
    {SPV_OPERAND_TYPE_MEMORY_MODEL,
     sizeof(memoryModelEntries) / sizeof(spv_operand_desc_t),
     memoryModelEntries},
    {SPV_OPERAND_TYPE_EXECUTION_MODE,
     sizeof(executionModeEntries) / sizeof(spv_operand_desc_t),
     executionModeEntries},
    {SPV_OPERAND_TYPE_STORAGE_CLASS,
     sizeof(storageClassEntries) / sizeof(spv_operand_desc_t),
     storageClassEntries},
    {SPV_OPERAND_TYPE_DIMENSIONALITY,
     sizeof(dimensionalityEntries) / sizeof(spv_operand_desc_t),
     dimensionalityEntries},
    {SPV_OPERAND_TYPE_SAMPLER_ADDRESSING_MODE,
     sizeof(samplerAddressingModeEntries) / sizeof(spv_operand_desc_t),
     samplerAddressingModeEntries},
    {SPV_OPERAND_TYPE_SAMPLER_FILTER_MODE,
     sizeof(samplerFilterModeEntries) / sizeof(spv_operand_desc_t),
     samplerFilterModeEntries},
    {SPV_OPERAND_TYPE_SAMPLER_IMAGE_FORMAT,
     sizeof(samplerImageFormatEntries) / sizeof(spv_operand_desc_t),
     samplerImageFormatEntries},
    {SPV_OPERAND_TYPE_IMAGE_CHANNEL_ORDER,
     sizeof(imageChannelOrderEntries) / sizeof(spv_operand_desc_t),
     imageChannelOrderEntries},
    {SPV_OPERAND_TYPE_IMAGE_CHANNEL_DATA_TYPE,
     sizeof(imageChannelDataTypeEntries) / sizeof(spv_operand_desc_t),
     imageChannelDataTypeEntries},
    {SPV_OPERAND_TYPE_OPTIONAL_IMAGE,
     sizeof(imageOperandEntries) / sizeof(spv_operand_desc_t),
     imageOperandEntries},
    {SPV_OPERAND_TYPE_FP_FAST_MATH_MODE,
     sizeof(fpFastMathModeEntries) / sizeof(spv_operand_desc_t),
     fpFastMathModeEntries},
    {SPV_OPERAND_TYPE_FP_ROUNDING_MODE,
     sizeof(fpRoundingModeEntries) / sizeof(spv_operand_desc_t),
     fpRoundingModeEntries},
    {SPV_OPERAND_TYPE_LINKAGE_TYPE,
     sizeof(linkageTypeEntries) / sizeof(spv_operand_desc_t),
     linkageTypeEntries},
    {SPV_OPERAND_TYPE_ACCESS_QUALIFIER,
     sizeof(accessQualifierEntries) / sizeof(spv_operand_desc_t),
     accessQualifierEntries},
    {SPV_OPERAND_TYPE_FUNCTION_PARAMETER_ATTRIBUTE,
     sizeof(functionParameterAttributeEntries) / sizeof(spv_operand_desc_t),
     functionParameterAttributeEntries},
    {SPV_OPERAND_TYPE_DECORATION,
     sizeof(decorationEntries) / sizeof(spv_operand_desc_t), decorationEntries},
    {SPV_OPERAND_TYPE_BUILT_IN,
     sizeof(builtInEntries) / sizeof(spv_operand_desc_t), builtInEntries},
    {SPV_OPERAND_TYPE_SELECTION_CONTROL,
     sizeof(selectionControlEntries) / sizeof(spv_operand_desc_t),
     selectionControlEntries},
    {SPV_OPERAND_TYPE_LOOP_CONTROL,
     sizeof(loopControlEntries) / sizeof(spv_operand_desc_t),
     loopControlEntries},
    {SPV_OPERAND_TYPE_FUNCTION_CONTROL,
     sizeof(functionControlEntries) / sizeof(spv_operand_desc_t),
     functionControlEntries},
    {SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
     sizeof(memorySemanticsEntries) / sizeof(spv_operand_desc_t),
     memorySemanticsEntries},
    {SPV_OPERAND_TYPE_OPTIONAL_MEMORY_ACCESS,
     sizeof(memoryAccessEntries) / sizeof(spv_operand_desc_t),
     memoryAccessEntries},
    {SPV_OPERAND_TYPE_EXECUTION_SCOPE,
     sizeof(scopeEntries) / sizeof(spv_operand_desc_t), scopeEntries},
    {SPV_OPERAND_TYPE_GROUP_OPERATION,
     sizeof(groupOperationEntries) / sizeof(spv_operand_desc_t),
     groupOperationEntries},
    {SPV_OPERAND_TYPE_KERNEL_ENQ_FLAGS,
     sizeof(kernelKernelEnqueueFlagssEntries) / sizeof(spv_operand_desc_t),
     kernelKernelEnqueueFlagssEntries},
    {SPV_OPERAND_TYPE_KERNEL_PROFILING_INFO,
     sizeof(kernelProfilingInfoEntries) / sizeof(spv_operand_desc_t),
     kernelProfilingInfoEntries},
    {SPV_OPERAND_TYPE_CAPABILITY,
     sizeof(capabilityInfoEntries) / sizeof(spv_operand_desc_t),
     capabilityInfoEntries},
};

spv_result_t spvOperandTableGet(spv_operand_table* pOperandTable) {
  if (!pOperandTable) return SPV_ERROR_INVALID_POINTER;

  static const spv_operand_table_t table = {
      sizeof(opcodeEntryTypes) / sizeof(spv_operand_desc_group_t),
      opcodeEntryTypes};

  *pOperandTable = &table;

  return SPV_SUCCESS;
}

spv_result_t spvOperandTableNameLookup(const spv_operand_table table,
                                       const spv_operand_type_t type,
                                       const char* name,
                                       const size_t nameLength,
                                       spv_operand_desc* pEntry) {
  if (!table) return SPV_ERROR_INVALID_TABLE;
  if (!name || !pEntry) return SPV_ERROR_INVALID_POINTER;

  for (uint64_t typeIndex = 0; typeIndex < table->count; ++typeIndex) {
    if (type == table->types[typeIndex].type) {
      for (uint64_t operandIndex = 0;
           operandIndex < table->types[typeIndex].count; ++operandIndex) {
        if (nameLength ==
                strlen(table->types[typeIndex].entries[operandIndex].name) &&
            !strncmp(table->types[typeIndex].entries[operandIndex].name, name,
                     nameLength)) {
          *pEntry = &table->types[typeIndex].entries[operandIndex];
          return SPV_SUCCESS;
        }
      }
    }
  }

  return SPV_ERROR_INVALID_LOOKUP;
}

spv_result_t spvOperandTableValueLookup(const spv_operand_table table,
                                        const spv_operand_type_t type,
                                        const uint32_t value,
                                        spv_operand_desc* pEntry) {
  if (!table) return SPV_ERROR_INVALID_TABLE;
  if (!pEntry) return SPV_ERROR_INVALID_POINTER;

  for (uint64_t typeIndex = 0; typeIndex < table->count; ++typeIndex) {
    if (type == table->types[typeIndex].type) {
      for (uint64_t operandIndex = 0;
           operandIndex < table->types[typeIndex].count; ++operandIndex) {
        if (value == table->types[typeIndex].entries[operandIndex].value) {
          *pEntry = &table->types[typeIndex].entries[operandIndex];
          return SPV_SUCCESS;
        }
      }
    }
  }

  return SPV_ERROR_INVALID_LOOKUP;
}

const char* spvOperandTypeStr(spv_operand_type_t type) {
  switch (type) {
    case SPV_OPERAND_TYPE_ID:
    case SPV_OPERAND_TYPE_OPTIONAL_ID:
    case SPV_OPERAND_TYPE_ID_IN_OPTIONAL_TUPLE:
      return "ID";
    case SPV_OPERAND_TYPE_RESULT_ID:
      return "result ID";
    case SPV_OPERAND_TYPE_LITERAL_NUMBER:
      return "literal number";
    case SPV_OPERAND_TYPE_MULTIWORD_LITERAL_NUMBER:
      return "multiple word literal number";
    case SPV_OPERAND_TYPE_LITERAL_STRING:
      return "literal string";
    case SPV_OPERAND_TYPE_SOURCE_LANGUAGE:
      return "source language";
    case SPV_OPERAND_TYPE_EXECUTION_MODEL:
      return "execution model";
    case SPV_OPERAND_TYPE_ADDRESSING_MODEL:
      return "addressing model";
    case SPV_OPERAND_TYPE_MEMORY_MODEL:
      return "memory model";
    case SPV_OPERAND_TYPE_EXECUTION_MODE:
      return "execution mode";
    case SPV_OPERAND_TYPE_STORAGE_CLASS:
      return "storage class";
    case SPV_OPERAND_TYPE_DIMENSIONALITY:
      return "dimensionality";
    case SPV_OPERAND_TYPE_SAMPLER_ADDRESSING_MODE:
      return "addressing mode";
    case SPV_OPERAND_TYPE_SAMPLER_FILTER_MODE:
      return "sampler filter mode";
    case SPV_OPERAND_TYPE_SAMPLER_IMAGE_FORMAT:
      return "sampler image format";
    case SPV_OPERAND_TYPE_FP_FAST_MATH_MODE:
      return "floating pointer fast math mode";
    case SPV_OPERAND_TYPE_FP_ROUNDING_MODE:
      return "floating point rounding mode";
    case SPV_OPERAND_TYPE_LINKAGE_TYPE:
      return "linkage type";
    case SPV_OPERAND_TYPE_ACCESS_QUALIFIER:
      return "access qualifier";
    case SPV_OPERAND_TYPE_FUNCTION_PARAMETER_ATTRIBUTE:
      return "function parameter attribute";
    case SPV_OPERAND_TYPE_DECORATION:
      return "decoration";
    case SPV_OPERAND_TYPE_BUILT_IN:
      return "built in";
    case SPV_OPERAND_TYPE_SELECTION_CONTROL:
      return "selection control";
    case SPV_OPERAND_TYPE_LOOP_CONTROL:
      return "loop control";
    case SPV_OPERAND_TYPE_FUNCTION_CONTROL:
      return "function control";
    case SPV_OPERAND_TYPE_MEMORY_SEMANTICS:
      return "memory semantics";
    case SPV_OPERAND_TYPE_OPTIONAL_MEMORY_ACCESS:
      return "memory access";
    case SPV_OPERAND_TYPE_EXECUTION_SCOPE:
      return "execution scope ID";
    case SPV_OPERAND_TYPE_GROUP_OPERATION:
      return "group operation";
    case SPV_OPERAND_TYPE_KERNEL_ENQ_FLAGS:
      return "kernel enqeue flags";
    case SPV_OPERAND_TYPE_KERNEL_PROFILING_INFO:
      return "kernel profiling info";
    case SPV_OPERAND_TYPE_CAPABILITY:
      return "capability";
    case SPV_OPERAND_TYPE_OPTIONAL_IMAGE:
      return "image operand";
    case SPV_OPERAND_TYPE_NONE:
      return "NONE";
    default:
      assert(0 && "Unhandled operand type!");
      break;
  }
  return "unknown";
}

void spvPrependOperandTypes(const spv_operand_type_t* types,
                            spv_operand_pattern_t* pattern) {
  const spv_operand_type_t* endTypes;
  for (endTypes = types; *endTypes != SPV_OPERAND_TYPE_NONE; ++endTypes)
    ;
  pattern->insert(pattern->begin(), types, endTypes);
}

void spvPrependOperandTypesForMask(const spv_operand_table operandTable,
                                   const spv_operand_type_t type,
                                   const uint32_t mask,
                                   spv_operand_pattern_t* pattern) {
  // Scan from highest bits to lowest bits because we will prepend in LIFO
  // fashion, and we need the operands for lower order bits to appear first.
  for (uint32_t candidate_bit = (1 << 31); candidate_bit; candidate_bit >>= 1) {
    if (candidate_bit & mask) {
      spv_operand_desc entry = nullptr;
      if (SPV_SUCCESS == spvOperandTableValueLookup(operandTable, type,
                                                    candidate_bit, &entry)) {
        spvPrependOperandTypes(entry->operandTypes, pattern);
      }
    }
  }
}

bool spvOperandIsOptional(spv_operand_type_t type) {
  // Variable means zero or more times.
  if (spvOperandIsVariable(type)) return true;

  switch (type) {
    case SPV_OPERAND_TYPE_OPTIONAL_ID:
    case SPV_OPERAND_TYPE_OPTIONAL_IMAGE:
    case SPV_OPERAND_TYPE_OPTIONAL_LITERAL_NUMBER:
    case SPV_OPERAND_TYPE_OPTIONAL_LITERAL_STRING:
    case SPV_OPERAND_TYPE_OPTIONAL_MEMORY_ACCESS:
    case SPV_OPERAND_TYPE_OPTIONAL_EXECUTION_MODE:
    case SPV_OPERAND_TYPE_OPTIONAL_CIV:
      return true;
    default:
      break;
  }
  return false;
}

bool spvOperandIsVariable(spv_operand_type_t type) {
  switch (type) {
    case SPV_OPERAND_TYPE_VARIABLE_ID:
    case SPV_OPERAND_TYPE_VARIABLE_LITERAL_NUMBER:
    case SPV_OPERAND_TYPE_VARIABLE_ID_LITERAL_NUMBER:
    case SPV_OPERAND_TYPE_VARIABLE_LITERAL_NUMBER_ID:
    case SPV_OPERAND_TYPE_VARIABLE_EXECUTION_MODE:
      return true;
    default:
      break;
  }
  return false;
}

bool spvExpandOperandSequenceOnce(spv_operand_type_t type,
                                  spv_operand_pattern_t* pattern) {
  switch (type) {
    case SPV_OPERAND_TYPE_VARIABLE_ID:
      pattern->insert(pattern->begin(), {SPV_OPERAND_TYPE_OPTIONAL_ID, type});
      return true;
    case SPV_OPERAND_TYPE_VARIABLE_LITERAL_NUMBER:
      pattern->insert(pattern->begin(),
                      {SPV_OPERAND_TYPE_OPTIONAL_LITERAL_NUMBER, type});
      return true;
    case SPV_OPERAND_TYPE_VARIABLE_LITERAL_NUMBER_ID:
      // Represents Zero or more (Literal number, Id) pairs.
      pattern->insert(pattern->begin(),
                      {SPV_OPERAND_TYPE_OPTIONAL_LITERAL_NUMBER,
                       SPV_OPERAND_TYPE_ID_IN_OPTIONAL_TUPLE, type});
      return true;
    case SPV_OPERAND_TYPE_VARIABLE_ID_LITERAL_NUMBER:
      // Represents Zero or more (Id, Literal number) pairs.
      pattern->insert(
          pattern->begin(),
          {SPV_OPERAND_TYPE_OPTIONAL_ID,
           SPV_OPERAND_TYPE_LITERAL_NUMBER_IN_OPTIONAL_TUPLE, type});
      return true;
    case SPV_OPERAND_TYPE_VARIABLE_EXECUTION_MODE:
      pattern->insert(pattern->begin(),
                      {SPV_OPERAND_TYPE_OPTIONAL_EXECUTION_MODE, type});
      return true;
    default:
      break;
  }
  return false;
}

spv_operand_type_t spvTakeFirstMatchableOperand(
    spv_operand_pattern_t* pattern) {
  assert(!pattern->empty());
  spv_operand_type_t result;
  do {
    result = pattern->front();
    pattern->pop_front();
  } while (spvExpandOperandSequenceOnce(result, pattern));
  return result;
}

spv_operand_pattern_t spvAlternatePatternFollowingImmediate(
    const spv_operand_pattern_t& pattern) {
  spv_operand_pattern_t alternatePattern;
  for (const auto& operand : pattern) {
    if (operand == SPV_OPERAND_TYPE_RESULT_ID) {
      alternatePattern.push_back(operand);
      alternatePattern.push_back(SPV_OPERAND_TYPE_OPTIONAL_CIV);
      return alternatePattern;
    }
    alternatePattern.push_back(SPV_OPERAND_TYPE_OPTIONAL_CIV);
  }
  // No result-id found, so just expect CIVs.
  return {SPV_OPERAND_TYPE_OPTIONAL_CIV};
}
