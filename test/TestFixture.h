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
#ifndef _TEST_FIXTURE_H_
#define _TEST_FIXTURE_H_

#include "UnitSPIRV.h"

namespace test_fixture {

// Common setup for TextToBinary tests. SetText() should be called to populate
// the actual test text.
template <typename T>
class TextToBinaryTestBase : public T {
 public:
  // Shorthand for SPIR-V compilation result.
  using SpirvVector = std::vector<uint32_t>;

  // Offset into a SpirvVector at which the first instruction starts.
  const SpirvVector::size_type kFirstInstruction = 5;

  TextToBinaryTestBase()
      : opcodeTable(nullptr),
        operandTable(nullptr),
        diagnostic(nullptr),
        text(),
        binary(nullptr) {
    EXPECT_EQ(SPV_SUCCESS, spvOpcodeTableGet(&opcodeTable));
    EXPECT_EQ(SPV_SUCCESS, spvOperandTableGet(&operandTable));
    EXPECT_EQ(SPV_SUCCESS, spvExtInstTableGet(&extInstTable));
    char textStr[] = "substitute the text member variable with your test";
    text = {textStr, strlen(textStr)};
  }

  virtual ~TextToBinaryTestBase() {
    DestroyBinary();
    if (diagnostic) spvDiagnosticDestroy(diagnostic);
  }

  // Returns subvector v[from:end).
  SpirvVector Subvector(const SpirvVector& v, SpirvVector::size_type from) {
    assert(from < v.size());
    return SpirvVector(v.begin() + from, v.end());
  }

  // Compiles SPIR-V text, asserting compilation success.  Returns the compiled
  // code.
  SpirvVector CompileSuccessfully(const std::string& text) {
    spv_result_t status =
        spvTextToBinary(text.c_str(), text.size(), opcodeTable, operandTable,
                        extInstTable, &binary, &diagnostic);
    EXPECT_EQ(SPV_SUCCESS, status) << text;
    SpirvVector code_copy;
    if (status == SPV_SUCCESS) {
      code_copy = SpirvVector(binary->code, binary->code + binary->wordCount);
      DestroyBinary();
    } else {
      spvDiagnosticPrint(diagnostic);
    }
    return code_copy;
  }

  // Compiles SPIR-V text, asserting compilation failure.  Returns the error
  // message(s).
  std::string CompileFailure(const std::string& text) {
    EXPECT_NE(SPV_SUCCESS,
              spvTextToBinary(text.c_str(), text.size(), opcodeTable,
                              operandTable, extInstTable, &binary, &diagnostic))
        << text;
    DestroyBinary();
    return diagnostic->error;
  }

  // Encodes SPIR-V text into binary and then decodes the binary. Returns the
  // decoded text.
  std::string EncodeAndDecodeSuccessfully(const std::string& text) {
    DestroyBinary();
    spv_result_t error =
        spvTextToBinary(text.c_str(), text.size(), opcodeTable, operandTable,
                        extInstTable, &binary, &diagnostic);
    if (error) {
      spvDiagnosticPrint(diagnostic);
      spvDiagnosticDestroy(diagnostic);
    }
    EXPECT_EQ(SPV_SUCCESS, error);

    spv_text decoded_text;
    error = spvBinaryToText(
        binary->code, binary->wordCount, SPV_BINARY_TO_TEXT_OPTION_NONE,
        opcodeTable, operandTable, extInstTable, &decoded_text, &diagnostic);
    if (error) {
      spvDiagnosticPrint(diagnostic);
      spvDiagnosticDestroy(diagnostic);
    }
    EXPECT_EQ(SPV_SUCCESS, error);

    const std::string decoded_string = decoded_text->str;
    spvTextDestroy(decoded_text);

    return decoded_string;
  }

  // Compiles SPIR-V text, asserts success, and returns the words representing
  // the instructions.  In particular, skip the words in the SPIR-V header.
  SpirvVector CompiledInstructions(const std::string& text) {
    const SpirvVector code = CompileSuccessfully(text);
    SpirvVector result;
    // Extract just the instructions.
    // If the code fails to compile, then return the empty vector.
    // In any case, don't crash or invoke undefined behaviour.
    if (code.size() >= kFirstInstruction)
      result = Subvector(code, kFirstInstruction);
    return result;
  }

  void SetText(const std::string& code) {
    textString = code;
    text.str = textString.c_str();
    text.length = textString.size();
  }

  // Destroys the binary, if it exists.
  void DestroyBinary() {
    spvBinaryDestroy(binary);
    binary = nullptr;
  }

  spv_opcode_table opcodeTable;
  spv_operand_table operandTable;
  spv_ext_inst_table extInstTable;
  spv_diagnostic diagnostic;

  std::string textString;
  spv_text_t text;
  spv_binary binary;
};

using TextToBinaryTest = TextToBinaryTestBase<::testing::Test>;

}  // namespace test_fixture

#endif  // _TEXT_FIXTURE_H_
