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

// Assembler tests for instructions in the "Barrier Instructions" section
// of the SPIR-V spec.

#include "UnitSPIRV.h"

#include "gmock/gmock.h"
#include "TestFixture.h"

namespace {

using spvtest::MakeInstruction;
using spvtest::TextToBinaryTest;
using ::testing::Eq;

// Test OpMemoryBarrier

using OpMemoryBarrier = spvtest::TextToBinaryTest;

TEST_F(OpMemoryBarrier, Good) {
  const std::string input = "OpMemoryBarrier %1 %2\n";
  EXPECT_THAT(CompiledInstructions(input),
              Eq(MakeInstruction(SpvOpMemoryBarrier, {1, 2})));
  EXPECT_THAT(EncodeAndDecodeSuccessfully(input), Eq(input));
}

TEST_F(OpMemoryBarrier, BadMissingScopeId) {
  const std::string input = "OpMemoryBarrier\n";
  EXPECT_THAT(CompileFailure(input),
              Eq("Expected operand, found end of stream."));
}

TEST_F(OpMemoryBarrier, BadInvalidScopeId) {
  const std::string input = "OpMemoryBarrier 99\n";
  EXPECT_THAT(CompileFailure(input),
              Eq("Expected id to start with %."));
}

TEST_F(OpMemoryBarrier, BadMissingMemorySemanticsId) {
  const std::string input = "OpMemoryBarrier %scope\n";
  EXPECT_THAT(CompileFailure(input),
              Eq("Expected operand, found end of stream."));
}

TEST_F(OpMemoryBarrier, BadInvalidMemorySemanticsId) {
  const std::string input = "OpMemoryBarrier %scope 14\n";
  EXPECT_THAT(CompileFailure(input),
              Eq("Expected id to start with %."));
}

// TODO(dneto): OpControlBarrier
// TODO(dneto): OpAsyncGroupCopy
// TODO(dneto): OpWaitGroupEvents
// TODO(dneto): OpGroupAll
// TODO(dneto): OpGroupAny
// TODO(dneto): OpGroupBroadcast
// TODO(dneto): OpGroupIAdd
// TODO(dneto): OpGroupFAdd
// TODO(dneto): OpGroupFMin
// TODO(dneto): OpGroupUMin
// TODO(dneto): OpGroupSMin
// TODO(dneto): OpGroupFMax
// TODO(dneto): OpGroupUMax
// TODO(dneto): OpGroupSMax

}  // anonymous namespace
