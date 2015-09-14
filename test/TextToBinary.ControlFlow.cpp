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

// Assembler tests for instructions in the "Control Flow" section of the
// SPIR-V spec.

#include "UnitSPIRV.h"

#include "gmock/gmock.h"
#include "TestFixture.h"

namespace {

using spvtest::MakeInstruction;
using ::testing::Eq;

// An example case for an enumerated value.
template <typename E>
struct EnumCase {
  E value;
  std::string name;
};

// Test OpSelectionMerge

using OpSelectionMergeTest = test_fixture::TextToBinaryTestBase<
    ::testing::TestWithParam<EnumCase<spv::SelectionControlMask>>>;

TEST_P(OpSelectionMergeTest, AnySingleSelectionControlMask) {
  std::string input = "OpSelectionMerge %1 " + GetParam().name;
  EXPECT_THAT(
      CompiledInstructions(input),
      Eq(MakeInstruction(spv::OpSelectionMerge, {1, GetParam().value})));
}

// clang-format off
#define CASE(VALUE,NAME) { spv::SelectionControl##VALUE, NAME}
INSTANTIATE_TEST_CASE_P(TextToBinarySelectionMerge, OpSelectionMergeTest,
                        ::testing::ValuesIn(std::vector<EnumCase<spv::SelectionControlMask>>{
                            CASE(MaskNone, "None"),
                            CASE(FlattenMask, "Flatten"),
                            CASE(DontFlattenMask, "DontFlatten"),
                        }));
#undef CASE
// clang-format on

// TODO(dneto): Combination of selection control masks.

// TODO(dneto): OpPhi
// TODO(dneto): OpLoopMerge
// TODO(dneto): OpLabel
// TODO(dneto): OpBranch
// TODO(dneto): OpSwitch
// TODO(dneto): OpKill
// TODO(dneto): OpReturn
// TODO(dneto): OpReturnValue
// TODO(dneto): OpUnreachable
// TODO(dneto): OpLifetimeStart
// TODO(dneto): OpLifetimeStop

}  // anonymous namespace
