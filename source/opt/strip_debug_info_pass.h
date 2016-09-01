// Copyright (c) 2016 Google Inc.
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

#ifndef LIBSPIRV_OPT_STRIP_DEBUG_INFO_PASS_H_
#define LIBSPIRV_OPT_STRIP_DEBUG_INFO_PASS_H_

#include "module.h"
#include "pass.h"

namespace spvtools {
namespace opt {

// The optimization pass for removing debug instructions (as documented in
// Section 3.32.2 of the SPIR-V spec).
class StripDebugInfoPass : public Pass {
 public:
  const char* name() const override { return "strip-debug"; }
  bool Process(ir::Module* module) override;
};

}  // namespace opt
}  // namespace spvtools

#endif  // LIBSPIRV_OPT_STRIP_DEBUG_INFO_PASS_H_
