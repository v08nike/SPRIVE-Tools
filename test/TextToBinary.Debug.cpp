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

// Assembler tests for instructions in the "Debug" section of the
// SPIR-V spec.

#include "UnitSPIRV.h"

#include <string>

#include "gmock/gmock.h"
#include "TestFixture.h"

namespace {

using spvtest::MakeInstruction;
using spvtest::MakeVector;
using spvtest::TextToBinaryTest;
using ::testing::Eq;

// Test OpSource

// A single test case for OpSource
struct LanguageCase {
  uint32_t get_language_value() const {
    return static_cast<uint32_t>(language_value);
  }
  const char* language_name;
  spv::SourceLanguage language_value;
  uint32_t version;
};

// clang-format off
// The list of OpSource cases to use.
const LanguageCase kLanguageCases[] = {
#define CASE(NAME, VERSION) \
  { #NAME, spv::SourceLanguage##NAME, VERSION }
  CASE(Unknown, 0),
  CASE(Unknown, 999),
  CASE(ESSL, 310),
  CASE(GLSL, 450),
  CASE(OpenCL, 210),
#undef CASE
};
// clang-format on

using OpSourceTest =
    spvtest::TextToBinaryTestBase<::testing::TestWithParam<LanguageCase>>;

TEST_P(OpSourceTest, AnyLanguage) {
  std::string input = std::string("OpSource ") + GetParam().language_name +
                      " " + std::to_string(GetParam().version);
  EXPECT_THAT(CompiledInstructions(input),
              Eq(MakeInstruction(spv::OpSource, {GetParam().get_language_value(),
                                                 GetParam().version})));
}

INSTANTIATE_TEST_CASE_P(TextToBinaryTestDebug, OpSourceTest,
                        ::testing::ValuesIn(kLanguageCases));

// Test OpSourceContinued

using OpSourceContinuedTest =
    spvtest::TextToBinaryTestBase<::testing::TestWithParam<const char*>>;

TEST_P(OpSourceContinuedTest, AnyExtension) {
  // TODO(dneto): utf-8, quoting, escaping
  std::string input = std::string("OpSourceContinued \"") + GetParam() + "\"";
  EXPECT_THAT(
      CompiledInstructions(input),
      Eq(MakeInstruction(spv::OpSourceContinued, MakeVector(GetParam()))));
}

// TODO(dneto): utf-8, quoting, escaping
INSTANTIATE_TEST_CASE_P(TextToBinaryTestDebug, OpSourceContinuedTest,
                        ::testing::ValuesIn(std::vector<const char*>{
                            "", "foo bar this and that"}));

// Test OpSourceExtension

using OpSourceExtensionTest =
    spvtest::TextToBinaryTestBase<::testing::TestWithParam<const char*>>;

TEST_P(OpSourceExtensionTest, AnyExtension) {
  // TODO(dneto): utf-8, quoting, escaping
  std::string input = std::string("OpSourceExtension \"") + GetParam() + "\"";
  EXPECT_THAT(
      CompiledInstructions(input),
      Eq(MakeInstruction(spv::OpSourceExtension, MakeVector(GetParam()))));
}

// TODO(dneto): utf-8, quoting, escaping
INSTANTIATE_TEST_CASE_P(TextToBinaryTestDebug, OpSourceExtensionTest,
                        ::testing::ValuesIn(std::vector<const char*>{
                            "", "foo bar this and that"}));

TEST_F(TextToBinaryTest, OpLine) {
  EXPECT_THAT(CompiledInstructions("OpLine %srcfile 42 99"),
              Eq(MakeInstruction(spv::OpLine, {1, 42, 99})));
}

TEST_F(TextToBinaryTest, OpNoLine) {
  EXPECT_THAT(CompiledInstructions("OpNoLine"),
              Eq(MakeInstruction(spv::OpNoLine, {})));
}

using OpStringTest =
    spvtest::TextToBinaryTestBase<::testing::TestWithParam<const char*>>;

TEST_P(OpStringTest, AnyString) {
  // TODO(dneto): utf-8, quoting, escaping
  std::string input = std::string("%result = OpString \"") + GetParam() + "\"";
  std::vector<uint32_t> expected_operands = MakeVector(GetParam());
  expected_operands.insert(expected_operands.begin(), 1);  // The ID of the result.
  EXPECT_THAT(CompiledInstructions(input),
              Eq(MakeInstruction(spv::OpString, expected_operands)));
}

// TODO(dneto): utf-8, quoting, escaping
INSTANTIATE_TEST_CASE_P(TextToBinaryTestDebug, OpStringTest,
                        ::testing::ValuesIn(std::vector<const char*>{
                            "", "foo bar this and that"}));

using OpNameTest =
    spvtest::TextToBinaryTestBase<::testing::TestWithParam<const char*>>;

TEST_P(OpNameTest, AnyString) {
  // TODO(dneto): utf-8, quoting, escaping
  std::string input = std::string("OpName %target \"") + GetParam() + "\"";
  std::vector<uint32_t> expected_operands = MakeVector(GetParam());
  expected_operands.insert(expected_operands.begin(), 1);  // The ID of the target.
  EXPECT_THAT(CompiledInstructions(input),
              Eq(MakeInstruction(spv::OpName, expected_operands)));
}

// TODO(dneto): utf-8, quoting, escaping
INSTANTIATE_TEST_CASE_P(TextToBinaryTestDebug, OpNameTest,
                        ::testing::ValuesIn(std::vector<const char*>{
                            "", "foo bar this and that"}));

using OpMemberNameTest =
    spvtest::TextToBinaryTestBase<::testing::TestWithParam<const char*>>;

TEST_P(OpMemberNameTest, AnyString) {
  // TODO(dneto): utf-8, quoting, escaping
  std::string input =
      std::string("OpMemberName %type 42 \"") + GetParam() + "\"";
  std::vector<uint32_t> expected_operands = {1, 42};
  std::vector<uint32_t> encoded_string = MakeVector(GetParam());
  expected_operands.insert(expected_operands.end(), encoded_string.begin(),
                           encoded_string.end());
  EXPECT_THAT(CompiledInstructions(input),
              Eq(MakeInstruction(spv::OpMemberName, expected_operands)));
}

// TODO(dneto): utf-8, quoting, escaping
INSTANTIATE_TEST_CASE_P(TextToBinaryTestDebug, OpMemberNameTest,
                        ::testing::ValuesIn(std::vector<const char*>{
                            "", "foo bar this and that"}));

// TODO(dneto): Parse failures?

}  // anonymous namespace
