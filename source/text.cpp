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

#include "text.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "binary.h"
#include "bitwisecast.h"
#include "diagnostic.h"
#include "ext_inst.h"
#include <libspirv/libspirv.h>
#include "opcode.h"
#include "operand.h"

using spvutils::BitwiseCast;

// Structures

using spv_named_id_table = std::unordered_map<std::string, uint32_t>;

// Text API

std::string spvGetWord(const char *str) {
  size_t index = 0;
  while (true) {
    switch (str[index]) {
      case '\0':
      case '\t':
      case '\v':
      case '\r':
      case '\n':
      case ' ':
        return std::string(str, str + index);
      default:
        index++;
    }
  }
  assert(0 && "Unreachable");
  return "";  // Make certain compilers happy.
}

uint32_t spvNamedIdAssignOrGet(spv_named_id_table* table, const char *textValue,
                               uint32_t *pBound) {
  if (table->end() == table->find(textValue)) {
    (*table)[std::string(textValue)] = *pBound;
  }
  return (*table)[textValue];
}

spv_result_t spvTextAdvanceLine(const spv_text text, spv_position position) {
  while (true) {
    switch (text->str[position->index]) {
      case '\0':
        return SPV_END_OF_STREAM;
      case '\n':
        position->column = 0;
        position->line++;
        position->index++;
        return SPV_SUCCESS;
      default:
        position->column++;
        position->index++;
        break;
    }
  }
}

bool spvIsValidIDCharacter(const char value) {
  return value == '_' || 0 != ::isalnum(value);
}

// Returns true if the given string represents a valid ID name.
bool spvIsValidID(const char* textValue) {
  const char* c = textValue;
  for (; *c != '\0'; ++c) {
    if (!spvIsValidIDCharacter(*c)) {
      return false;
    }
  }
  // If the string was empty, then the ID also is not valid.
  return c != textValue;
}

spv_result_t spvTextAdvance(const spv_text text, spv_position position) {
  // NOTE: Consume white space, otherwise don't advance.
  switch (text->str[position->index]) {
    case '\0':
      return SPV_END_OF_STREAM;
    case ';':
      if (spv_result_t error = spvTextAdvanceLine(text, position)) return error;
      return spvTextAdvance(text, position);
    case ' ':
    case '\t':
      position->column++;
      position->index++;
      return spvTextAdvance(text, position);
    case '\n':
      position->column = 0;
      position->line++;
      position->index++;
      return spvTextAdvance(text, position);
    default:
      break;
  }

  return SPV_SUCCESS;
}

spv_result_t spvTextWordGet(const spv_text text,
                            const spv_position startPosition, std::string &word,
                            spv_position endPosition) {
  if (!text->str || !text->length) return SPV_ERROR_INVALID_TEXT;
  if (!startPosition || !endPosition) return SPV_ERROR_INVALID_POINTER;

  *endPosition = *startPosition;

  bool quoting = false;
  bool escaping = false;

  // NOTE: Assumes first character is not white space!
  while (true) {
    const char ch = text->str[endPosition->index];
    if (ch == '\\')
      escaping = !escaping;
    else {
      switch (ch) {
        case '"':
          if (!escaping) quoting = !quoting;
          break;
        case ' ':
        case ';':
        case '\t':
        case '\n':
          if (escaping || quoting) break;
        // Fall through.
        case '\0': {  // NOTE: End of word found!
          word.assign(text->str + startPosition->index,
                      (size_t)(endPosition->index - startPosition->index));
          return SPV_SUCCESS;
        }
        default:
          break;
      }
      escaping = false;
    }

    endPosition->column++;
    endPosition->index++;
  }
}

namespace {

// Returns true if the string at the given position in text starts with "Op".
bool spvStartsWithOp(const spv_text text, const spv_position position) {
  if (text->length < position->index + 3) return false;
  char ch0 = text->str[position->index];
  char ch1 = text->str[position->index + 1];
  char ch2 = text->str[position->index + 2];
  return ('O' == ch0 && 'p' == ch1 && ('A' <= ch2 && ch2 <= 'Z'));
}

}  // anonymous namespace

// Returns true if a new instruction begins at the given position in text.
bool spvTextIsStartOfNewInst(const spv_text text, const spv_position position) {
  spv_position_t nextPosition = *position;
  if (spvTextAdvance(text, &nextPosition)) return false;
  if (spvStartsWithOp(text, &nextPosition)) return true;

  std::string word;
  spv_position_t startPosition = *position;
  if (spvTextWordGet(text, &startPosition, word, &nextPosition)) return false;
  if ('%' != word.front()) return false;

  if (spvTextAdvance(text, &nextPosition)) return false;
  startPosition = nextPosition;
  if (spvTextWordGet(text, &startPosition, word, &nextPosition)) return false;
  if ("=" != word) return false;

  if (spvTextAdvance(text, &nextPosition)) return false;
  startPosition = nextPosition;
  if (spvStartsWithOp(text, &startPosition)) return true;
  return false;
}

spv_result_t spvTextStringGet(const spv_text text,
                              const spv_position startPosition,
                              std::string &string, spv_position endPosition) {
  if (!text->str || !text->length) return SPV_ERROR_INVALID_TEXT;
  if (!startPosition || !endPosition) return SPV_ERROR_INVALID_POINTER;

  if ('"' != text->str[startPosition->index]) return SPV_ERROR_INVALID_TEXT;

  *endPosition = *startPosition;

  // NOTE: Assumes first character is not white space
  while (true) {
    endPosition->column++;
    endPosition->index++;

    switch (text->str[endPosition->index]) {
      case '"': {
        endPosition->column++;
        endPosition->index++;

        string.assign(text->str + startPosition->index,
                      (size_t)(endPosition->index - startPosition->index));

        return SPV_SUCCESS;
      }
      case '\n':
      case '\0':
        return SPV_ERROR_INVALID_TEXT;
      default:
        break;
    }
  }
}

spv_result_t spvTextToUInt32(const char *textValue, uint32_t *pValue) {
  char *endPtr = nullptr;
  *pValue = strtoul(textValue, &endPtr, 0);
  if (0 == *pValue && textValue == endPtr) {
    return SPV_ERROR_INVALID_TEXT;
  }
  return SPV_SUCCESS;
}

spv_result_t spvTextToLiteral(const char *textValue, spv_literal_t *pLiteral) {
  bool isSigned = false;
  int numPeriods = 0;
  bool isString = false;

  const size_t len = strlen(textValue);
  if (len == 0) return SPV_FAILED_MATCH;

  for (uint64_t index = 0; index < len; ++index) {
    switch (textValue[index]) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        break;
      case '.':
        numPeriods++;
        break;
      case '-':
        if (index == 0) {
          isSigned = true;
        } else {
          isString = true;
        }
        break;
      default:
        isString = true;
        index = len;  // break out of the loop too.
        break;
    }
  }

  pLiteral->type = spv_literal_type_t(99);

  if (isString || numPeriods > 1 || (isSigned && len == 1)) {
    // TODO(dneto): Allow escaping.
    if (len < 2 || textValue[0] != '"' || textValue[len - 1] != '"')
      return SPV_FAILED_MATCH;
    pLiteral->type = SPV_LITERAL_TYPE_STRING;
    // Need room for the null-terminator.
    if (len >= sizeof(pLiteral->value.str)) return SPV_ERROR_OUT_OF_MEMORY;
    strncpy(pLiteral->value.str, textValue + 1, len - 2);
    pLiteral->value.str[len - 2] = 0;
  } else if (numPeriods == 1) {
    double d = std::strtod(textValue, nullptr);
    float f = (float)d;
    if (d == (double)f) {
      pLiteral->type = SPV_LITERAL_TYPE_FLOAT_32;
      pLiteral->value.f = f;
    } else {
      pLiteral->type = SPV_LITERAL_TYPE_FLOAT_64;
      pLiteral->value.d = d;
    }
  } else if (isSigned) {
    int64_t i64 = strtoll(textValue, nullptr, 10);
    int32_t i32 = (int32_t)i64;
    if (i64 == (int64_t)i32) {
      pLiteral->type = SPV_LITERAL_TYPE_INT_32;
      pLiteral->value.i32 = i32;
    } else {
      pLiteral->type = SPV_LITERAL_TYPE_INT_64;
      pLiteral->value.i64 = i64;
    }
  } else {
    uint64_t u64 = strtoull(textValue, nullptr, 10);
    uint32_t u32 = (uint32_t)u64;
    if (u64 == (uint64_t)u32) {
      pLiteral->type = SPV_LITERAL_TYPE_UINT_32;
      pLiteral->value.u32 = u32;
    } else {
      pLiteral->type = SPV_LITERAL_TYPE_UINT_64;
      pLiteral->value.u64 = u64;
    }
  }

  return SPV_SUCCESS;
}

spv_result_t spvTextParseMaskOperand(const spv_operand_table operandTable,
                                     const spv_operand_type_t type,
                                     const char *textValue, uint32_t *pValue) {
  if (textValue == nullptr) return SPV_ERROR_INVALID_TEXT;
  size_t text_length = strlen(textValue);
  if (text_length == 0) return SPV_ERROR_INVALID_TEXT;
  const char *text_end = textValue + text_length;

  // We only support mask expressions in ASCII, so the separator value is a
  // char.
  const char separator = '|';

  // Accumulate the result by interpreting one word at a time, scanning
  // from left to right.
  uint32_t value = 0;
  const char *begin = textValue;  // The left end of the current word.
  const char *end = nullptr;  // One character past the end of the current word.
  do {
    end = std::find(begin, text_end, separator);

    spv_operand_desc entry = nullptr;
    if (spvOperandTableNameLookup(operandTable, type, begin, end - begin,
                                  &entry)) {
      return SPV_ERROR_INVALID_TEXT;
    }
    value |= entry->value;

    // Advance to the next word by skipping over the separator.
    begin = end + 1;
  } while (end != text_end);

  *pValue = value;
  return SPV_SUCCESS;
}



/// @brief Translate an Opcode operand to binary form
///
/// @param[in] type of the operand
/// @param[in] textValue word of text to be parsed
/// @param[in] operandTable operand lookup table
/// @param[in,out] namedIdTable table of named ID's
/// @param[out] pInst return binary Opcode
/// @param[in,out] pExpectedOperands the operand types expected
/// @param[in,out] pBound current highest defined ID value
/// @param[in] pPosition used in diagnostic on error
/// @param[out] pDiagnostic populated on error
///
/// @return result code
spv_result_t spvTextEncodeOperand(
    const spv_operand_type_t type, const char *textValue,
    const spv_operand_table operandTable, const spv_ext_inst_table extInstTable,
    spv_named_id_table* namedIdTable, spv_instruction_t *pInst,
    spv_operand_pattern_t *pExpectedOperands, uint32_t *pBound,
    const spv_position position, spv_diagnostic *pDiagnostic) {
  // NOTE: Handle immediate int in the stream
  if ('!' == textValue[0]) {
    const char *begin = textValue + 1;
    char *end = nullptr;
    uint32_t immediateInt = strtoul(begin, &end, 0);
    size_t size = strlen(textValue);
    size_t length = (end - begin);
    if (size - 1 != length) {
      DIAGNOSTIC << "Invalid immediate integer '" << textValue << "'.";
      return SPV_ERROR_INVALID_TEXT;
    }
    position->column += size;
    position->index += size;
    pInst->words[pInst->wordCount] = immediateInt;
    pInst->wordCount += 1;
    return SPV_SUCCESS;
  }

  switch (type) {
    case SPV_OPERAND_TYPE_EXECUTION_SCOPE:
    case SPV_OPERAND_TYPE_ID:
    case SPV_OPERAND_TYPE_ID_IN_OPTIONAL_TUPLE:
    case SPV_OPERAND_TYPE_OPTIONAL_ID:
    case SPV_OPERAND_TYPE_MEMORY_SEMANTICS:
    case SPV_OPERAND_TYPE_RESULT_ID: {
      if ('%' == textValue[0]) {
        textValue++;
      } else {
        DIAGNOSTIC << "Expected id to start with %.";
        return SPV_ERROR_INVALID_TEXT;
      }
      if (!spvIsValidID(textValue)) {
        DIAGNOSTIC << "Invalid ID " << textValue;
        return SPV_ERROR_INVALID_TEXT;
      }
      const uint32_t id =
          spvNamedIdAssignOrGet(namedIdTable, textValue, pBound);
      pInst->words[pInst->wordCount++] = id;
      *pBound = std::max(*pBound, id + 1);
    } break;
    case SPV_OPERAND_TYPE_LITERAL_NUMBER: {
      // NOTE: Special case for extension instruction lookup
      if (OpExtInst == pInst->opcode) {
        spv_ext_inst_desc extInst;
        if (spvExtInstTableNameLookup(extInstTable, pInst->extInstType,
                                      textValue, &extInst)) {
          DIAGNOSTIC << "Invalid extended instruction name '" << textValue
                     << "'.";
          return SPV_ERROR_INVALID_TEXT;
        }
        pInst->words[pInst->wordCount++] = extInst->ext_inst;

        // Prepare to parse the operands for the extended instructions.
        spvPrependOperandTypes(extInst->operandTypes, pExpectedOperands);

        return SPV_SUCCESS;
      }
    }  // Fall through for the general case.
    case SPV_OPERAND_TYPE_MULTIWORD_LITERAL_NUMBER:
    case SPV_OPERAND_TYPE_LITERAL_NUMBER_IN_OPTIONAL_TUPLE:
    case SPV_OPERAND_TYPE_OPTIONAL_LITERAL_NUMBER: {
      spv_literal_t literal = {};
      spv_result_t error = spvTextToLiteral(textValue, &literal);
      if (error != SPV_SUCCESS) {
        if (error == SPV_ERROR_OUT_OF_MEMORY) return error;
        if (spvOperandIsOptional(type)) return SPV_FAILED_MATCH;
        DIAGNOSTIC << "Invalid literal number '" << textValue << "'.";
        return SPV_ERROR_INVALID_TEXT;
      }
      switch (literal.type) {
        // We do not have to print diagnostics here because spvBinaryEncode*
        // prints diagnostic messages on failure.
        case SPV_LITERAL_TYPE_INT_32:
          if (spvBinaryEncodeU32(BitwiseCast<uint32_t>(literal.value.i32),
                                 pInst, position, pDiagnostic))
            return SPV_ERROR_INVALID_TEXT;
          break;
        case SPV_LITERAL_TYPE_INT_64: {
          if (spvBinaryEncodeU64(BitwiseCast<uint64_t>(literal.value.i64),
                                 pInst, position, pDiagnostic))
            return SPV_ERROR_INVALID_TEXT;
        } break;
        case SPV_LITERAL_TYPE_UINT_32: {
          if (spvBinaryEncodeU32(literal.value.u32, pInst, position,
                                 pDiagnostic))
            return SPV_ERROR_INVALID_TEXT;
        } break;
        case SPV_LITERAL_TYPE_UINT_64: {
          if (spvBinaryEncodeU64(BitwiseCast<uint64_t>(literal.value.u64),
                                 pInst, position, pDiagnostic))
            return SPV_ERROR_INVALID_TEXT;
        } break;
        case SPV_LITERAL_TYPE_FLOAT_32: {
          if (spvBinaryEncodeU32(BitwiseCast<uint32_t>(literal.value.f), pInst,
                                 position, pDiagnostic))
            return SPV_ERROR_INVALID_TEXT;
        } break;
        case SPV_LITERAL_TYPE_FLOAT_64: {
          if (spvBinaryEncodeU64(BitwiseCast<uint64_t>(literal.value.d), pInst,
                                 position, pDiagnostic))
            return SPV_ERROR_INVALID_TEXT;
        } break;
        case SPV_LITERAL_TYPE_STRING: {
          DIAGNOSTIC << "Expected literal number, found literal string '"
                     << textValue << "'.";
          return SPV_FAILED_MATCH;
        } break;
        default:
          DIAGNOSTIC << "Invalid literal number '" << textValue << "'";
          return SPV_ERROR_INVALID_TEXT;
      }
    } break;
    case SPV_OPERAND_TYPE_LITERAL_STRING:
    case SPV_OPERAND_TYPE_OPTIONAL_LITERAL_STRING: {
      spv_literal_t literal = {};
      spv_result_t error = spvTextToLiteral(textValue, &literal);
      if (error != SPV_SUCCESS) {
        if (error == SPV_ERROR_OUT_OF_MEMORY) return error;
        if (spvOperandIsOptional(type)) return SPV_FAILED_MATCH;
        DIAGNOSTIC << "Invalid literal string '" << textValue << "'.";
        return SPV_ERROR_INVALID_TEXT;
      }
      if (literal.type != SPV_LITERAL_TYPE_STRING) {
        DIAGNOSTIC << "Expected literal string, found literal number '"
                   << textValue << "'.";
        return SPV_FAILED_MATCH;
      }

      // NOTE: Special case for extended instruction library import
      if (OpExtInstImport == pInst->opcode) {
        pInst->extInstType = spvExtInstImportTypeGet(literal.value.str);
      }

      if (spvBinaryEncodeString(literal.value.str, pInst, position,
                                pDiagnostic))
        return SPV_ERROR_INVALID_TEXT;
    } break;
    case SPV_OPERAND_TYPE_FP_FAST_MATH_MODE:
    case SPV_OPERAND_TYPE_FUNCTION_CONTROL:
    case SPV_OPERAND_TYPE_LOOP_CONTROL:
    case SPV_OPERAND_TYPE_OPTIONAL_IMAGE:
    case SPV_OPERAND_TYPE_OPTIONAL_MEMORY_ACCESS:
    case SPV_OPERAND_TYPE_SELECTION_CONTROL: {
      uint32_t value;
      if (spvTextParseMaskOperand(operandTable, type, textValue, &value)) {
        DIAGNOSTIC << "Invalid " << spvOperandTypeStr(type) << " '" << textValue
                   << "'.";
        return SPV_ERROR_INVALID_TEXT;
      }
      if (auto error = spvBinaryEncodeU32(value, pInst, position, pDiagnostic))
        return error;
      // Prepare to parse the operands for this logical operand.
      spvPrependOperandTypesForMask(operandTable, type, value, pExpectedOperands);
    } break;
    default: {
      // NOTE: All non literal operands are handled here using the operand
      // table.
      spv_operand_desc entry;
      if (spvOperandTableNameLookup(operandTable, type, textValue,
                                    strlen(textValue), &entry)) {
        DIAGNOSTIC << "Invalid " << spvOperandTypeStr(type) << " '" << textValue
                   << "'.";
        return SPV_ERROR_INVALID_TEXT;
      }
      if (spvBinaryEncodeU32(entry->value, pInst, position, pDiagnostic)) {
        DIAGNOSTIC << "Invalid " << spvOperandTypeStr(type) << " '" << textValue
                   << "'.";
        return SPV_ERROR_INVALID_TEXT;
      }

      // Prepare to parse the operands for this logical operand.
      spvPrependOperandTypes(entry->operandTypes, pExpectedOperands);
    } break;
  }
  return SPV_SUCCESS;
}

namespace {

/// Encodes an instruction started by !<integer> at the given position in text.
///
/// Puts the encoded words into *pInst.  If successful, moves position past the
/// instruction and returns SPV_SUCCESS.  Otherwise, returns an error code and
/// leaves position pointing to the error in text.
spv_result_t encodeInstructionStartingWithImmediate(
    const spv_text text, const spv_operand_table operandTable,
    const spv_ext_inst_table extInstTable, spv_named_id_table* namedIdTable,
    uint32_t *pBound, spv_instruction_t *pInst, spv_position position,
    spv_diagnostic *pDiagnostic) {
  std::string firstWord;
  spv_position_t nextPosition = {};
  auto error = spvTextWordGet(text, position, firstWord, &nextPosition);
  if (error) {
    DIAGNOSTIC << "Internal Error";
    return error;
  }

  assert(firstWord[0] == '!');
  const char *begin = firstWord.data() + 1;
  char *end = nullptr;
  uint32_t immediateInt = strtoul(begin, &end, 0);
  if ((begin + firstWord.size() - 1) != end) {
    DIAGNOSTIC << "Invalid immediate integer '" << firstWord << "'.";
    return SPV_ERROR_INVALID_TEXT;
  }
  position->column += firstWord.size();
  position->index += firstWord.size();
  pInst->words[0] = immediateInt;
  pInst->wordCount = 1;
  while (spvTextAdvance(text, position) != SPV_END_OF_STREAM) {
    // A beginning of a new instruction means we're done.
    if (spvTextIsStartOfNewInst(text, position)) return SPV_SUCCESS;

    // Otherwise, there must be an operand that's either a literal, an ID, or
    // an immediate.
    std::string operandValue;
    if ((error = spvTextWordGet(text, position, operandValue, &nextPosition))) {
      DIAGNOSTIC << "Internal Error";
      return error;
    }

    if (operandValue == "=") {
      DIAGNOSTIC << firstWord << " not allowed before =.";
      return SPV_ERROR_INVALID_TEXT;
    }

    // Needed to pass to spvTextEncodeOpcode(), but it shouldn't ever be
    // expanded.
    spv_operand_pattern_t dummyExpectedOperands;
    error = spvTextEncodeOperand(
        SPV_OPERAND_TYPE_OPTIONAL_LITERAL_NUMBER, operandValue.c_str(),
        operandTable, extInstTable, namedIdTable, pInst, &dummyExpectedOperands,
        pBound, position, pDiagnostic);
    if (error == SPV_FAILED_MATCH) {
      // It's not a literal number -- is it a literal string?
      error = spvTextEncodeOperand(
          SPV_OPERAND_TYPE_OPTIONAL_LITERAL_STRING, operandValue.c_str(),
          operandTable, extInstTable, namedIdTable, pInst,
          &dummyExpectedOperands, pBound, position, pDiagnostic);
    }
    if (error == SPV_FAILED_MATCH) {
      // It's not a literal -- is it an ID?
      error = spvTextEncodeOperand(
          SPV_OPERAND_TYPE_OPTIONAL_ID, operandValue.c_str(), operandTable,
          extInstTable, namedIdTable, pInst, &dummyExpectedOperands, pBound,
          position, pDiagnostic);
      if (error) {
        DIAGNOSTIC << "Invalid word following " << firstWord << ": "
                   << operandValue;
      }
    }
    if (error) return error;
    *position = nextPosition;
  }
  return SPV_SUCCESS;
}

}  // anonymous namespace

/// @brief Translate single Opcode and operands to binary form
///
/// @param[in] text stream to translate
/// @param[in] format the assembly syntax format of text
/// @param[in] opcodeTable Opcode lookup table
/// @param[in] operandTable operand lookup table
/// @param[in,out] namedIdTable table of named ID's
/// @param[in,out] pBound current highest defined ID value
/// @param[out] pInst returned binary Opcode
/// @param[in,out] pPosition in the text stream
/// @param[out] pDiagnostic populated on failure
///
/// @return result code
spv_result_t spvTextEncodeOpcode(
    const spv_text text, spv_assembly_syntax_format_t format,
    const spv_opcode_table opcodeTable, const spv_operand_table operandTable,
    const spv_ext_inst_table extInstTable, spv_named_id_table* namedIdTable,
    uint32_t *pBound, spv_instruction_t *pInst, spv_position position,
    spv_diagnostic *pDiagnostic) {

  // Check for !<integer> first.
  if ('!' == text->str[position->index]) {
    return encodeInstructionStartingWithImmediate(
        text, operandTable, extInstTable, namedIdTable, pBound, pInst, position,
        pDiagnostic);
  }

  // An assembly instruction has two possible formats:
  // 1(CAF): <opcode> <operand>..., e.g., "OpTypeVoid %void".
  // 2(AAF): <result-id> = <opcode> <operand>..., e.g., "%void = OpTypeVoid".

  std::string firstWord;
  spv_position_t nextPosition = {};
  spv_result_t error = spvTextWordGet(text, position, firstWord, &nextPosition);
  if (error) {
    DIAGNOSTIC << "Internal Error";
    return error;
  }

  std::string opcodeName;
  std::string result_id;
  spv_position_t result_id_position = {};
  if (spvStartsWithOp(text, position)) {
    opcodeName = firstWord;
  } else {
    // If the first word of this instruction is not an opcode, we must be
    // processing AAF now.
    if (SPV_ASSEMBLY_SYNTAX_FORMAT_ASSIGNMENT != format) {
      DIAGNOSTIC
          << "Expected <opcode> at the beginning of an instruction, found '"
          << firstWord << "'.";
      return SPV_ERROR_INVALID_TEXT;
    }

    result_id = firstWord;
    if ('%' != result_id.front()) {
      DIAGNOSTIC << "Expected <opcode> or <result-id> at the beginning "
                    "of an instruction, found '"
                 << result_id << "'.";
      return SPV_ERROR_INVALID_TEXT;
    }
    result_id_position = *position;

    // The '=' sign.
    *position = nextPosition;
    if (spvTextAdvance(text, position)) {
      DIAGNOSTIC << "Expected '=', found end of stream.";
      return SPV_ERROR_INVALID_TEXT;
    }
    std::string equal_sign;
    error = spvTextWordGet(text, position, equal_sign, &nextPosition);
    if ("=" != equal_sign) {
      DIAGNOSTIC << "'=' expected after result id.";
      return SPV_ERROR_INVALID_TEXT;
    }

    // The <opcode> after the '=' sign.
    *position = nextPosition;
    if (spvTextAdvance(text, position)) {
      DIAGNOSTIC << "Expected opcode, found end of stream.";
      return SPV_ERROR_INVALID_TEXT;
    }
    error = spvTextWordGet(text, position, opcodeName, &nextPosition);
    if (error) {
      DIAGNOSTIC << "Internal Error";
      return error;
    }
    if (!spvStartsWithOp(text, position)) {
      DIAGNOSTIC << "Invalid Opcode prefix '" << opcodeName << "'.";
      return SPV_ERROR_INVALID_TEXT;
    }
  }

  // NOTE: The table contains Opcode names without the "Op" prefix.
  const char *pInstName = opcodeName.data() + 2;

  spv_opcode_desc opcodeEntry;
  error = spvOpcodeTableNameLookup(opcodeTable, pInstName, &opcodeEntry);
  if (error) {
    DIAGNOSTIC << "Invalid Opcode name '"
               << spvGetWord(text->str + position->index) << "'";
    return error;
  }
  if (SPV_ASSEMBLY_SYNTAX_FORMAT_ASSIGNMENT == format) {
    // If this instruction has <result-id>, check it follows AAF.
    if (opcodeEntry->hasResult && result_id.empty()) {
      DIAGNOSTIC << "Expected <result-id> at the beginning of an "
                    "instruction, found '"
                 << firstWord << "'.";
      return SPV_ERROR_INVALID_TEXT;
    }
  }
  pInst->opcode = opcodeEntry->opcode;
  *position = nextPosition;
  pInst->wordCount++;

  // Maintains the ordered list of expected operand types.
  // For many instructions we only need the {numTypes, operandTypes}
  // entries in opcodeEntry.  However, sometimes we need to modify
  // the list as we parse the operands. This occurs when an operand
  // has its own logical operands (such as the LocalSize operand for
  // ExecutionMode), or for extended instructions that may have their
  // own operands depending on the selected extended instruction.
  spv_operand_pattern_t expectedOperands(
      opcodeEntry->operandTypes,
      opcodeEntry->operandTypes + opcodeEntry->numTypes);

  while (!expectedOperands.empty()) {
    const spv_operand_type_t type = expectedOperands.front();
    expectedOperands.pop_front();

    // Expand optional tuples lazily.
    if (spvExpandOperandSequenceOnce(type, &expectedOperands)) continue;

    if (type == SPV_OPERAND_TYPE_RESULT_ID && !result_id.empty()) {
      // Handle the <result-id> for value generating instructions.
      // We've already consumed it from the text stream.  Here
      // we inject its words into the instruction.
      error = spvTextEncodeOperand(SPV_OPERAND_TYPE_RESULT_ID,
                                   result_id.c_str(), operandTable,
                                   extInstTable, namedIdTable, pInst, nullptr,
                                   pBound, &result_id_position, pDiagnostic);
      if (error) return error;
    } else {
      // Find the next word.
      error = spvTextAdvance(text, position);
      if (error == SPV_END_OF_STREAM) {
        if (spvOperandIsOptional(type)) {
          // This would have been the last potential operand for the
          // instruction,
          // and we didn't find one.  We're finished parsing this instruction.
          break;
        } else {
          DIAGNOSTIC << "Expected operand, found end of stream.";
          return SPV_ERROR_INVALID_TEXT;
        }
      }
      assert(error == SPV_SUCCESS && "Somebody added another way to fail");

      if (spvTextIsStartOfNewInst(text, position)) {
        if (spvOperandIsOptional(type)) {
          break;
        } else {
          DIAGNOSTIC << "Expected operand, found next instruction instead.";
          return SPV_ERROR_INVALID_TEXT;
        }
      }

      std::string operandValue;
      error = spvTextWordGet(text, position, operandValue, &nextPosition);
      if (error) {
        DIAGNOSTIC << "Internal Error";
        return error;
      }

      error = spvTextEncodeOperand(
          type, operandValue.c_str(), operandTable, extInstTable, namedIdTable,
          pInst, &expectedOperands, pBound, position, pDiagnostic);

      if (error == SPV_FAILED_MATCH && spvOperandIsOptional(type))
        return SPV_SUCCESS;

      if (error) return error;

      *position = nextPosition;
    }
  }

  pInst->words[0] = spvOpcodeMake(pInst->wordCount, opcodeEntry->opcode);

  return SPV_SUCCESS;
}

namespace {

// Translates a given assembly language module into binary form.
// If a diagnostic is generated, it is not yet marked as being
// for a text-based input.
spv_result_t spvTextToBinaryInternal(const spv_text text,
                                     spv_assembly_syntax_format_t format,
                                     const spv_opcode_table opcodeTable,
                                     const spv_operand_table operandTable,
                                     const spv_ext_inst_table extInstTable,
                                     spv_binary *pBinary,
                                     spv_diagnostic *pDiagnostic) {
  spv_position_t position = {};
  if (!text->str || !text->length) {
    DIAGNOSTIC << "Text stream is empty.";
    return SPV_ERROR_INVALID_TEXT;
  }
  if (!opcodeTable || !operandTable || !extInstTable)
    return SPV_ERROR_INVALID_TABLE;
  if (!pBinary) return SPV_ERROR_INVALID_POINTER;
  if (!pDiagnostic) return SPV_ERROR_INVALID_DIAGNOSTIC;

  // NOTE: Ensure diagnostic is zero initialised
  *pDiagnostic = {};

  uint32_t bound = 1;

  std::vector<spv_instruction_t> instructions;

  if (spvTextAdvance(text, &position)) {
    DIAGNOSTIC << "Text stream is empty.";
    return SPV_ERROR_INVALID_TEXT;
  }

  // This causes namedIdTable to get cleaned up as soon as it is no
  // longer necessary.
  {
    spv_named_id_table namedIdTable;
    spv_ext_inst_type_t extInstType = SPV_EXT_INST_TYPE_NONE;
    while (text->length > position.index) {
      spv_instruction_t inst = {};
      inst.extInstType = extInstType;

      if (spvTextEncodeOpcode(text, format, opcodeTable, operandTable,
                              extInstTable, &namedIdTable, &bound, &inst,
                              &position, pDiagnostic)) {
        return SPV_ERROR_INVALID_TEXT;
      }
      extInstType = inst.extInstType;

      instructions.push_back(inst);

      if (spvTextAdvance(text, &position)) break;
    }
  }

  size_t totalSize = SPV_INDEX_INSTRUCTION;
  for (auto &inst : instructions) {
    totalSize += inst.wordCount;
  }

  uint32_t *data = new uint32_t[totalSize];
  if (!data) return SPV_ERROR_OUT_OF_MEMORY;
  uint64_t currentIndex = SPV_INDEX_INSTRUCTION;
  for (auto &inst : instructions) {
    memcpy(data + currentIndex, inst.words, sizeof(uint32_t) * inst.wordCount);
    currentIndex += inst.wordCount;
  }

  spv_binary binary = new spv_binary_t();
  if (!binary) {
    delete[] data;
    return SPV_ERROR_OUT_OF_MEMORY;
  }
  binary->code = data;
  binary->wordCount = totalSize;

  spv_result_t error = spvBinaryHeaderSet(binary, bound);
  if (error) {
    spvBinaryDestroy(binary);
    return error;
  }

  *pBinary = binary;

  return SPV_SUCCESS;
}

}  // anonymous namespace

spv_result_t spvTextToBinary(const char *input_text,
                             const uint64_t input_text_size,
                             const spv_opcode_table opcodeTable,
                             const spv_operand_table operandTable,
                             const spv_ext_inst_table extInstTable,
                             spv_binary *pBinary, spv_diagnostic *pDiagnostic) {
  return spvTextWithFormatToBinary(
      input_text, input_text_size, SPV_ASSEMBLY_SYNTAX_FORMAT_DEFAULT,
      opcodeTable, operandTable, extInstTable, pBinary, pDiagnostic);
}

spv_result_t spvTextWithFormatToBinary(
    const char *input_text, const uint64_t input_text_size,
    spv_assembly_syntax_format_t format, const spv_opcode_table opcodeTable,
    const spv_operand_table operandTable, const spv_ext_inst_table extInstTable,
    spv_binary *pBinary, spv_diagnostic *pDiagnostic) {
  spv_text_t text = {input_text, input_text_size};

  spv_result_t result =
      spvTextToBinaryInternal(&text, format, opcodeTable, operandTable,
                              extInstTable, pBinary, pDiagnostic);
  if (pDiagnostic && *pDiagnostic) (*pDiagnostic)->isTextSource = true;

  return result;
}

void spvTextDestroy(spv_text text) {
  if (!text) return;
  if (text->str) {
    delete[] text->str;
  }
  delete text;
}
