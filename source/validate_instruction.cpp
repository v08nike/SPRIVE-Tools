// Copyright (c) 2015-2016 The Khronos Group Inc.
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

// Performs validation on instructions that appear inside of a SPIR-V block.

#include "validate_passes.h"

namespace libspirv {

spv_result_t InstructionPass(ValidationState_t& _,
                             const spv_parsed_instruction_t* inst) {
  if (_.is_enabled(SPV_VALIDATE_INSTRUCTION_BIT)) {
    SpvOp opcode = inst->opcode;
    switch (opcode) {
      case SpvOpVariable: {
        const uint32_t storage_class = inst->words[inst->operands[2].offset];
        if (_.getLayoutSection() > kLayoutFunctionDeclarations) {
          if (storage_class != SpvStorageClassFunction) {
            return _.diag(SPV_ERROR_INVALID_LAYOUT)
                   << "Variables must have a function[7] storage class inside"
                      " of a function";
          }
        } else {
          if (storage_class == SpvStorageClassFunction) {
            return _.diag(SPV_ERROR_INVALID_LAYOUT)
                   << "Variables can not have a function[7] storage class "
                      "outside of a function";
          }
        }
      } break;
      default:
        break;
    }
  }
  return SPV_SUCCESS;
}

}
