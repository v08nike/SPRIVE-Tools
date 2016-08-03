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

#include "bin_to_dot.h"

#include <cassert>
#include <iostream>
#include <vector>

#include "assembly_grammar.h"

namespace {

const char* kMergeStyle = "style=dashed";
const char* kContinueStyle = "style=dotted";

// A DotConverter can be used to dump the GraphViz "dot" graph for
// a SPIR-V module.
class DotConverter {
 public:
  DotConverter(std::iostream* out) : out_(*out) {}

  // Emits the graph preamble.
  void Begin() const {
    out_ << "digraph {\n";
    // Emit a simple legend
    out_ << "legend_merge_src [shape=plaintext, label=\"\"];\n"
         << "legend_merge_dest [shape=plaintext, label=\"\"];\n"
         << "legend_merge_src -> legend_merge_dest [label=\" merge\","
         << kMergeStyle << "];\n"
         << "legend_continue_src [shape=plaintext, label=\"\"];\n"
         << "legend_continue_dest [shape=plaintext, label=\"\"];\n"
         << "legend_continue_src -> legend_continue_dest [label=\" continue\","
         << kContinueStyle << "];\n";
  }
  // Emits the graph postamble.
  void End() const { out_ << "}\n"; }

  // Emits the Dot commands for the given instruction.
  spv_result_t HandleInstruction(const spv_parsed_instruction_t& inst);

 private:
  // Ends processing for the current block, emitting its dot code.
  void FlushBlock(const std::vector<uint32_t>& successors);

  // The ID of the current functio, or 0 if outside of a function.
  uint32_t current_function_id_ = 0;

  // The ID of the current basic block, or 0 if outside of a block.
  uint32_t current_block_id_ = 0;

  // Have we completed processing for the entry block to this fuction?
  bool seen_function_entry_block_ = false;

  // The Id of the merge block for this block if it exists, or 0 otherwise.
  uint32_t merge_ = 0;
  // The Id of the continue target block for this block if it exists, or 0
  // otherwise.
  uint32_t continue_target_ = 0;

  // The output stream.
  std::ostream& out_;
};

spv_result_t DotConverter::HandleInstruction(
    const spv_parsed_instruction_t& inst) {
  switch(inst.opcode) {
    case SpvOpFunction:
      current_function_id_ = inst.result_id;
      seen_function_entry_block_ = false;
      break;
    case SpvOpFunctionEnd:
      current_function_id_ = 0;
      break;

    case SpvOpLabel:
      current_block_id_ = inst.result_id;
      break;

    case SpvOpBranch:
      FlushBlock({inst.words[1]});
      break;
    case SpvOpBranchConditional:
      FlushBlock({inst.words[2], inst.words[3]});
      break;
    case SpvOpSwitch: {
      std::vector<uint32_t> successors{inst.words[2]};
      for (size_t i = 3; i < inst.num_operands; i += 2) {
        successors.push_back(inst.words[inst.operands[i].offset]);
      }
      FlushBlock(successors);
    } break;

    case SpvOpKill:
    case SpvOpReturn:
    case SpvOpUnreachable:
    case SpvOpReturnValue:
      FlushBlock({});
      break;

    case SpvOpLoopMerge:
      merge_ = inst.words[1];
      continue_target_ = inst.words[2];
      break;
    case SpvOpSelectionMerge:
      merge_ = inst.words[1];
      break;
    default:
      break;
  }
  return SPV_SUCCESS;
}

void DotConverter::FlushBlock(const std::vector<uint32_t>& successors) {
  out_ << current_block_id_;
  if (!seen_function_entry_block_) {
    out_ << " [label=\"" << current_block_id_ << "\nFn " << current_function_id_
         << " entry\", shape=box]";
  }
  out_ << ";\n";

  for (auto successor : successors) {
    out_ << current_block_id_ << " -> " << successor << ";\n";
  }

  if (merge_) {
    out_ << current_block_id_ << " -> " << merge_ << " [" << kMergeStyle
         << "];\n";
  }
  if (continue_target_) {
    out_ << current_block_id_ << " -> " << continue_target_ << " ["
         << kContinueStyle << "];\n";
  }

  // Reset the book-keeping for a block.
  seen_function_entry_block_ = true;
  merge_ = 0;
  continue_target_ = 0;
}

spv_result_t HandleInstruction(
    void* user_data, const spv_parsed_instruction_t* parsed_instruction) {
  assert(user_data);
  auto converter = static_cast<DotConverter*>(user_data);
  return converter->HandleInstruction(*parsed_instruction);
}

}  // anonymous namespace

spv_result_t BinaryToDot(const spv_const_context context, const uint32_t* words,
                         size_t num_words, std::iostream* out,
                         spv_diagnostic* diagnostic) {
  // Invalid arguments return error codes, but don't necessarily generate
  // diagnostics.  These are programmer errors, not user errors.
  if (!diagnostic) return SPV_ERROR_INVALID_DIAGNOSTIC;
  const libspirv::AssemblyGrammar grammar(context);
  if (!grammar.isValid()) return SPV_ERROR_INVALID_TABLE;

  DotConverter converter(out);
  converter.Begin();
  if (auto error = spvBinaryParse(context, &converter, words, num_words,
                                  nullptr, HandleInstruction, diagnostic)) {
    return error;
  }
  converter.End();

  return SPV_SUCCESS;
}
