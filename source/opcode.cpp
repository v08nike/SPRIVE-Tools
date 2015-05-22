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

#include <libspirv/libspirv.h>
#include "binary.h"
#include "opcode.h"

#include <assert.h>
#include <string.h>

// Opcode API

const char *spvGeneratorStr(uint32_t generator) {
  switch (generator) {
    case SPV_GENERATOR_KHRONOS:
      return "Khronos";
    case SPV_GENERATOR_VALVE:
      return "Valve";
    case SPV_GENERATOR_LUNARG:
      return "LunarG";
    case SPV_GENERATOR_CODEPLAY:
      return "Codeplay Software Ltd.";
    default:
      return "Unknown";
  }
}

uint32_t spvOpcodeMake(uint16_t wordCount, Op opcode) {
  return ((uint32_t)opcode) | (((uint32_t)wordCount) << 16);
}

void spvOpcodeSplit(const uint32_t word, uint16_t *pWordCount, Op *pOpcode) {
  if (pWordCount) {
    *pWordCount = (uint16_t)((0xffff0000 & word) >> 16);
  }
  if (pOpcode) {
    *pOpcode = (Op)(0x0000ffff & word);
  }
}

static const spv_opcode_desc_t opcodeTableEntries[] = {
    {"Nop", 1, OpNop, SPV_OPCODE_FLAGS_NONE, 0, {}},
    {"Undef",
     3,
     OpUndef,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {
     "Source",
     3,
     OpSource,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_SOURCE_LANGUAGE, SPV_OPERAND_TYPE_LITERAL_NUMBER},
    },
    {"SourceExtension",
     1,
     OpSourceExtension,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"Name",
     2,
     OpName,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"MemberName",
     3,
     OpMemberName,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"String",
     2,
     OpString,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"Line",
     5,
     OpLine,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"DecorationGroup",
     2,
     OpDecorationGroup,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"Decorate",
     3,
     OpDecorate,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_DECORATION,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"MemberDecorate",
     4,
     OpMemberDecorate,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_DECORATION, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"GroupDecorate",
     2,
     OpGroupDecorate,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"GroupMemberDecorate",
     2,
     OpGroupMemberDecorate,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"Extension",
     1,
     OpExtension,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"ExtInstImport",
     2,
     OpExtInstImport,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"ExtInst",
     5,
     OpExtInst,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"MemoryModel",
     3,
     OpMemoryModel,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ADDRESSING_MODEL, SPV_OPERAND_TYPE_MEMORY_MODEL}},
    {"EntryPoint",
     3,
     OpEntryPoint,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_EXECUTION_MODEL, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"ExecutionMode",
     3,
     OpExecutionMode,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_EXECUTION_MODE,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"CompileFlag",
     1,
     OpCompileFlag,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"Capability",
     2,
     OpCapability,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_CAPABILITY}},
    {"TypeVoid",
     2,
     OpTypeVoid,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"TypeBool",
     2,
     OpTypeBool,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"TypeInt",
     4,
     OpTypeInt,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"TypeFloat",
     3,
     OpTypeFloat,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"TypeVector",
     4,
     OpTypeVector,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"TypeMatrix",
     4,
     OpTypeMatrix,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityMatrix,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"TypeSampler",
     8,
     OpTypeSampler,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {
      SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_DIMENSIONALITY, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_ID  // TODO: See Khronos bug 13755
     }},
    {"TypeFilter",
     2,
     OpTypeFilter,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"TypeArray",
     4,
     OpTypeArray,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TypeRuntimeArray",
     3,
     OpTypeRuntimeArray,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"TypeStruct",
     2,
     OpTypeStruct,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"TypeOpaque",
     2,
     OpTypeOpaque,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"TypePointer",
     4,
     OpTypePointer,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_STORAGE_CLASS,
      SPV_OPERAND_TYPE_ID}},
    {"TypeFunction",
     3,
     OpTypeFunction,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"TypeEvent",
     2,
     OpTypeEvent,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"TypeDeviceEvent",
     2,
     OpTypeDeviceEvent,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"TypeReserveId",
     2,
     OpTypeReserveId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"TypeQueue",
     2,
     OpTypeQueue,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"TypePipe",
     4,
     OpTypePipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ACCESS_QUALIFIER}},
    {"ConstantTrue",
     3,
     OpConstantTrue,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"ConstantFalse",
     3,
     OpConstantFalse,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"Constant",
     3,
     OpConstant,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"ConstantComposite",
     3,
     OpConstantComposite,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"ConstantSampler",
     6,
     OpConstantSampler,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_SAMPLER_ADDRESSING_MODE, SPV_OPERAND_TYPE_LITERAL_NUMBER,
      SPV_OPERAND_TYPE_SAMPLER_FILTER_MODE}},
    {"ConstantNull",
     3,
     OpConstantNull,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"SpecConstantTrue",
     3,
     OpSpecConstantTrue,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"SpecConstantFalse",
     3,
     OpSpecConstantFalse,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"SpecConstant",
     3,
     OpSpecConstant,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"SpecConstantComposite",
     3,
     OpSpecConstantComposite,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ELLIPSIS}},
    {
     "SpecConstantOp",
     4,
     OpSpecConstantOp,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_LITERAL_NUMBER, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ELLIPSIS},

    },
    {"Variable",
     4,
     OpVariable,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_STORAGE_CLASS, SPV_OPERAND_TYPE_ID}},
    {"VariableArray",
     5,
     OpVariableArray,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityAddresses,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_STORAGE_CLASS, SPV_OPERAND_TYPE_ID}},
    {"Load",
     4,
     OpLoad,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_MEMORY_ACCESS, SPV_OPERAND_TYPE_MEMORY_ACCESS,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"Store",
     3,
     OpStore,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_MEMORY_ACCESS,
      SPV_OPERAND_TYPE_MEMORY_ACCESS, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"CopyMemory",
     3,
     OpCopyMemory,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_MEMORY_ACCESS,
      SPV_OPERAND_TYPE_MEMORY_ACCESS, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"CopyMemorySized",
     4,
     OpCopyMemorySized,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityAddresses,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_MEMORY_ACCESS, SPV_OPERAND_TYPE_MEMORY_ACCESS,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"AccessChain",
     4,
     OpAccessChain,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"InBoundsAccessChain",
     4,
     OpInBoundsAccessChain,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"ArrayLength",
     5,
     OpArrayLength,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"ImagePointer",
     6,
     OpImagePointer,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"GenericPtrMemSemantics",
     4,
     OpGenericPtrMemSemantics,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"Function",
     5,
     OpFunction,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_FUNCTION_CONTROL, SPV_OPERAND_TYPE_ID}},
    {"FunctionParameter",
     3,
     OpFunctionParameter,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"FunctionEnd", 1, OpFunctionEnd, SPV_OPCODE_FLAGS_NONE, 0, {}},
    {"FunctionCall",
     4,
     OpFunctionCall,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"Sampler",
     5,
     OpSampler,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"TextureSample",
     5,
     OpTextureSample,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleDref",
     6,
     OpTextureSampleDref,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleLod",
     6,
     OpTextureSampleLod,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleProj",
     5,
     OpTextureSampleProj,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleGrad",
     7,
     OpTextureSampleGrad,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleOffset",
     6,
     OpTextureSampleOffset,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleProjLod",
     6,
     OpTextureSampleProjLod,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleProjGrad",
     7,
     OpTextureSampleProjGrad,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleLodOffset",
     7,
     OpTextureSampleLodOffset,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleProjOffset",
     6,
     OpTextureSampleProjOffset,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleGradOffset",
     8,
     OpTextureSampleGradOffset,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"TextureSampleProjLodOffset",
     7,
     OpTextureSampleProjLodOffset,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureSampleProjGradOffset",
     8,
     OpTextureSampleProjGradOffset,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"TextureFetchTexel",
     6,
     OpTextureFetchTexel,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureFetchTexelOffset",
     6,
     OpTextureFetchTexelOffset,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureFetchSample",
     6,
     OpTextureFetchSample,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureFetchTexel",
     5,
     OpTextureFetchTexel,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"TextureGather",
     6,
     OpTextureGather,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureGatherOffset",
     7,
     OpTextureGatherOffset,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"TextureGatherOffsets",
     7,
     OpTextureGatherOffsets,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureQuerySizeLod",
     5,
     OpTextureQuerySizeLod,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"TextureQuerySize",
     4,
     OpTextureQuerySize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureQueryLod",
     5,
     OpTextureQueryLod,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"TextureQueryLevels",
     4,
     OpTextureQueryLevels,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"TextureQuerySamples",
     4,
     OpTextureQuerySamples,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"ConvertFToU",
     4,
     OpConvertFToU,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"ConvertFToS",
     4,
     OpConvertFToS,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"ConvertSToF",
     4,
     OpConvertSToF,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"ConvertUToF",
     4,
     OpConvertUToF,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"UConvert",
     4,
     OpUConvert,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"SConvert",
     4,
     OpSConvert,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"FConvert",
     4,
     OpFConvert,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"ConvertPtrToU",
     4,
     OpConvertPtrToU,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityAddresses,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"ConvertUToPtr",
     4,
     OpConvertUToPtr,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityAddresses,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"PtrCastToGeneric",
     4,
     OpPtrCastToGeneric,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"GenericCastToPtr",
     4,
     OpGenericCastToPtr,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"Bitcast",
     4,
     OpBitcast,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"GenericCastToPtrExplicit",
     5,
     OpGenericCastToPtrExplicit,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_STORAGE_CLASS}},
    {"SatConvertSToU",
     4,
     OpSatConvertSToU,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"SatConvertUToS",
     4,
     OpSatConvertUToS,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"VectorExtractDynamic",
     5,
     OpVectorExtractDynamic,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"VectorInsertDynamic",
     6,
     OpVectorInsertDynamic,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"VectorShuffle",
     5,
     OpVectorShuffle,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"CompositeConstruct",
     3,
     OpCompositeConstruct,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ELLIPSIS}},
    {"CompositeExtract",
     4,
     OpCompositeExtract,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"CompositeInsert",
     5,
     OpCompositeInsert,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"CopyObject",
     4,
     OpCopyObject,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"Transpose",
     4,
     OpTranspose,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityMatrix,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"SNegate",
     4,
     OpSNegate,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"FNegate",
     4,
     OpFNegate,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"Not",
     4,
     OpNot,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"IAdd",
     5,
     OpIAdd,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FAdd",
     5,
     OpFAdd,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ISub",
     5,
     OpISub,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FSub",
     5,
     OpFSub,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"IMul",
     5,
     OpIMul,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FMul",
     5,
     OpFMul,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"UDiv",
     5,
     OpUDiv,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"SDiv",
     5,
     OpSDiv,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FDiv",
     5,
     OpFDiv,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"UMod",
     5,
     OpUMod,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"SRem",
     5,
     OpSRem,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"SMod",
     5,
     OpSMod,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FRem",
     5,
     OpFRem,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FMod",
     5,
     OpFMod,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"VectorTimesScalar",
     5,
     OpVectorTimesScalar,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"MatrixTimesScalar",
     5,
     OpMatrixTimesScalar,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityMatrix,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"VectorTimesMatrix",
     5,
     OpVectorTimesMatrix,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityMatrix,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"MatrixTimesVector",
     5,
     OpMatrixTimesVector,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityMatrix,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"MatrixTimesMatrix",
     5,
     OpMatrixTimesMatrix,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityMatrix,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"OuterProduct",
     5,
     OpOuterProduct,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityMatrix,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"Dot",
     5,
     OpDot,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ShiftRightLogical",
     5,
     OpShiftRightLogical,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ShiftRightArithmetic",
     5,
     OpShiftRightArithmetic,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ShiftLeftLogical",
     5,
     OpShiftLeftLogical,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"BitwiseOr",
     5,
     OpBitwiseOr,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"BitwiseXor",
     5,
     OpBitwiseXor,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"BitwiseAnd",
     5,
     OpBitwiseAnd,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"Any",
     4,
     OpAny,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"All",
     4,
     OpAll,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"IsNan",
     4,
     OpIsNan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"IsInf",
     4,
     OpIsInf,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"IsFinite",
     4,
     OpIsFinite,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"IsNormal",
     4,
     OpIsNormal,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"SignBitSet",
     4,
     OpSignBitSet,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"LessOrGreater",
     5,
     OpLessOrGreater,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"Ordered",
     5,
     OpOrdered,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"Unordered",
     5,
     OpUnordered,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"LogicalOr",
     5,
     OpLogicalOr,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"LogicalXor",
     5,
     OpLogicalXor,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"LogicalAnd",
     5,
     OpLogicalAnd,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"Select",
     6,
     OpSelect,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"IEqual",
     5,
     OpIEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FOrdEqual",
     5,
     OpFOrdEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FUnordEqual",
     5,
     OpFUnordEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"INotEqual",
     5,
     OpINotEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FOrdNotEqual",
     5,
     OpFOrdNotEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FUnordNotEqual",
     5,
     OpFUnordNotEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ULessThan",
     5,
     OpULessThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"SLessThan",
     5,
     OpSLessThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FOrdLessThan",
     5,
     OpFOrdLessThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FUnordLessThan",
     5,
     OpFUnordLessThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"UGreaterThan",
     5,
     OpUGreaterThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"SGreaterThan",
     5,
     OpSGreaterThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FOrdGreaterThan",
     5,
     OpFOrdGreaterThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FUnordGreaterThan",
     5,
     OpFUnordGreaterThan,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ULessThanEqual",
     5,
     OpULessThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"SLessThanEqual",
     5,
     OpSLessThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FOrdLessThanEqual",
     5,
     OpFOrdLessThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FUnordLessThanEqual",
     5,
     OpFUnordLessThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"UGreaterThanEqual",
     5,
     OpUGreaterThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"SGreaterThanEqual",
     5,
     OpSGreaterThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FOrdGreaterThanEqual",
     5,
     OpFOrdGreaterThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"FUnordGreaterThanEqual",
     5,
     OpFUnordGreaterThanEqual,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"DPdx",
     4,
     OpDPdx,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"DPdy",
     4,
     OpDPdy,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {
     "Fwidth",
     4,
     OpFwidth,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID},
    },
    {"DPdxFine",
     4,
     OpDPdxFine,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"DPdyFine",
     4,
     OpDPdyFine,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {
     "FwidthFine",
     4,
     OpFwidthFine,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID},
    },
    {"DPdxCoarse",
     4,
     OpDPdxCoarse,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"DPdyCoarse",
     4,
     OpDPdyCoarse,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {
     "FwidthCoarse",
     4,
     OpFwidthCoarse,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityShader,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID},
    },
    {"Phi",
     3,
     OpPhi,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"LoopMerge",
     3,
     OpLoopMerge,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LOOP_CONTROL}},
    {"SelectionMerge",
     3,
     OpSelectionMerge,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_SELECTION_CONTROL}},
    {"Label",
     2,
     OpLabel,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_RESULT_ID}},
    {"Branch", 2, OpBranch, SPV_OPCODE_FLAGS_NONE, 0, {SPV_OPERAND_TYPE_ID}},
    {"BranchConditional",
     4,
     OpBranchConditional,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"Switch",
     3,
     OpSwitch,
     SPV_OPCODE_FLAGS_VARIABLE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_LITERAL, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL,
      SPV_OPERAND_TYPE_ID}},
    {"Kill", 1, OpKill, SPV_OPCODE_FLAGS_CAPABILITIES, CapabilityShader, {}},
    {"Return", 1, OpReturn, SPV_OPCODE_FLAGS_NONE, 0, {}},
    {"ReturnValue",
     2,
     OpReturnValue,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID}},
    {"Unreachable",
     1,
     OpUnreachable,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {}},
    {"LifetimeStart",
     3,
     OpLifetimeStart,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"LifetimeStop",
     3,
     OpLifetimeStop,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL_NUMBER}},
    {"AtomicInit",
     3,
     OpAtomicInit,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"AtomicLoad",
     6,
     OpAtomicLoad,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS}},
    {"AtomicStore",
     5,
     OpAtomicStore,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_EXECUTION_SCOPE,
      SPV_OPERAND_TYPE_MEMORY_SEMANTICS, SPV_OPERAND_TYPE_ID}},
    {"AtomicExchange",
     7,
     OpAtomicExchange,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicCompareExchange",
     8,
     OpAtomicCompareExchange,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"AtomicCompareExchangeWeak",
     8,
     OpAtomicCompareExchangeWeak,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"AtomicIIncrement",
     6,
     OpAtomicIIncrement,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS}},
    {"AtomicIDecrement",
     6,
     OpAtomicIDecrement,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS}},
    {"AtomicIAdd",
     7,
     OpAtomicIAdd,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicISub",
     7,
     OpAtomicISub,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicUMin",
     7,
     OpAtomicUMin,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicUMax",
     7,
     OpAtomicUMax,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicAnd",
     7,
     OpAtomicAnd,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicOr",
     7,
     OpAtomicOr,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicXor",
     7,
     OpAtomicXor,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicIMin",
     7,
     OpAtomicIMin,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"AtomicIMax",
     7,
     OpAtomicIMax,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS,
      SPV_OPERAND_TYPE_ID}},
    {"EmitVertex",
     1,
     OpEmitVertex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGeometry,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_LITERAL_STRING}},
    {"EndPrimitive",
     1,
     OpEndPrimitive,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGeometry,
     {}},
    {"EmitStreamVertex",
     2,
     OpEmitStreamVertex,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGeometry,
     {SPV_OPERAND_TYPE_ID}},
    {"EndStreamPrimitive",
     2,
     OpEndStreamPrimitive,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGeometry,
     {SPV_OPERAND_TYPE_ID}},
    {"ControlBarrier",
     2,
     OpControlBarrier,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_EXECUTION_SCOPE}},
    {"MemoryBarrier",
     3,
     OpMemoryBarrier,
     SPV_OPCODE_FLAGS_NONE,
     0,
     {SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_MEMORY_SEMANTICS}},
    {"AsyncGroupCopy",
     9,
     OpAsyncGroupCopy,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"WaitGroupEvents",
     6,
     OpWaitGroupEvents,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityKernel,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"GroupAll",
     5,
     OpGroupAll,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID}},
    {"GroupAny",
     5,
     OpGroupAny,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID}},
    {"GroupBroadcast",
     6,
     OpGroupBroadcast,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"GroupIAdd",
     6,
     OpGroupIAdd,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"GroupFAdd",
     6,
     OpGroupFAdd,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"GroupFMin",
     6,
     OpGroupFMin,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"GroupUMin",
     6,
     OpGroupUMin,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"GroupSMin",
     6,
     OpGroupSMin,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"GroupFMax",
     6,
     OpGroupFMax,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"GroupUMax",
     6,
     OpGroupUMax,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"GroupSMax",
     6,
     OpGroupSMax,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityGroups,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_GROUP_OPERATION,
      SPV_OPERAND_TYPE_ID}},
    {"EnqueueMarker",
     7,
     OpEnqueueMarker,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"EnqueueKernel",
     13,
     OpEnqueueKernel,
     SPV_OPCODE_FLAGS_VARIABLE | SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_KERNEL_ENQ_FLAGS, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ELLIPSIS}},
    {"GetKernelNDrangeSubGroupCount",
     5,
     OpGetKernelNDrangeSubGroupCount,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"GetKernelNDrangeMaxSubGroupSize",
     5,
     OpGetKernelNDrangeMaxSubGroupSize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"GetKernelWorkGroupSize",
     4,
     OpGetKernelWorkGroupSize,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"GetKernelPreferredWorkGroupSizeMultiple",
     4,
     OpGetKernelPreferredWorkGroupSizeMultiple,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"RetainEvent",
     2,
     OpRetainEvent,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID}},
    {"ReleaseEvent",
     2,
     OpRetainEvent,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID}},
    {"CreateUserEvent",
     3,
     OpCreateUserEvent,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"IsValidEvent",
     4,
     OpIsValidEvent,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"SetUserEventStatus",
     3,
     OpSetUserEventStatus,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"CapabilitytureEventProfilingInfo",
     4,
     OpCaptureEventProfilingInfo,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_KERENL_PROFILING_INFO,
      SPV_OPERAND_TYPE_ID}},
    {"GetDefaultQueue",
     3,
     OpGetDefaultQueue,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID}},
    {"BuildNDRange",
     6,
     OpBuildNDRange,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityDeviceEnqueue,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"ReadPipe",
     5,
     OpReadPipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"WritePipe",
     5,
     OpWritePipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ReservedReadPipe",
     7,
     OpReservedReadPipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"ReservedWritePipe",
     7,
     OpReservedWritePipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"ReserveReadPipePackets",
     5,
     OpReserveReadPipePackets,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"ReserveWritePipePackets",
     5,
     OpReserveWritePipePackets,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"CommitReadPipe",
     3,
     OpCommitReadPipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"CommitWritePipe",
     3,
     OpCommitWritePipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_ID}},
    {"IsValidReserveId",
     4,
     OpIsValidReserveId,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"GetNumPipePackets",
     4,
     OpGetNumPipePackets,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"GetMaxPipePackets",
     4,
     OpGetMaxPipePackets,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID, SPV_OPERAND_TYPE_ID}},
    {"GroupReserveReadPipePackets",
     6,
     OpGroupReserveReadPipePackets,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"GroupReserveWritePipePackets",
     6,
     OpGroupReserveWritePipePackets,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_ID, SPV_OPERAND_TYPE_RESULT_ID,
      SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"GroupCommitReadPipe",
     4,
     OpGroupCommitReadPipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}},
    {"GroupCommitWritePipe",
     4,
     OpGroupCommitWritePipe,
     SPV_OPCODE_FLAGS_CAPABILITIES,
     CapabilityPipes,
     {SPV_OPERAND_TYPE_EXECUTION_SCOPE, SPV_OPERAND_TYPE_ID,
      SPV_OPERAND_TYPE_ID}}};

spv_result_t spvOpcodeTableGet(spv_opcode_table *pInstTable) {
  spvCheck(!pInstTable, return SPV_ERROR_INVALID_POINTER);

  static const spv_opcode_table_t table = {
      sizeof(opcodeTableEntries) / sizeof(spv_opcode_desc_t),
      opcodeTableEntries};

  *pInstTable = &table;

  return SPV_SUCCESS;
}

spv_result_t spvOpcodeTableNameLookup(const spv_opcode_table table,
                                      const char *name,
                                      spv_opcode_desc *pEntry) {
  spvCheck(!name || !pEntry, return SPV_ERROR_INVALID_POINTER);
  spvCheck(!table, return SPV_ERROR_INVALID_TABLE);

  // TODO: This lookup of the Opcode table is suboptimal! Binary sort would be
  // preferable but the table requires sorting on the Opcode name, but it's
  // static
  // const initialized and matches the order of the spec.
  const size_t nameLength = strlen(name);
  for (uint64_t opcodeIndex = 0; opcodeIndex < table->count; ++opcodeIndex) {
    if (nameLength == strlen(table->entries[opcodeIndex].name) &&
        !strncmp(name, table->entries[opcodeIndex].name, nameLength)) {
      // NOTE: Found out Opcode!
      *pEntry = &table->entries[opcodeIndex];
      return SPV_SUCCESS;
    }
  }

  return SPV_ERROR_INVALID_LOOKUP;
}

spv_result_t spvOpcodeTableValueLookup(const spv_opcode_table table,
                                       const Op opcode,
                                       spv_opcode_desc *pEntry) {
  spvCheck(!table, return SPV_ERROR_INVALID_TABLE);
  spvCheck(!pEntry, return SPV_ERROR_INVALID_POINTER);

  // TODO: As above this lookup is not optimal.
  for (uint64_t opcodeIndex = 0; opcodeIndex < table->count; ++opcodeIndex) {
    if (opcode == table->entries[opcodeIndex].opcode) {
      // NOTE: Found the Opcode!
      *pEntry = &table->entries[opcodeIndex];
      return SPV_SUCCESS;
    }
  }

  return SPV_ERROR_INVALID_LOOKUP;
}

int32_t spvOpcodeIsVariable(spv_opcode_desc entry) {
  return SPV_OPCODE_FLAGS_VARIABLE ==
         (SPV_OPCODE_FLAGS_VARIABLE & entry->flags);
}

int32_t spvOpcodeRequiresCapabilities(spv_opcode_desc entry) {
  return SPV_OPCODE_FLAGS_CAPABILITIES ==
         (SPV_OPCODE_FLAGS_CAPABILITIES & entry->flags);
}

void spvInstructionCopy(const uint32_t *words, const Op opcode,
                        const uint16_t wordCount, const spv_endianness_t endian,
                        spv_instruction_t *pInst) {
  pInst->opcode = opcode;
  pInst->wordCount = wordCount;
  for (uint16_t wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    pInst->words[wordIndex] = spvFixWord(words[wordIndex], endian);
    if (!wordIndex) {
      uint16_t thisWordCount;
      Op thisOpcode;
      spvOpcodeSplit(pInst->words[wordIndex], &thisWordCount, &thisOpcode);
      assert(opcode == thisOpcode && wordCount == thisWordCount &&
             "Endianness failed!");
    }
  }
}

const char *spvOpcodeString(const Op opcode) {
#define CASE(OPCODE) \
  case OPCODE:       \
    return #OPCODE;
  switch (opcode) {
    CASE(OpNop)
    CASE(OpSource)
    CASE(OpSourceExtension)
    CASE(OpExtension)
    CASE(OpExtInstImport)
    CASE(OpMemoryModel)
    CASE(OpEntryPoint)
    CASE(OpExecutionMode)
    CASE(OpTypeVoid)
    CASE(OpTypeBool)
    CASE(OpTypeInt)
    CASE(OpTypeFloat)
    CASE(OpTypeVector)
    CASE(OpTypeMatrix)
    CASE(OpTypeSampler)
    CASE(OpTypeFilter)
    CASE(OpTypeArray)
    CASE(OpTypeRuntimeArray)
    CASE(OpTypeStruct)
    CASE(OpTypeOpaque)
    CASE(OpTypePointer)
    CASE(OpTypeFunction)
    CASE(OpTypeEvent)
    CASE(OpTypeDeviceEvent)
    CASE(OpTypeReserveId)
    CASE(OpTypeQueue)
    CASE(OpTypePipe)
    CASE(OpConstantTrue)
    CASE(OpConstantFalse)
    CASE(OpConstant)
    CASE(OpConstantComposite)
    CASE(OpConstantSampler)
    CASE(OpConstantNull)
    CASE(OpSpecConstantTrue)
    CASE(OpSpecConstantFalse)
    CASE(OpSpecConstant)
    CASE(OpSpecConstantComposite)
    CASE(OpVariable)
    CASE(OpVariableArray)
    CASE(OpFunction)
    CASE(OpFunctionParameter)
    CASE(OpFunctionEnd)
    CASE(OpFunctionCall)
    CASE(OpExtInst)
    CASE(OpUndef)
    CASE(OpLoad)
    CASE(OpStore)
    CASE(OpPhi)
    CASE(OpDecorationGroup)
    CASE(OpDecorate)
    CASE(OpMemberDecorate)
    CASE(OpGroupDecorate)
    CASE(OpGroupMemberDecorate)
    CASE(OpName)
    CASE(OpMemberName)
    CASE(OpString)
    CASE(OpLine)
    CASE(OpVectorExtractDynamic)
    CASE(OpVectorInsertDynamic)
    CASE(OpVectorShuffle)
    CASE(OpCompositeConstruct)
    CASE(OpCompositeExtract)
    CASE(OpCompositeInsert)
    CASE(OpCopyObject)
    CASE(OpCopyMemory)
    CASE(OpCopyMemorySized)
    CASE(OpSampler)
    CASE(OpTextureSample)
    CASE(OpTextureSampleDref)
    CASE(OpTextureSampleLod)
    CASE(OpTextureSampleProj)
    CASE(OpTextureSampleGrad)
    CASE(OpTextureSampleOffset)
    CASE(OpTextureSampleProjLod)
    CASE(OpTextureSampleProjGrad)
    CASE(OpTextureSampleLodOffset)
    CASE(OpTextureSampleProjOffset)
    CASE(OpTextureSampleGradOffset)
    CASE(OpTextureSampleProjLodOffset)
    CASE(OpTextureSampleProjGradOffset)
    CASE(OpTextureFetchTexelOffset)
    CASE(OpTextureFetchSample)
    CASE(OpTextureFetchTexel)
    CASE(OpTextureGather)
    CASE(OpTextureGatherOffset)
    CASE(OpTextureGatherOffsets)
    CASE(OpTextureQuerySizeLod)
    CASE(OpTextureQuerySize)
    CASE(OpTextureQueryLod)
    CASE(OpTextureQueryLevels)
    CASE(OpTextureQuerySamples)
    CASE(OpAccessChain)
    CASE(OpInBoundsAccessChain)
    CASE(OpSNegate)
    CASE(OpFNegate)
    CASE(OpNot)
    CASE(OpAny)
    CASE(OpAll)
    CASE(OpConvertFToU)
    CASE(OpConvertFToS)
    CASE(OpConvertSToF)
    CASE(OpConvertUToF)
    CASE(OpUConvert)
    CASE(OpSConvert)
    CASE(OpFConvert)
    CASE(OpConvertPtrToU)
    CASE(OpConvertUToPtr)
    CASE(OpPtrCastToGeneric)
    CASE(OpGenericCastToPtr)
    CASE(OpBitcast)
    CASE(OpTranspose)
    CASE(OpIsNan)
    CASE(OpIsInf)
    CASE(OpIsFinite)
    CASE(OpIsNormal)
    CASE(OpSignBitSet)
    CASE(OpLessOrGreater)
    CASE(OpOrdered)
    CASE(OpUnordered)
    CASE(OpArrayLength)
    CASE(OpIAdd)
    CASE(OpFAdd)
    CASE(OpISub)
    CASE(OpFSub)
    CASE(OpIMul)
    CASE(OpFMul)
    CASE(OpUDiv)
    CASE(OpSDiv)
    CASE(OpFDiv)
    CASE(OpUMod)
    CASE(OpSRem)
    CASE(OpSMod)
    CASE(OpFRem)
    CASE(OpFMod)
    CASE(OpVectorTimesScalar)
    CASE(OpMatrixTimesScalar)
    CASE(OpVectorTimesMatrix)
    CASE(OpMatrixTimesVector)
    CASE(OpMatrixTimesMatrix)
    CASE(OpOuterProduct)
    CASE(OpDot)
    CASE(OpShiftRightLogical)
    CASE(OpShiftRightArithmetic)
    CASE(OpShiftLeftLogical)
    CASE(OpLogicalOr)
    CASE(OpLogicalXor)
    CASE(OpLogicalAnd)
    CASE(OpBitwiseOr)
    CASE(OpBitwiseXor)
    CASE(OpBitwiseAnd)
    CASE(OpSelect)
    CASE(OpIEqual)
    CASE(OpFOrdEqual)
    CASE(OpFUnordEqual)
    CASE(OpINotEqual)
    CASE(OpFOrdNotEqual)
    CASE(OpFUnordNotEqual)
    CASE(OpULessThan)
    CASE(OpSLessThan)
    CASE(OpFOrdLessThan)
    CASE(OpFUnordLessThan)
    CASE(OpUGreaterThan)
    CASE(OpSGreaterThan)
    CASE(OpFOrdGreaterThan)
    CASE(OpFUnordGreaterThan)
    CASE(OpULessThanEqual)
    CASE(OpSLessThanEqual)
    CASE(OpFOrdLessThanEqual)
    CASE(OpFUnordLessThanEqual)
    CASE(OpUGreaterThanEqual)
    CASE(OpSGreaterThanEqual)
    CASE(OpFOrdGreaterThanEqual)
    CASE(OpFUnordGreaterThanEqual)
    CASE(OpDPdx)
    CASE(OpDPdy)
    CASE(OpFwidth)
    CASE(OpDPdxFine)
    CASE(OpDPdyFine)
    CASE(OpFwidthFine)
    CASE(OpDPdxCoarse)
    CASE(OpDPdyCoarse)
    CASE(OpFwidthCoarse)
    CASE(OpEmitVertex)
    CASE(OpEndPrimitive)
    CASE(OpEmitStreamVertex)
    CASE(OpEndStreamPrimitive)
    CASE(OpControlBarrier)
    CASE(OpMemoryBarrier)
    CASE(OpImagePointer)
    CASE(OpAtomicInit)
    CASE(OpAtomicLoad)
    CASE(OpAtomicStore)
    CASE(OpAtomicExchange)
    CASE(OpAtomicCompareExchange)
    CASE(OpAtomicCompareExchangeWeak)
    CASE(OpAtomicIIncrement)
    CASE(OpAtomicIDecrement)
    CASE(OpAtomicIAdd)
    CASE(OpAtomicISub)
    CASE(OpAtomicUMin)
    CASE(OpAtomicUMax)
    CASE(OpAtomicAnd)
    CASE(OpAtomicOr)
    CASE(OpAtomicXor)
    CASE(OpLoopMerge)
    CASE(OpSelectionMerge)
    CASE(OpLabel)
    CASE(OpBranch)
    CASE(OpBranchConditional)
    CASE(OpSwitch)
    CASE(OpKill)
    CASE(OpReturn)
    CASE(OpReturnValue)
    CASE(OpUnreachable)
    CASE(OpLifetimeStart)
    CASE(OpLifetimeStop)
    CASE(OpCompileFlag)
    CASE(OpAsyncGroupCopy)
    CASE(OpWaitGroupEvents)
    CASE(OpGroupAll)
    CASE(OpGroupAny)
    CASE(OpGroupBroadcast)
    CASE(OpGroupIAdd)
    CASE(OpGroupFAdd)
    CASE(OpGroupFMin)
    CASE(OpGroupUMin)
    CASE(OpGroupSMin)
    CASE(OpGroupFMax)
    CASE(OpGroupUMax)
    CASE(OpGroupSMax)
    CASE(OpGenericCastToPtrExplicit)
    CASE(OpGenericPtrMemSemantics)
    CASE(OpReadPipe)
    CASE(OpWritePipe)
    CASE(OpReservedReadPipe)
    CASE(OpReservedWritePipe)
    CASE(OpReserveReadPipePackets)
    CASE(OpReserveWritePipePackets)
    CASE(OpCommitReadPipe)
    CASE(OpCommitWritePipe)
    CASE(OpIsValidReserveId)
    CASE(OpGetNumPipePackets)
    CASE(OpGetMaxPipePackets)
    CASE(OpGroupReserveReadPipePackets)
    CASE(OpGroupReserveWritePipePackets)
    CASE(OpGroupCommitReadPipe)
    CASE(OpGroupCommitWritePipe)
    CASE(OpEnqueueMarker)
    CASE(OpEnqueueKernel)
    CASE(OpGetKernelNDrangeSubGroupCount)
    CASE(OpGetKernelNDrangeMaxSubGroupSize)
    CASE(OpGetKernelWorkGroupSize)
    CASE(OpGetKernelPreferredWorkGroupSizeMultiple)
    CASE(OpRetainEvent)
    CASE(OpReleaseEvent)
    CASE(OpCreateUserEvent)
    CASE(OpIsValidEvent)
    CASE(OpSetUserEventStatus)
    CASE(OpCaptureEventProfilingInfo)
    CASE(OpGetDefaultQueue)
    CASE(OpBuildNDRange)
    default:
      assert(0 && "Unreachable!");
  }
#undef CASE
  return "unknown";
}

int32_t spvOpcodeIsType(const Op opcode) {
  switch (opcode) {
    case OpTypeVoid:
    case OpTypeBool:
    case OpTypeInt:
    case OpTypeFloat:
    case OpTypeVector:
    case OpTypeMatrix:
    case OpTypeSampler:
    case OpTypeFilter:
    case OpTypeArray:
    case OpTypeRuntimeArray:
    case OpTypeStruct:
    case OpTypeOpaque:
    case OpTypePointer:
    case OpTypeFunction:
    case OpTypeEvent:
    case OpTypeDeviceEvent:
    case OpTypeReserveId:
    case OpTypeQueue:
    case OpTypePipe:
      return true;
    default:
      return false;
  }
}

int32_t spvOpcodeIsScalarType(const Op opcode) {
  switch (opcode) {
    case OpTypeInt:
    case OpTypeFloat:
      return true;
    default:
      return false;
  }
}

int32_t spvOpcodeIsConstant(const Op opcode) {
  switch (opcode) {
    case OpConstantTrue:
    case OpConstantFalse:
    case OpConstant:
    case OpConstantComposite:
    case OpConstantSampler:
    // case OpConstantNull:
    case OpConstantNull:
    case OpSpecConstantTrue:
    case OpSpecConstantFalse:
    case OpSpecConstant:
    case OpSpecConstantComposite:
      // case OpSpecConstantOp:
      return true;
    default:
      return false;
  }
}

int32_t spvOpcodeIsComposite(const Op opcode) {
  switch (opcode) {
    case OpTypeVector:
    case OpTypeMatrix:
    case OpTypeArray:
    case OpTypeStruct:
      return true;
    default:
      return false;
  }
}

int32_t spvOpcodeAreTypesEqual(const spv_instruction_t *pTypeInst0,
                               const spv_instruction_t *pTypeInst1) {
  spvCheck(pTypeInst0->opcode != pTypeInst1->opcode, return false);
  spvCheck(pTypeInst0->words[1] != pTypeInst1->words[1], return false);
  return true;
}

int32_t spvOpcodeIsPointer(const Op opcode) {
  switch (opcode) {
    case OpVariable:
    case OpVariableArray:
    case OpAccessChain:
    case OpInBoundsAccessChain:
    // TODO: case OpImagePointer:
    case OpFunctionParameter:
      return true;
    default:
      return false;
  }
}

int32_t spvOpcodeIsObject(const Op opcode) {
  switch (opcode) {
    case OpConstantTrue:
    case OpConstantFalse:
    case OpConstant:
    case OpConstantComposite:
    // TODO: case OpConstantSampler:
    case OpConstantNull:
    case OpSpecConstantTrue:
    case OpSpecConstantFalse:
    case OpSpecConstant:
    case OpSpecConstantComposite:
    // TODO: case OpSpecConstantOp:
    case OpVariable:
    case OpVariableArray:
    case OpAccessChain:
    case OpInBoundsAccessChain:
    case OpConvertFToU:
    case OpConvertFToS:
    case OpConvertSToF:
    case OpConvertUToF:
    case OpUConvert:
    case OpSConvert:
    case OpFConvert:
    case OpConvertPtrToU:
    // TODO: case OpConvertUToPtr:
    case OpPtrCastToGeneric:
    // TODO: case OpGenericCastToPtr:
    case OpBitcast:
    // TODO: case OpGenericCastToPtrExplicit:
    case OpSatConvertSToU:
    case OpSatConvertUToS:
    case OpVectorExtractDynamic:
    case OpCompositeConstruct:
    case OpCompositeExtract:
    case OpCopyObject:
    case OpTranspose:
    case OpSNegate:
    case OpFNegate:
    case OpNot:
    case OpIAdd:
    case OpFAdd:
    case OpISub:
    case OpFSub:
    case OpIMul:
    case OpFMul:
    case OpUDiv:
    case OpSDiv:
    case OpFDiv:
    case OpUMod:
    case OpSRem:
    case OpSMod:
    case OpVectorTimesScalar:
    case OpMatrixTimesScalar:
    case OpVectorTimesMatrix:
    case OpMatrixTimesVector:
    case OpMatrixTimesMatrix:
    case OpOuterProduct:
    case OpDot:
    case OpShiftRightLogical:
    case OpShiftRightArithmetic:
    case OpShiftLeftLogical:
    case OpBitwiseOr:
    case OpBitwiseXor:
    case OpBitwiseAnd:
    case OpAny:
    case OpAll:
    case OpIsNan:
    case OpIsInf:
    case OpIsFinite:
    case OpIsNormal:
    case OpSignBitSet:
    case OpLessOrGreater:
    case OpOrdered:
    case OpUnordered:
    case OpLogicalOr:
    case OpLogicalXor:
    case OpLogicalAnd:
    case OpSelect:
    case OpIEqual:
    case OpFOrdEqual:
    case OpFUnordEqual:
    case OpINotEqual:
    case OpFOrdNotEqual:
    case OpFUnordNotEqual:
    case OpULessThan:
    case OpSLessThan:
    case OpFOrdLessThan:
    case OpFUnordLessThan:
    case OpUGreaterThan:
    case OpSGreaterThan:
    case OpFOrdGreaterThan:
    case OpFUnordGreaterThan:
    case OpULessThanEqual:
    case OpSLessThanEqual:
    case OpFOrdLessThanEqual:
    case OpFUnordLessThanEqual:
    case OpUGreaterThanEqual:
    case OpSGreaterThanEqual:
    case OpFOrdGreaterThanEqual:
    case OpFUnordGreaterThanEqual:
    case OpDPdx:
    case OpDPdy:
    case OpFwidth:
    case OpDPdxFine:
    case OpDPdyFine:
    case OpFwidthFine:
    case OpDPdxCoarse:
    case OpDPdyCoarse:
    case OpFwidthCoarse:
    case OpReturnValue:
      return true;
    default:
      return false;
  }
}

int32_t spvOpcodeIsBasicTypeNullable(Op opcode) {
  switch (opcode) {
    case OpTypeBool:
    case OpTypeInt:
    case OpTypeFloat:
    case OpTypePointer:
    case OpTypeEvent:
    case OpTypeDeviceEvent:
    case OpTypeReserveId:
    case OpTypeQueue:
      return true;
    default:
      return false;
  }
}

int32_t spvInstructionIsInBasicBlock(const spv_instruction_t *pFirstInst,
                                     const spv_instruction_t *pInst) {
  while (pFirstInst != pInst) {
    spvCheck(OpFunction == pInst->opcode, break);
    pInst--;
  }
  spvCheck(OpFunction != pInst->opcode, return false);
  return true;
}

int32_t spvOpcodeIsValue(Op opcode) {
  spvCheck(spvOpcodeIsPointer(opcode), return true);
  spvCheck(spvOpcodeIsConstant(opcode), return true);
  switch (opcode) {
    case OpLoad:
      // TODO: Other Opcode's resulting in a value
      return true;
    default:
      return false;
  }
}
