// Copyright (c) 2018 Google LLC
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

#include "simplification_pass.h"

#include <set>
#include <unordered_set>
#include <vector>

#include "fold.h"

namespace spvtools {
namespace opt {

Pass::Status SimplificationPass::Process(opt::IRContext* c) {
  InitializeProcessing(c);
  bool modified = false;

  for (opt::Function& function : *get_module()) {
    modified |= SimplifyFunction(&function);
  }
  return (modified ? Status::SuccessWithChange : Status::SuccessWithoutChange);
}

bool SimplificationPass::SimplifyFunction(opt::Function* function) {
  bool modified = false;
  // Phase 1: Traverse all instructions in dominance order.
  // The second phase will only be on the instructions whose inputs have changed
  // after being processed during phase 1.  Since OpPhi instructions are the
  // only instructions whose inputs do not necessarily dominate the use, we keep
  // track of the OpPhi instructions already seen, and add them to the work list
  // for phase 2 when needed.
  std::vector<opt::Instruction*> work_list;
  std::unordered_set<opt::Instruction*> process_phis;
  std::unordered_set<opt::Instruction*> inst_to_kill;
  std::unordered_set<opt::Instruction*> in_work_list;
  const opt::InstructionFolder& folder = context()->get_instruction_folder();

  cfg()->ForEachBlockInReversePostOrder(
      function->entry().get(),
      [&modified, &process_phis, &work_list, &in_work_list, &inst_to_kill,
       folder, this](opt::BasicBlock* bb) {
        for (opt::Instruction* inst = &*bb->begin(); inst;
             inst = inst->NextNode()) {
          if (inst->opcode() == SpvOpPhi) {
            process_phis.insert(inst);
          }

          if (inst->opcode() == SpvOpCopyObject ||
              folder.FoldInstruction(inst)) {
            modified = true;
            context()->AnalyzeUses(inst);
            get_def_use_mgr()->ForEachUser(inst, [&work_list, &process_phis,
                                                  &in_work_list](
                                                     opt::Instruction* use) {
              if (process_phis.count(use) && in_work_list.insert(use).second) {
                work_list.push_back(use);
              }
            });
            if (inst->opcode() == SpvOpCopyObject) {
              context()->ReplaceAllUsesWith(inst->result_id(),
                                            inst->GetSingleWordInOperand(0));
              inst_to_kill.insert(inst);
              in_work_list.insert(inst);
            } else if (inst->opcode() == SpvOpNop) {
              inst_to_kill.insert(inst);
              in_work_list.insert(inst);
            }
          }
        }
      });

  // Phase 2: process the instructions in the work list until all of the work is
  //          done.  This time we add all users to the work list because phase 1
  //          has already finished.
  for (size_t i = 0; i < work_list.size(); ++i) {
    opt::Instruction* inst = work_list[i];
    in_work_list.erase(inst);
    if (inst->opcode() == SpvOpCopyObject || folder.FoldInstruction(inst)) {
      modified = true;
      context()->AnalyzeUses(inst);
      get_def_use_mgr()->ForEachUser(
          inst, [&work_list, &in_work_list](opt::Instruction* use) {
            if (!use->IsDecoration() && use->opcode() != SpvOpName &&
                in_work_list.insert(use).second) {
              work_list.push_back(use);
            }
          });

      if (inst->opcode() == SpvOpCopyObject) {
        context()->ReplaceAllUsesWith(inst->result_id(),
                                      inst->GetSingleWordInOperand(0));
        inst_to_kill.insert(inst);
        in_work_list.insert(inst);
      } else if (inst->opcode() == SpvOpNop) {
        inst_to_kill.insert(inst);
        in_work_list.insert(inst);
      }
    }
  }

  // Phase 3: Kill instructions we know are no longer needed.
  for (opt::Instruction* inst : inst_to_kill) {
    context()->KillInst(inst);
  }

  return modified;
}

}  // namespace opt
}  // namespace spvtools
