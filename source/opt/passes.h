// Copyright (c) 2016 Google Inc.
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

#ifndef LIBSPIRV_OPT_PASSES_H_
#define LIBSPIRV_OPT_PASSES_H_

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

#include "constants.h"
#include "def_use_manager.h"
#include "module.h"
#include "pass.h"
#include "type_manager.h"

namespace spvtools {
namespace opt {

// A null pass that does nothing.
class NullPass : public Pass {
  const char* name() const override { return "null"; }
  bool Process(ir::Module*) override { return false; }
};

// The optimization pass for removing debug instructions (as documented in
// Section 3.32.2 of the SPIR-V spec).
class StripDebugInfoPass : public Pass {
 public:
  const char* name() const override { return "strip-debug"; }
  bool Process(ir::Module* module) override;
};

// The transformation pass that specializes the value of spec constants to
// their default values. This pass only processes the spec constants that have
// Spec ID decorations (defined by OpSpecConstant, OpSpecConstantTrue and
// OpSpecConstantFalse instructions) and replaces them with their front-end
// version counterparts (OpConstant, OpConstantTrue and OpConstantFalse). The
// corresponding Spec ID annotation instructions will also be removed. This
// pass does not fold the newly added front-end constants and does not process
// other spec constants defined by OpSpecConstantComposite or OpSpecConstantOp.
class FreezeSpecConstantValuePass : public Pass {
 public:
  const char* name() const override { return "freeze-spec-const"; }
  bool Process(ir::Module*) override;
};

// The optimization pass to remove dead constants, including front-end
// contants: defined by OpConstant, OpConstantComposite, OpConstantTrue and
// OpConstantFalse; and spec constants: defined by OpSpecConstant,
// OpSpecConstantComposite, OpSpecConstantTrue, OpSpecConstantFalse and
// OpSpecConstantOp.
class EliminateDeadConstantPass : public Pass {
 public:
  const char* name() const override { return "eliminate-dead-const"; }
  bool Process(ir::Module*) override;
};

// The optimization pass that folds "Spec Constants", which are defined by
// OpSpecConstantOp and OpSpecConstantComposite instruction, to "Normal
// Constants", which are defined by OpConstantTrue, OpConstantFalse,
// OpConstant, OpConstantNull and OpConstantComposite instructions. Note that
// Spec Constants defined with OpSpecConstant, OpSpecConstantTrue and
// OpSpecConstantFalse instructions are not handled, as these instructions
// indicate their value are not determined and can be changed in future.  A
// Spec Constant is foldable if all of its value(s) can be determined from the
// module. For example, an Integer Spec Constant defined with OpSpecConstantOp
// instruction can be folded if its value won't change later. This pass will
// replace the original OpSpecContantOp instruction with an OpConstant
// instruction. When folding composite type Spec Constants, new instructions
// may be inserted to define the components of the composite constant first,
// then the original Spec Constants will be replace with OpConstantComposite
// instructions.
// There are some operations not supported:
//  OpSConvert, OpFConvert, OpQuantizeToF16 and all the operations under Kernel
//  capability.
// TODO(qining): Add support for the operations listed above.
class FoldSpecConstantOpAndCompositePass : public Pass {
 public:
  FoldSpecConstantOpAndCompositePass()
      : max_id_(0),
        module_(nullptr),
        def_use_mgr_(nullptr),
        type_mgr_(nullptr),
        id_to_const_val_() {}
  const char* name() const override { return "fold-spec-const-op-composite"; }
  bool Process(ir::Module* module) override {
    Initialize(module);
    return ProcessImpl(module);
  };

 private:
  // Initializes the type manager, def-use manager and get the maximal id used
  // in the module.
  void Initialize(ir::Module* module) {
    type_mgr_.reset(new analysis::TypeManager(*module));
    def_use_mgr_.reset(new analysis::DefUseManager(module));
    for (const auto& id_def : def_use_mgr_->id_to_defs()) {
      max_id_ = std::max(max_id_, id_def.first);
    }
    module_ = module;
  };

  // The real entry of processing. Iterates through the types-constants-globals
  // section of the given module, finds the Spec Constants defined with
  // OpSpecConstantOp and OpSpecConstantComposite instructions. If the result
  // value of those spec constants can be folded, fold them to their
  // corresponding normal constants. Returns true if the module was modified.
  bool ProcessImpl(ir::Module*);

  // Processes the OpSpecConstantOp instruction pointed by the given
  // instruction iterator, folds it to normal constants if possible. Returns
  // true if the spec constant is folded to normal constants. New instructions
  // will be inserted before the OpSpecConstantOp instruction pointed by the
  // instruction iterator. The instruction iterator, which is passed by
  // pointer, will still point to the original OpSpecConstantOp instruction. If
  // folding is done successfully, the original OpSpecConstantOp instruction
  // will be changed to Nop and new folded instruction will be inserted before
  // it.
  bool ProcessOpSpecConstantOp(ir::Module::inst_iterator* pos);

  // Try to fold the OpSpecConstantOp CompositeExtract instruction pointed by
  // the given instruction iterator to a normal constant defining instruction.
  // Returns the pointer to the new constant defining instruction if succeeded.
  // Otherwise returns nullptr.
  ir::Instruction* DoCompositeExtract(ir::Module::inst_iterator* inst_iter_ptr);

  // Try to fold the OpSpecConstantOp VectorShuffle instruction pointed by the
  // given instruction iterator to a normal constant defining instruction.
  // Returns the pointer to the new constant defining instruction if succeeded.
  // Otherwise return nullptr.
  ir::Instruction* DoVectorShuffle(ir::Module::inst_iterator* inst_iter_ptr);

  // Try to fold the OpSpecConstantOp <component wise operations> instruction
  // pointed by the given instruction iterator to a normal constant defining
  // instruction. Returns the pointer to the new constant defining instruction
  // if succeeded, otherwise return nullptr.
  ir::Instruction* DoComponentWiseOperation(
      ir::Module::inst_iterator* inst_iter_ptr);

  // Creates a constant defining instruction for the given Constant instance
  // and inserts the instruction at the position specified by the given
  // instruction iterator. Returns a pointer to the created instruction if
  // succeeded, otherwise returns a null pointer. The instruction iterator
  // points to the same instruction before and after the insertion. This is the
  // only method that actually manages id creation/assignment and instruction
  // creation/insertion for a new Constant instance.
  ir::Instruction* BuildInstructionAndAddToModule(
      std::unique_ptr<analysis::Constant> c, ir::Module::inst_iterator* pos);

  // Creates a Constant instance to hold the constant value of the given
  // instruction. If the given instruction defines a normal constants whose
  // value is already known in the module, returns the unique pointer to the
  // created Constant instance. Otherwise does not create anything and returns a
  // nullptr.
  std::unique_ptr<analysis::Constant> CreateConstFromInst(
      ir::Instruction* inst);

  // Creates a Constant instance with the given type and a vector of constant
  // defining words. Returns an unique pointer to the created Constant instance
  // if the Constant instance can be created successfully. To create scalar
  // type constants, the vector should contain the constant value in 32 bit
  // words and the given type must be of type Bool, Integer or Float. To create
  // composite type constants, the vector should contain the component ids, and
  // those component ids should have been recorded before as Normal Constants.
  // And the given type must be of type Struct, Vector or Array. When creating
  // VectorType Constant instance, the components must be scalars of the same
  // type, either Bool, Integer or Float. If any of the rules above failed, the
  // creation will fail and nullptr will be returned. If the vector is empty,
  // a NullConstant instance will be created with the given type.
  std::unique_ptr<analysis::Constant> CreateConst(
      const analysis::Type* type,
      const std::vector<uint32_t>& literal_words_or_ids);

  // Creates an instruction with the given result id to declare a constant
  // represented by the given Constant instance. Returns an unique pointer to
  // the created instruction if the instruction can be created successfully.
  // Otherwise, returns a null pointer.
  std::unique_ptr<ir::Instruction> CreateInstruction(uint32_t result_id,
                                                     analysis::Constant* c);

  // Creates an OpConstantComposite instruction with the given result id and
  // the CompositeConst instance which represents a composite constant. Returns
  // an unique pointer to the created instruction if succeeded. Otherwise
  // returns a null pointer.
  std::unique_ptr<ir::Instruction> CreateCompositeInstruction(
      uint32_t result_id, analysis::CompositeConstant* cc);

  // A helper function to get the collected normal constant with the given id.
  // Returns the pointer to the Constant instance in case it is found.
  // Otherwise, returns null pointer.
  analysis::Constant* FindRecordedConst(uint32_t id);
  // A helper function to get the id of a collected constant with the pointer
  // to the Constant instance. Returns 0 in case the constant is not found.
  uint32_t FindRecordedConst(const analysis::Constant* c);

  // A helper function to get a vector of Constant instances with the specified
  // ids. If can not find the Constant instance for any one of the ids, returns
  // an empty vector.
  std::vector<const analysis::Constant*> GetConstsFromIds(
      const std::vector<uint32_t>& ids);

  // A helper function to get the result type of the given instrution. Returns
  // nullptr if the instruction does not have a type id (type id is 0).
  analysis::Type* GetType(const ir::Instruction* inst) {
    return type_mgr_->GetType(inst->type_id());
  }

  // The maximum used ID.
  uint32_t max_id_;
  // A pointer to the module under process.
  ir::Module* module_;
  // DefUse manager
  std::unique_ptr<analysis::DefUseManager> def_use_mgr_;
  // Type manager
  std::unique_ptr<analysis::TypeManager> type_mgr_;

  // A mapping from the result ids of Normal Constants to their
  // analysis::Constant instances. All Normal Constants in the module, either
  // existing ones before optimization or the newly generated ones, should have
  // their Constant instance stored and their result id registered in this map.
  std::unordered_map<uint32_t, std::unique_ptr<analysis::Constant>>
      id_to_const_val_;
  // A mapping from the analsis::Constant instance of Normal Contants to their
  // result id in the module. This is a mirror map of id_to_const_val_. All
  // Normal Constants that defining instructions in the module should have
  // their analysis::Constant and their result id registered here.
  std::unordered_map<const analysis::Constant*, uint32_t> const_val_to_id_;
};

}  // namespace opt
}  // namespace spvtools

#endif  // LIBSPIRV_OPT_PASSES_H_
