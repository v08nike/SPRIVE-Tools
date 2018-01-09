// Copyright (c) 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIBSPIRV_UTIL_FOLD_H_
#define LIBSPIRV_UTIL_FOLD_H_

#include "constants.h"
#include "def_use_manager.h"

#include <cstdint>
#include <vector>

namespace spvtools {
namespace opt {

// Returns the result of folding a scalar instruction with the given |opcode|
// and |operands|. Each entry in |operands| is a pointer to an
// analysis::Constant instance, which should've been created with the constant
// manager (See IRContext::get_constant_mgr).
//
// It is an error to call this function with an opcode that does not pass the
// IsFoldableOpcode test. If any error occurs during folding, the folder will
// faill with a call to assert.
uint32_t FoldScalars(SpvOp opcode,
                     const std::vector<const analysis::Constant*>& operands);

// Returns the result of performing an operation with the given |opcode| over
// constant vectors with |num_dims| dimensions.  Each entry in |operands| is a
// pointer to an analysis::Constant instance, which should've been created with
// the constant manager (See IRContext::get_constant_mgr).
//
// This function iterates through the given vector type constant operands and
// calculates the result for each element of the result vector to return.
// Vectors with longer than 32-bit scalar components are not accepted in this
// function.
//
// It is an error to call this function with an opcode that does not pass the
// IsFoldableOpcode test. If any error occurs during folding, the folder will
// faill with a call to assert.
std::vector<uint32_t> FoldVectors(
    SpvOp opcode, uint32_t num_dims,
    const std::vector<const analysis::Constant*>& operands);

// Returns true if |opcode| represents an operation handled by FoldScalars or
// FoldVectors.
bool IsFoldableOpcode(SpvOp opcode);

// Returns true if |cst| is supported by FoldScalars and FoldVectors.
bool IsFoldableConstant(const analysis::Constant* cst);

// Returns true if |FoldInstructionToConstant| could fold an instruction whose
// result type is |type_inst|.
bool IsFoldableType(ir::Instruction* type_inst);

// Tries to fold |inst| to a single constant, when the input ids to |inst| have
// been substituted using |id_map|.  Returns a pointer to the OpConstant*
// instruction if successful.  If necessary, a new constant instruction is
// created and placed in the global values section.
//
// |id_map| is a function that takes one result id and returns another.  It can
// be used for things like CCP where it is known that some ids contain a
// constant, but the instruction itself has not been updated yet.  This can map
// those ids to the appropriate constants.
ir::Instruction* FoldInstructionToConstant(
    ir::Instruction* inst, std::function<uint32_t(uint32_t)> id_map);

// Tries to fold |inst| to a simpler instruction that computes the same value,
// when the input ids to |inst| have been substituted using |id_map|.  Returns a
// pointer to the simplified instruction if successful.  If necessary, a new
// instruction is created and placed in the global values section, for
// constants, or after |inst| for other instructions.
//
// |inst| must be an instruction that exists in the body of a function.
//
// |id_map| is a function that takes one result id and returns another.  It can
// be used for things like CCP where it is known that some ids contain a
// constant, but the instruction itself has not been updated yet.  This can map
// those ids to the appropriate constants.
ir::Instruction* FoldInstruction(ir::Instruction* inst,
                                 std::function<uint32_t(uint32_t)> id_map);

// The same as above when |id_map| is the identity function.
inline ir::Instruction* FoldInstruction(ir::Instruction* inst) {
  auto identity_map = [](uint32_t id) { return id; };
  return FoldInstructionToConstant(inst, identity_map);
}

}  // namespace opt
}  // namespace spvtools

#endif  // LIBSPIRV_UTIL_FOLD_H_
