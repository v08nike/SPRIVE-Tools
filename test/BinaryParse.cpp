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

#include "UnitSPIRV.h"

#include "TestFixture.h"
#include "gmock/gmock.h"

// Returns true if two spv_parsed_operand_t values are equal.
// To use this operator, this definition must appear in the same namespace
// as spv_parsed_operand_t.
static bool operator==(const spv_parsed_operand_t& a,
                       const spv_parsed_operand_t& b) {
  return a.offset == b.offset && a.num_words == b.num_words &&
         a.type == b.type && a.number_kind == b.number_kind &&
         a.number_bit_width == b.number_bit_width;
}

namespace {

using ::spvtest::Concatenate;
using ::spvtest::MakeInstruction;
using ::spvtest::MakeVector;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;

// An easily-constructible and comparable object for the contents of an
// spv_parsed_instruction_t.  Unlike spv_parsed_instruction_t, owns the memory
// of its components.
struct ParsedInstruction {
  ParsedInstruction(const spv_parsed_instruction_t& inst)
      : words(inst.words, inst.words + inst.num_words),
        opcode(inst.opcode),
        ext_inst_type(inst.ext_inst_type),
        type_id(inst.type_id),
        result_id(inst.result_id),
        operands(inst.operands, inst.operands + inst.num_operands) {}

  std::vector<uint32_t> words;
  SpvOp opcode;
  spv_ext_inst_type_t ext_inst_type;
  uint32_t type_id;
  uint32_t result_id;
  std::vector<spv_parsed_operand_t> operands;

  bool operator==(const ParsedInstruction& b) const {
    return words == b.words && opcode == b.opcode &&
           ext_inst_type == b.ext_inst_type && type_id == b.type_id &&
           result_id == b.result_id && operands == b.operands;
  }
};

// Prints a ParsedInstruction object to the given output stream, and returns
// the stream.
std::ostream& operator<<(std::ostream& os, const ParsedInstruction& inst) {
  os << "\nParsedInstruction( {";
  spvtest::PrintTo(spvtest::WordVector(inst.words), &os);
  os << "}, opcode: " << int(inst.opcode)
     << " ext_inst_type: " << int(inst.ext_inst_type)
     << " type_id: " << inst.type_id << " result_id: " << inst.result_id;
  for (const auto& operand : inst.operands) {
    os << " { offset: " << operand.offset << " num_words: " << operand.num_words
       << " type: " << int(operand.type)
       << " number_kind: " << int(operand.number_kind)
       << " number_bit_width: " << int(operand.number_bit_width) << "}";
  }
  os << ")";
  return os;
}

// Sanity check for the equality operator on ParsedInstruction.
TEST(ParsedInstruction, ZeroInitializedAreEqual) {
  spv_parsed_instruction_t pi = {};
  ParsedInstruction a(pi);
  ParsedInstruction b(pi);
  EXPECT_THAT(a, ::testing::TypedEq<ParsedInstruction>(b));
}

// A template class with static member functions that forward to non-static
// member functions corresponding to the header and instruction callbacks
// for spvBinaryParse.
template <typename Client, typename T>
class BinaryParseClient : public T {
 public:
  static spv_result_t Header(void* user_data, spv_endianness_t endian,
                             uint32_t magic, uint32_t version,
                             uint32_t generator, uint32_t id_bound,
                             uint32_t reserved) {
    return static_cast<Client*>(user_data)
        ->HandleHeader(endian, magic, version, generator, id_bound, reserved);
  }

  static spv_result_t Instruction(
      void* user_data, const spv_parsed_instruction_t* parsed_instruction) {
    return static_cast<Client*>(user_data)
        ->HandleInstruction(parsed_instruction);
  }
};

using Words = std::vector<uint32_t>;
using Endians = std::vector<spv_endianness_t>;
using Sentences = std::vector<Words>;  // Maybe this is too cute?
using Instructions = std::vector<ParsedInstruction>;

// A binary parse client that captures the results of parsing a binary,
// and whose callbacks can be made to succeed for a specified number of
// times, and then always fail with a given failure code.
template <typename T>
class CaptureParseResults
    : public BinaryParseClient<CaptureParseResults<T>, T> {
 public:
  // Capture the header from the parser callback.
  // If the number of callback successes has not yet been exhausted, then
  // returns SPV_SUCCESS, which itself counts as a callback success.
  // Otherwise returns the stored failure code.
  virtual spv_result_t HandleHeader(spv_endianness_t endian, uint32_t magic,
                                    uint32_t version, uint32_t generator,
                                    uint32_t id_bound, uint32_t reserved) {
    endians_.push_back(endian);
    headers_.push_back({magic, version, generator, id_bound, reserved});
    return ComputeResultCode();
  }

  // Capture the parsed instruction data from the parser callback.
  // If the number of callback successes has not yet been exhausted, then
  // returns SPV_SUCCESS, which itself counts as a callback success.
  // Otherwise returns the stored failure code.
  virtual spv_result_t HandleInstruction(
      const spv_parsed_instruction_t* parsed_instruction) {
    EXPECT_NE(nullptr, parsed_instruction);
    instructions_.emplace_back(*parsed_instruction);
    return ComputeResultCode();
  }

  // Getters
  const Endians& endians() const { return endians_; }
  const Sentences& headers() const { return headers_; }
  const Instructions& instructions() const { return instructions_; }

 protected:
  // Returns the appropriate result code based on whether we still have more
  // successes to return.  Decrements the number of successes still remaining,
  // if needed.
  spv_result_t ComputeResultCode() {
    if (num_passing_callbacks_ < 0) return SPV_SUCCESS;
    if (num_passing_callbacks_ == 0) return fail_code_;
    num_passing_callbacks_--;
    return SPV_SUCCESS;
  }

  // How many callbacks should succeed before they start failing?
  // If this is negative, then all callbacks should pass.
  int num_passing_callbacks_ = -1;  // By default, never fail.
  // The result code to use on callback failure.
  spv_result_t fail_code_ = SPV_ERROR_INVALID_BINARY;

  // Accumulated results for calls to HandleHeader.
  Endians endians_;
  Sentences headers_;

  // Accumulated results for calls to HandleHeader.
  Instructions instructions_;
};

// The SPIR-V module header words for the Khronos Assembler generator,
// for a module with an ID bound of 1.
const uint32_t kHeaderForBound1[] = {
    SpvMagicNumber, SpvVersion,
    SPV_GENERATOR_WORD(SPV_GENERATOR_KHRONOS_ASSEMBLER, 0), 1 /*bound*/,
    0 /*schema*/};

// Returns the expected SPIR-V module header words for the Khronos
// Assembler generator, and with a given Id bound.
Words ExpectedHeaderForBound(uint32_t bound) {
  Words result{std::begin(kHeaderForBound1), std::end(kHeaderForBound1)};
  result[SPV_INDEX_BOUND] = bound;
  return result;
}

// Returns a parsed operand for a non-number value at the given word offset
// within an instruction.
spv_parsed_operand_t MakeSimpleOperand(uint16_t offset,
                                       spv_operand_type_t type) {
  return {offset, 1, type, SPV_NUMBER_NONE, 0};
}

// Returns a parsed operand for a literal unsigned integer value at the given
// word offset within an instruction.
spv_parsed_operand_t MakeLiteralNumberOperand(uint16_t offset) {
  return {offset, 1, SPV_OPERAND_TYPE_LITERAL_INTEGER, SPV_NUMBER_UNSIGNED_INT,
          32};
}

// Returns a parsed operand for a literal string value at the given
// word offset within an instruction.
spv_parsed_operand_t MakeLiteralStringOperand(uint16_t offset,
                                              uint16_t length) {
  return {offset, length, SPV_OPERAND_TYPE_LITERAL_STRING, SPV_NUMBER_NONE, 0};
}

// Returns a ParsedInstruction for an OpTypeVoid instruction that would
// generate the given result Id.
ParsedInstruction MakeParsedVoidTypeInstruction(uint32_t result_id) {
  const auto void_inst = MakeInstruction(SpvOpTypeVoid, {result_id});
  const auto void_operands = std::vector<spv_parsed_operand_t>{
      MakeSimpleOperand(1, SPV_OPERAND_TYPE_RESULT_ID)};
  const spv_parsed_instruction_t parsed_void_inst = {
      void_inst.data(),
      uint16_t(void_inst.size()),
      SpvOpTypeVoid,
      SPV_EXT_INST_TYPE_NONE,
      0,  // type id
      result_id,
      void_operands.data(),
      uint16_t(void_operands.size())};
  return ParsedInstruction(parsed_void_inst);
}

// Returns a ParsedInstruction for an OpTypeInt instruction that generates
// the given result Id for a 32-bit signed integer scalar type.
ParsedInstruction MakeParsedInt32TypeInstruction(uint32_t result_id) {
  const auto i32_inst = MakeInstruction(SpvOpTypeInt, {result_id, 32, 1});
  const auto i32_operands = std::vector<spv_parsed_operand_t>{
      MakeSimpleOperand(1, SPV_OPERAND_TYPE_RESULT_ID),
      MakeLiteralNumberOperand(2), MakeLiteralNumberOperand(3)};
  spv_parsed_instruction_t parsed_i32_inst = {i32_inst.data(),
                                              uint16_t(i32_inst.size()),
                                              SpvOpTypeInt,
                                              SPV_EXT_INST_TYPE_NONE,
                                              0,  // type id
                                              result_id,
                                              i32_operands.data(),
                                              uint16_t(i32_operands.size())};
  return ParsedInstruction(parsed_i32_inst);
}

using BinaryParseTest =
    spvtest::TextToBinaryTestBase<CaptureParseResults<::testing::Test>>;

TEST_F(BinaryParseTest, EmptyModuleHasValidHeaderAndNoInstructionCallbacks) {
  const auto binary = CompileSuccessfully("");
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_SUCCESS,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, &diagnostic));
  EXPECT_EQ(nullptr, diagnostic);
  EXPECT_THAT(endians(), AnyOf(Eq(Endians{SPV_ENDIANNESS_LITTLE}),
                               Eq(Endians{SPV_ENDIANNESS_BIG})));
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(1)}));
  EXPECT_THAT(instructions(), Eq(Instructions{}));
}

TEST_F(BinaryParseTest,
       ModuleWithSingleInstructionHasValidHeaderAndInstructionCallback) {
  const auto binary = CompileSuccessfully("%1 = OpTypeVoid");
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_SUCCESS,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, &diagnostic));
  EXPECT_EQ(nullptr, diagnostic);
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(2)}));
  EXPECT_THAT(instructions(),
              Eq(Instructions{MakeParsedVoidTypeInstruction(1)}));
}

TEST_F(BinaryParseTest, NullHeaderCallbackIsIgnored) {
  const auto binary = CompileSuccessfully("%1 = OpTypeVoid");
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_SUCCESS,
            spvBinaryParse(context, this, binary.data(), binary.size(), nullptr,
                           Instruction, &diagnostic));
  EXPECT_EQ(nullptr, diagnostic);
  EXPECT_THAT(headers(), Eq(Sentences{}));  // No header callback.
  EXPECT_THAT(instructions(),
              Eq(Instructions{MakeParsedVoidTypeInstruction(1)}));
}

TEST_F(BinaryParseTest, NullInstructionCallbackIsIgnored) {
  const auto binary = CompileSuccessfully("%1 = OpTypeVoid");
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_SUCCESS,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           nullptr, &diagnostic));
  EXPECT_EQ(nullptr, diagnostic);
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(2)}));
  EXPECT_THAT(instructions(), Eq(Instructions{}));  // No instruction callback.
}

// Check the result of multiple instruction callbacks.
//
// This test exercises non-default values for the following members of the
// spv_parsed_instruction_t struct: words, num_words, opcode, result_id,
// operands, num_operands.
TEST_F(BinaryParseTest, TwoScalarTypesGenerateTwoInstructionCallbacks) {
  const auto binary = CompileSuccessfully(
      "%1 = OpTypeVoid "
      "%2 = OpTypeInt 32 1");
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_SUCCESS,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, &diagnostic));
  EXPECT_EQ(nullptr, diagnostic);

  // The Id bound must be computed correctly.  The module has two generated Ids,
  // so the bound is 3.
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(3)}));

  // The instructions callbacks must have the correct data.
  EXPECT_THAT(instructions(), Eq(Instructions{
                                  MakeParsedVoidTypeInstruction(1),
                                  MakeParsedInt32TypeInstruction(2),
                              }));
}

TEST_F(BinaryParseTest, EarlyReturnWithZeroPassingCallbacks) {
  const auto binary = CompileSuccessfully(
      "%1 = OpTypeVoid "
      "%2 = OpTypeInt 32 1");

  num_passing_callbacks_ = 0;

  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, &diagnostic));
  // On error, the binary parser doesn't generate its own diagnostics.
  EXPECT_EQ(nullptr, diagnostic);

  // Early termination is registered after we have saved the header result.
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(3)}));
  // The instruction callbacks are never called.
  EXPECT_THAT(instructions(), Eq(Instructions{}));
}

TEST_F(BinaryParseTest,
       EarlyReturnWithZeroPassingCallbacksAndSpecifiedResultCode) {
  const auto binary = CompileSuccessfully(
      "%1 = OpTypeVoid "
      "%2 = OpTypeInt 32 1");

  num_passing_callbacks_ = 0;
  fail_code_ = SPV_REQUESTED_TERMINATION;

  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_REQUESTED_TERMINATION,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, &diagnostic));
  // On early termination, the binary parser doesn't generate its own
  // diagnostics.
  EXPECT_EQ(nullptr, diagnostic);
  // Early exit is registered after we have saved the header result.
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(3)}));
  // The instruction callbacks are never called.
  EXPECT_THAT(instructions(), Eq(Instructions{}));
}

TEST_F(BinaryParseTest, EarlyReturnWithOnePassingCallback) {
  const auto binary = CompileSuccessfully(
      "%1 = OpTypeVoid "
      "%2 = OpTypeInt 32 1 "
      "%3 = OpTypeFloat 32");

  num_passing_callbacks_ = 1;
  fail_code_ = SPV_REQUESTED_TERMINATION;
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_REQUESTED_TERMINATION,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, &diagnostic));
  // On early termination, the binary parser doesn't generate its own
  // diagnostics.
  EXPECT_EQ(nullptr, diagnostic);

  // Early termination is registered after we have saved the header result.
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(4)}));
  // The header callback succeeded.  Then the instruction callback was called
  // once (on the first instruction), and then it requested termination,
  // preventing further instruction callbacks.
  EXPECT_THAT(instructions(), Eq(Instructions{
                                  MakeParsedVoidTypeInstruction(1),
                              }));
}

TEST_F(BinaryParseTest, EarlyReturnWithTwoPassingCallbacks) {
  const auto binary = CompileSuccessfully(
      "%1 = OpTypeVoid "
      "%2 = OpTypeInt 32 1 "
      "%3 = OpTypeFloat 32");

  num_passing_callbacks_ = 2;
  fail_code_ = SPV_REQUESTED_TERMINATION;
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(SPV_REQUESTED_TERMINATION,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, &diagnostic));
  // On early termination, the binary parser doesn't generate its own
  // diagnostics.
  EXPECT_EQ(nullptr, diagnostic);

  // Early termination is registered after we have saved the header result.
  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(4)}));
  // The header callback succeeded.  Then the instruction callback was
  // called twice and then it requested termination, preventing further
  // instruction callbacks.
  EXPECT_THAT(instructions(), Eq(Instructions{
                                  MakeParsedVoidTypeInstruction(1),
                                  MakeParsedInt32TypeInstruction(2),
                              }));
}

TEST_F(BinaryParseTest, InstructionWithStringOperand) {
  const std::string str =
      "the future is already here, it's just not evenly distributed";
  const auto str_words = MakeVector(str);
  const auto instruction = MakeInstruction(SpvOpName, {99}, str_words);
  const auto binary = Concatenate({ExpectedHeaderForBound(100), instruction});

  EXPECT_EQ(SPV_SUCCESS,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, nullptr));

  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(100)}));

  const auto operands = std::vector<spv_parsed_operand_t>{
      MakeSimpleOperand(1, SPV_OPERAND_TYPE_ID),
      MakeLiteralStringOperand(2, uint16_t(str_words.size()))};
  const spv_parsed_instruction_t parsed_inst = {instruction.data(),
                                                uint16_t(instruction.size()),
                                                SpvOpName,
                                                SPV_EXT_INST_TYPE_NONE,
                                                0,  // type id
                                                0,  // No result id for OpName
                                                operands.data(),
                                                uint16_t(operands.size())};
  EXPECT_THAT(instructions(), Eq(Instructions{ParsedInstruction(parsed_inst)}));
}

// Checks for non-zero values for the result_id and ext_inst_type members
// spv_parsed_instruction_t.
TEST_F(BinaryParseTest, ExtendedInstruction) {
  const auto binary = CompileSuccessfully(
      "%extcl = OpExtInstImport \"OpenCL.std\" "
      "%result = OpExtInst %float %extcl sqrt %x");

  EXPECT_EQ(SPV_SUCCESS,
            spvBinaryParse(context, this, binary.data(), binary.size(), Header,
                           Instruction, nullptr));

  EXPECT_THAT(headers(), Eq(Sentences{ExpectedHeaderForBound(5)}));

  const auto operands = std::vector<spv_parsed_operand_t>{
      MakeSimpleOperand(1, SPV_OPERAND_TYPE_TYPE_ID),
      MakeSimpleOperand(2, SPV_OPERAND_TYPE_RESULT_ID),
      MakeSimpleOperand(3, SPV_OPERAND_TYPE_ID),  // Extended instruction set Id
      MakeSimpleOperand(4, SPV_OPERAND_TYPE_EXTENSION_INSTRUCTION_NUMBER),
      MakeSimpleOperand(5, SPV_OPERAND_TYPE_ID),  // Id of the argument
  };
  const auto instruction = MakeInstruction(
      SpvOpExtInst, {2, 3, 1, uint32_t(OpenCLLIB::Entrypoints::Sqrt), 4});
  const spv_parsed_instruction_t parsed_inst = {instruction.data(),
                                                uint16_t(instruction.size()),
                                                SpvOpExtInst,
                                                SPV_EXT_INST_TYPE_OPENCL_STD,
                                                2,  // type id
                                                3,  // result id
                                                operands.data(),
                                                uint16_t(operands.size())};
  EXPECT_THAT(instructions(), ElementsAre(_, ParsedInstruction(parsed_inst)));
}

// A binary parser diagnostic test case where we provide the words array
// pointer and word count explicitly.
struct WordsAndCountDiagnosticCase {
  const uint32_t* words;
  size_t num_words;
  std::string expected_diagnostic;
};

using BinaryParseWordsAndCountDiagnosticTest = spvtest::TextToBinaryTestBase<
    ::testing::TestWithParam<WordsAndCountDiagnosticCase>>;

TEST_P(BinaryParseWordsAndCountDiagnosticTest, WordAndCountCases) {
  spv_diagnostic diagnostic = nullptr;
  EXPECT_EQ(
      SPV_ERROR_INVALID_BINARY,
      spvBinaryParse(context, this, GetParam().words, GetParam().num_words,
                     nullptr, nullptr, &diagnostic));
  ASSERT_NE(nullptr, diagnostic);
  EXPECT_THAT(diagnostic->error, Eq(GetParam().expected_diagnostic));
}

INSTANTIATE_TEST_CASE_P(
    BinaryParseDiagnostic, BinaryParseWordsAndCountDiagnosticTest,
    ::testing::ValuesIn(std::vector<WordsAndCountDiagnosticCase>{
        {nullptr, 0, "Missing module."},
        {kHeaderForBound1, 0,
         "Module has incomplete header: only 0 words instead of 5"},
        {kHeaderForBound1, 1,
         "Module has incomplete header: only 1 words instead of 5"},
        {kHeaderForBound1, 2,
         "Module has incomplete header: only 2 words instead of 5"},
        {kHeaderForBound1, 3,
         "Module has incomplete header: only 3 words instead of 5"},
        {kHeaderForBound1, 4,
         "Module has incomplete header: only 4 words instead of 5"},
    }));

// A binary parser diagnostic test case where a vector of words is
// provided.  We'll use this to express cases that can't be created
// via the assembler.  Either we want to make a malformed instruction,
// or an invalid case the assembler would reject.
struct WordVectorDiagnosticCase {
  Words words;
  std::string expected_diagnostic;
};

using BinaryParseWordVectorDiagnosticTest = spvtest::TextToBinaryTestBase<
    ::testing::TestWithParam<WordVectorDiagnosticCase>>;

TEST_P(BinaryParseWordVectorDiagnosticTest, WordVectorCases) {
  spv_diagnostic diagnostic = nullptr;
  const auto& words = GetParam().words;
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            spvBinaryParse(context, this, words.data(), words.size(), nullptr,
                           nullptr, &diagnostic));
  ASSERT_NE(nullptr, diagnostic);
  EXPECT_THAT(diagnostic->error, Eq(GetParam().expected_diagnostic));
}

INSTANTIATE_TEST_CASE_P(
    BinaryParseDiagnostic, BinaryParseWordVectorDiagnosticTest,
    ::testing::ValuesIn(std::vector<WordVectorDiagnosticCase>{
        {Concatenate({ExpectedHeaderForBound(1), {spvOpcodeMake(0, SpvOpNop)}}),
         "Invalid instruction word count: 0"},
        {Concatenate({ExpectedHeaderForBound(1),
                      {spvOpcodeMake(1, static_cast<SpvOp>(0xffff))}}),
         "Invalid opcode: 65535"},
        {Concatenate({ExpectedHeaderForBound(1),
                      MakeInstruction(SpvOpNop, {42})}),
         "Invalid instruction OpNop starting at word 5: expected "
         "no more operands after 1 words, but stated word count is 2."},
        {Concatenate({ExpectedHeaderForBound(1),
                      MakeInstruction(SpvOpTypeVoid, {1, 2})}),
         "Invalid instruction OpTypeVoid starting at word 5: expected "
         "no more operands after 2 words, but stated word count is 3."},
        {Concatenate({ExpectedHeaderForBound(1),
                      MakeInstruction(SpvOpTypeVoid, {1, 2, 5, 9, 10})}),
         "Invalid instruction OpTypeVoid starting at word 5: expected "
         "no more operands after 2 words, but stated word count is 6."},
        {Concatenate({ExpectedHeaderForBound(1),
                      MakeInstruction(SpvOpTypeInt, {1, 32, 1, 9})}),
         "Invalid instruction OpTypeInt starting at word 5: expected "
         "no more operands after 4 words, but stated word count is 5."},
        {Concatenate({ExpectedHeaderForBound(1),
                      MakeInstruction(SpvOpTypeInt, {1})}),
         "End of input reached while decoding OpTypeInt starting at word 5:"
         " expected more operands after 2 words."},

        // Check several cases for running off the end of input.

        // Detect a missing single word operand.
        {Concatenate({ExpectedHeaderForBound(1),
                      {spvOpcodeMake(2, SpvOpTypeStruct)}}),
         "End of input reached while decoding OpTypeStruct starting at word"
         " 5: missing result ID operand at word offset 1."},
        // Detect this a missing a multi-word operand to OpConstant.
        // We also lie and say the OpConstant instruction has 5 words when
        // it only has 3.  Corresponds to something like this:
        //    %1 = OpTypeInt 64 0
        //    %2 = OpConstant %1 <missing>
        {Concatenate({ExpectedHeaderForBound(3),
                      {MakeInstruction(SpvOpTypeInt, {1, 64, 0})},
                      {spvOpcodeMake(5, SpvOpConstant), 1, 2}}),
         "End of input reached while decoding OpConstant starting at word"
         " 9: missing possibly multi-word literal number operand at word "
         "offset 3."},
        // Detect when we provide only one word from the 64-bit literal,
        // and again lie about the number of words in the instruction.
        {Concatenate({ExpectedHeaderForBound(3),
                      {MakeInstruction(SpvOpTypeInt, {1, 64, 0})},
                      {spvOpcodeMake(5, SpvOpConstant), 1, 2, 42}}),
         "End of input reached while decoding OpConstant starting at word"
         " 9: truncated possibly multi-word literal number operand at word "
         "offset 3."},
        // Detect when a required string operand is missing.
        // Also, lie about the length of the instruction.
        {Concatenate({ExpectedHeaderForBound(3),
                      {spvOpcodeMake(3, SpvOpString), 1}}),
         "End of input reached while decoding OpString starting at word"
         " 5: missing literal string operand at word offset 2."},
        // Detect when a required string operand is truncated: it's missing
        // a null terminator.  Catching the error avoids a buffer overrun.
        {Concatenate({ExpectedHeaderForBound(3),
                      {spvOpcodeMake(4, SpvOpString), 1, 0x41414141,
                       0x41414141}}),
         "End of input reached while decoding OpString starting at word"
         " 5: truncated literal string operand at word offset 2."},
        // Detect when an optional string operand is truncated: it's missing
        // a null terminator.  Catching the error avoids a buffer overrun.
        // (It is valid for an optional string operand to be absent.)
        {Concatenate({ExpectedHeaderForBound(3),
                      {spvOpcodeMake(6, SpvOpSource),
                       uint32_t(SpvSourceLanguageOpenCL_C), 210,
                       1 /* file id */,
                       /*start of string*/ 0x41414141, 0x41414141}}),
         "End of input reached while decoding OpSource starting at word"
         " 5: truncated literal string operand at word offset 4."},

        // (End of input exhaustion test cases.)

        // In this case the instruction word count is too small, where
        // it would truncate a multi-word operand to OpConstant.
        {Concatenate({ExpectedHeaderForBound(3),
                      {MakeInstruction(SpvOpTypeInt, {1, 64, 0})},
                      {spvOpcodeMake(4, SpvOpConstant), 1, 2, 44, 44}}),
         "Invalid word count: OpConstant starting at word 9 says it has 4"
         " words, but found 5 words instead."},
        // Word count is to small, where it would truncate a literal string.
        {Concatenate({ExpectedHeaderForBound(2),
                      {spvOpcodeMake(3, SpvOpString), 1, 0x41414141, 0}}),
         "Invalid word count: OpString starting at word 5 says it has 3"
         " words, but found 4 words instead."},
        {Concatenate({ExpectedHeaderForBound(2),
                      {spvOpcodeMake(2, SpvOpTypeVoid), 0}}),
         "Error: Result Id is 0"},
        {Concatenate({
             ExpectedHeaderForBound(2),
             {spvOpcodeMake(2, SpvOpTypeVoid), 1},
             {spvOpcodeMake(2, SpvOpTypeBool), 1},
         }),
         "Id 1 is defined more than once"},
        {Concatenate({ExpectedHeaderForBound(3),
                      MakeInstruction(SpvOpExtInst, {2, 3, 100, 4, 5})}),
         "OpExtInst set Id 100 does not reference an OpExtInstImport result "
         "Id"},
        {Concatenate({ExpectedHeaderForBound(3),
                      MakeInstruction(SpvOpSwitch, {1, 2, 42, 3})}),
         "Invalid OpSwitch: selector id 1 has no type"},
        {Concatenate({ExpectedHeaderForBound(3),
                      MakeInstruction(SpvOpTypeInt, {1, 32, 0}),
                      MakeInstruction(SpvOpSwitch, {1, 3, 42, 3})}),
         "Invalid OpSwitch: selector id 1 is a type, not a value"},
        {Concatenate({ExpectedHeaderForBound(3),
                      MakeInstruction(SpvOpTypeFloat, {1, 32}),
                      MakeInstruction(SpvOpConstant, {1, 2, 0x78f00000}),
                      MakeInstruction(SpvOpSwitch, {2, 3, 42, 3})}),
         "Invalid OpSwitch: selector id 2 is not a scalar integer"},
        {Concatenate({ExpectedHeaderForBound(3),
                      MakeInstruction(SpvOpExtInstImport, {1},
                                      MakeVector("invalid-import"))}),
         "Invalid extended instruction import 'invalid-import'"},
        {Concatenate({
             ExpectedHeaderForBound(3),
             MakeInstruction(SpvOpTypeInt, {1, 32, 0}),
             MakeInstruction(SpvOpConstant, {2, 2, 42}),
         }),
         "Type Id 2 is not a type"},
        {Concatenate({
             ExpectedHeaderForBound(3), MakeInstruction(SpvOpTypeBool, {1}),
             MakeInstruction(SpvOpConstant, {1, 2, 42}),
         }),
         "Type Id 1 is not a scalar numeric type"},
    }));

// A binary parser diagnostic case generated from an assembly text input.
struct AssemblyDiagnosticCase {
  std::string assembly;
  std::string expected_diagnostic;
};

using BinaryParseAssemblyDiagnosticTest = spvtest::TextToBinaryTestBase<
    ::testing::TestWithParam<AssemblyDiagnosticCase>>;

TEST_P(BinaryParseAssemblyDiagnosticTest, AssemblyCases) {
  spv_diagnostic diagnostic = nullptr;
  auto words = CompileSuccessfully(GetParam().assembly);
  EXPECT_EQ(SPV_ERROR_INVALID_BINARY,
            spvBinaryParse(context, this, words.data(), words.size(), nullptr,
                           nullptr, &diagnostic));
  ASSERT_NE(nullptr, diagnostic);
  EXPECT_THAT(diagnostic->error, Eq(GetParam().expected_diagnostic));
}

INSTANTIATE_TEST_CASE_P(
    BinaryParseDiagnostic, BinaryParseAssemblyDiagnosticTest,
    ::testing::ValuesIn(std::vector<AssemblyDiagnosticCase>{
        {"%1 = OpConstant !0 42", "Error: Type Id is 0"},
        // A required id is 0.
        {"OpName !0 \"foo\"", "Id is 0"},
        // An optional id is 0, in this case the optional
        // initializer.
        {"%2 = OpVariable %1 CrossWorkgroup !0", "Id is 0"},
        {"OpControlBarrier !0 %1 %2", "scope ID is 0"},
        {"OpControlBarrier %1 !0 %2", "scope ID is 0"},
        {"OpControlBarrier %1 %2 !0", "memory semantics ID is 0"},
        {"%import = OpExtInstImport \"GLSL.std.450\" "
         "%result = OpExtInst %type %import !999999 %x",
         "Invalid extended instruction number: 999999"},
        {"%2 = OpSpecConstantOp %1 !1000 %2",
         "Invalid OpSpecConstantOp opcode: 1000"},
        {"OpCapability !9999", "Invalid capability operand: 9999"},
        {"OpSource !9999 100", "Invalid source language operand: 9999"},
        {"OpEntryPoint !9999", "Invalid execution model operand: 9999"},
        {"OpMemoryModel !9999", "Invalid addressing model operand: 9999"},
        {"OpMemoryModel Logical !9999", "Invalid memory model operand: 9999"},
        {"OpExecutionMode %1 !9999", "Invalid execution mode operand: 9999"},
        {"OpTypeForwardPointer %1 !9999",
         "Invalid storage class operand: 9999"},
        {"%2 = OpTypeImage %1 !9999", "Invalid dimensionality operand: 9999"},
        {"%2 = OpTypeImage %1 1D 0 0 0 0 !9999",
         "Invalid image format operand: 9999"},
        {"OpDecorate %1 FPRoundingMode !9999",
         "Invalid floating-point rounding mode operand: 9999"},
        {"OpDecorate %1 LinkageAttributes \"C\" !9999",
         "Invalid linkage type operand: 9999"},
        {"%1 = OpTypePipe !9999", "Invalid access qualifier operand: 9999"},
        {"OpDecorate %1 FuncParamAttr !9999",
         "Invalid function parameter attribute operand: 9999"},
        {"OpDecorate %1 !9999", "Invalid decoration operand: 9999"},
        {"OpDecorate %1 BuiltIn !9999", "Invalid built-in operand: 9999"},
        {"%2 = OpGroupIAdd %1 %3 !9999",
         "Invalid group operation operand: 9999"},
        {"OpDecorate %1 FPFastMathMode !63",
         "Invalid floating-point fast math mode operand: 63 has invalid mask "
         "component 32"},
        {"%2 = OpFunction %2 !31",
         "Invalid function control operand: 31 has invalid mask component 16"},
        {"OpLoopMerge %1 %2 !7",
         "Invalid loop control operand: 7 has invalid mask component 4"},
        {"%2 = OpImageFetch %1 %image %coord !511",
         "Invalid image operand: 511 has invalid mask component 256"},
        {"OpSelectionMerge %1 !7",
         "Invalid selection control operand: 7 has invalid mask component 4"},
    }));

}  // anonymous namespace
