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

#ifndef LIBSPIRV_OPT_ELIMINATE_DEAD_FUNCTIONS_PASS_H_
#define LIBSPIRV_OPT_ELIMINATE_DEAD_FUNCTIONS_PASS_H_

#include "def_use_manager.h"
#include "function.h"
#include "mem_pass.h"
#include "module.h"

namespace spvtools {
namespace opt {

// See optimizer.hpp for documentation.
class EliminateDeadFunctionsPass : public MemPass {
 public:
  const char* name() const override { return "eliminate-dead-functions"; }
  Status Process(ir::IRContext* c) override;

 private:
  void EliminateFunction(ir::Function* func);
};

}  // namespace opt
}  // namespace spvtools

#endif  // LIBSPIRV_OPT_ELIMINATE_DEAD_FUNCTIONS_PASS_H_
