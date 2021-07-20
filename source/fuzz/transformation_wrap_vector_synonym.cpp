// Copyright (c) 2021 Shiyu Liu
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

#include "source/fuzz/transformation_wrap_vector_synonym.h"

#include "source/fuzz/data_descriptor.h"
#include "source/fuzz/fuzzer_util.h"
#include "source/opt/instruction.h"

namespace spvtools {
namespace fuzz {

TransformationWrapVectorSynonym::TransformationWrapVectorSynonym(
    protobufs::TransformationWrapVectorSynonym message)
    : message_(std::move(message)) {}

TransformationWrapVectorSynonym::TransformationWrapVectorSynonym(
    uint32_t instruction_id, uint32_t vector_operand1, uint32_t vector_operand2,
    uint32_t fresh_id, uint32_t pos) {
  message_.set_instruction_id(instruction_id);
  message_.set_vector_operand1(vector_operand1);
  message_.set_vector_operand2(vector_operand2);
  message_.set_fresh_id(fresh_id);
  message_.set_scalar_position(pos);
}

bool TransformationWrapVectorSynonym::IsApplicable(
    opt::IRContext* ir_context,
    const TransformationContext& transformation_context) const {
  // |fresh_id| must be fresh.
  if (!fuzzerutil::IsFreshId(ir_context, message_.fresh_id())) {
    return false;
  }

  const opt::Instruction* instruction =
      ir_context->get_def_use_mgr()->GetDef(message_.instruction_id());

  // |instruction_id| must refer to an existing instruction.
  if (instruction == nullptr) {
    return false;
  }

  if (!IsInstructionSupported(ir_context, *instruction)) {
    return false;
  }

  assert(!transformation_context.GetFactManager()->IdIsIrrelevant(
             instruction->result_id()) &&
         "Result id of the scalar operation must be relevant.");

  // |vector_operand1| and |vector_operand2| must exist.
  auto vec1 = ir_context->get_def_use_mgr()->GetDef(message_.vector_operand1());
  auto vec2 = ir_context->get_def_use_mgr()->GetDef(message_.vector_operand2());

  if (vec1 == nullptr || vec2 == nullptr) {
    return false;
  }

  // The 2 vectors must be the same valid vector type.
  auto vec1_type_id = vec1->type_id();
  auto vec2_type_id = vec2->type_id();

  if (vec1_type_id != vec2_type_id) {
    return false;
  }

  if (ir_context->get_def_use_mgr()->GetDef(vec1_type_id)->opcode() !=
      SpvOpTypeVector) {
    return false;
  }

  // |scalar_position| needs to be a non-negative integer less than the vector
  // length.
  // OpTypeVector instruction has the component count at index 2.
  if (message_.scalar_position() >= ir_context->get_def_use_mgr()
                                        ->GetDef(vec1_type_id)
                                        ->GetSingleWordInOperand(1)) {
    return false;
  }

  if (!transformation_context.GetFactManager()->IsSynonymous(
          MakeDataDescriptor(message_.vector_operand1(),
                             {message_.scalar_position()}),
          MakeDataDescriptor(instruction->GetSingleWordInOperand(0), {}))) {
    return false;
  }

  if (!transformation_context.GetFactManager()->IsSynonymous(
          MakeDataDescriptor(message_.vector_operand2(),
                             {message_.scalar_position()}),
          MakeDataDescriptor(instruction->GetSingleWordInOperand(1), {}))) {
    return false;
  }

  return true;
}

void TransformationWrapVectorSynonym::Apply(
    opt::IRContext* ir_context,
    TransformationContext* transformation_context) const {
  // Create an instruction descriptor for the original instruction.
  auto instruction =
      ir_context->get_def_use_mgr()->GetDef(message_.instruction_id());
  auto destination_block = ir_context->get_instr_block(instruction);

  //  Populate input operand list with two vectors for vector operation.
  opt::Instruction::OperandList in_operands;
  in_operands.push_back({SPV_OPERAND_TYPE_ID, {message_.vector_operand1()}});
  in_operands.push_back({SPV_OPERAND_TYPE_ID, {message_.vector_operand2()}});

  // Make a new arithmetic instruction: %fresh_id = OpXX %type_id %result_id1
  // %result_id2.
  auto vec_type_id = ir_context->get_def_use_mgr()
                         ->GetDef(message_.vector_operand1())
                         ->type_id();
  auto new_instruction = MakeUnique<opt::Instruction>(
      ir_context, instruction->opcode(), vec_type_id, message_.fresh_id(),
      std::move(in_operands));
  auto new_instruction_ptr = new_instruction.get();
  instruction->InsertBefore(std::move(new_instruction));
  ir_context->get_def_use_mgr()->AnalyzeInstDefUse(new_instruction_ptr);
  ir_context->set_instr_block(new_instruction_ptr, destination_block);

  // Add |fresh_id| to id bound.
  fuzzerutil::UpdateModuleIdBound(ir_context, message_.fresh_id());

  // Add synonyms between |fresh_id| and |instruction_id|.
  transformation_context->GetFactManager()->AddFactDataSynonym(
      MakeDataDescriptor(message_.fresh_id(), {message_.scalar_position()}),
      MakeDataDescriptor(message_.instruction_id(), {}));
}

protobufs::Transformation TransformationWrapVectorSynonym::ToMessage() const {
  protobufs::Transformation result;
  *result.mutable_wrap_vector_synonym() = message_;
  return result;
}

std::unordered_set<uint32_t> TransformationWrapVectorSynonym::GetFreshIds()
    const {
  return std::unordered_set<uint32_t>{message_.fresh_id()};
}

bool TransformationWrapVectorSynonym::IsInstructionSupported(
    opt::IRContext* ir_context, const opt::Instruction& instruction) {
  if (!instruction.result_id() || !instruction.type_id()) {
    return false;
  }
  auto type_instruction =
      ir_context->get_def_use_mgr()->GetDef(instruction.type_id());

  if ((type_instruction->opcode() != SpvOpTypeInt &&
       type_instruction->opcode() != SpvOpTypeFloat)) {
    return false;
  }

  switch (instruction.opcode()) {
    case SpvOpIAdd:
    case SpvOpISub:
    case SpvOpIMul:
    case SpvOpFAdd:
    case SpvOpFSub:
    case SpvOpFMul:
      return true;
    default:
      return false;
  }
}

}  // namespace fuzz
}  // namespace spvtools
