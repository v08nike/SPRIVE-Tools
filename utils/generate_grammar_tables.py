#!/usr/bin/env python
# Copyright (c) 2016 Google Inc.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Generates various info tables from SPIR-V JSON grammar."""

from __future__ import print_function

import errno
import functools
import json
import os.path
import re

# Prefix for all C variables generated by this script.
PYGEN_VARIABLE_PREFIX = 'pygen_variable'

def make_path_to_file(f):
    """Makes all ancestor directories to the given file, if they
    don't yet exist.

    Arguments:
        f: The file whose ancestor directories are to be created.
    """
    dir = os.path.dirname(os.path.abspath(f))
    try:
        os.makedirs(dir)
    except OSError as e:
        if e.errno == errno.EEXIST and os.path.isdir(dir):
            pass
        else:
            raise

def compose_capability_list(caps):
    """Returns a string containing a braced list of capabilities as enums.

    Arguments:
      - caps: a sequence of capability names

    Returns:
      a string containing the braced list of SpvCapability* enums named by caps.
    """
    return "{" + ", ".join(['SpvCapability{}'.format(c) for c in caps]) + "}"


def compose_extension_list(exts):
    """Returns a string containing a braced list of extensions as enums.

    Arguments:
      - exts: a sequence of extension names

    Returns:
      a string containing the braced list of extensions named by exts.
    """
    return "{" + ", ".join(['libspirv::Extension::k{}'.format(e) for e in exts]) + "}"


def convert_operand_kind(operand_tuple):
    """Returns the corresponding operand type used in spirv-tools for
    the given operand kind and quantifier used in the JSON grammar.

    Arguments:
      - operand_tuple: a tuple of two elements:
          - operand kind: used in the JSON grammar
          - quantifier: '', '?', or '*'

    Returns:
      a string of the enumerant name in spv_operand_type_t
    """
    kind, quantifier = operand_tuple
    # The following cases are where we differ between the JSON grammar and
    # spirv-tools.
    if kind == 'IdResultType':
        kind = 'TypeId'
    elif kind == 'IdResult':
        kind = 'ResultId'
    elif kind == 'IdMemorySemantics' or kind == 'MemorySemantics':
        kind = 'MemorySemanticsId'
    elif kind == 'IdScope' or kind == 'Scope':
        kind = 'ScopeId'
    elif kind == 'IdRef':
        kind = 'Id'

    elif kind == 'ImageOperands':
        kind = 'Image'
    elif kind == 'Dim':
        kind = 'Dimensionality'
    elif kind == 'ImageFormat':
        kind = 'SamplerImageFormat'
    elif kind == 'KernelEnqueueFlags':
        kind = 'KernelEnqFlags'

    elif kind == 'LiteralExtInstInteger':
        kind = 'ExtensionInstructionNumber'
    elif kind == 'LiteralSpecConstantOpInteger':
        kind = 'SpecConstantOpNumber'
    elif kind == 'LiteralContextDependentNumber':
        kind = 'TypedLiteralNumber'

    elif kind == 'PairLiteralIntegerIdRef':
        kind = 'LiteralIntegerId'
    elif kind == 'PairIdRefLiteralInteger':
        kind = 'IdLiteralInteger'
    elif kind == 'PairIdRefIdRef':  # Used by OpPhi in the grammar
        kind = 'Id'

    if kind == 'FPRoundingMode':
        kind = 'FpRoundingMode'
    elif kind == 'FPFastMathMode':
        kind = 'FpFastMathMode'

    if quantifier == '?':
        kind = 'Optional{}'.format(kind)
    elif quantifier == '*':
        kind = 'Variable{}'.format(kind)

    return 'SPV_OPERAND_TYPE_{}'.format(
        re.sub(r'([a-z])([A-Z])', r'\1_\2', kind).upper())


class InstInitializer(object):
    """Instances holds a SPIR-V instruction suitable for printing as
    the initializer for spv_opcode_desc_t."""

    def __init__(self, opname, caps, operands):
        """Initialization.

        Arguments:
          - opname: opcode name (with the 'Op' prefix)
          - caps: a sequence of capability names required by this opcode
          - operands: a sequence of (operand-kind, operand-quantifier) tuples
        """
        assert opname.startswith('Op')
        self.opname = opname[2:]  # Remove the "Op" prefix.
        self.caps_mask = compose_capability_list(caps)
        self.operands = [convert_operand_kind(o) for o in operands]

        self.fix_syntax()

        operands = [o[0] for o in operands]
        self.ref_type_id = 'IdResultType' in operands
        self.def_result_id = 'IdResult' in operands


    def fix_syntax(self):
        """Fix an instruction's syntax, adjusting for differences between
        the officially released grammar and how SPIRV-Tools uses the grammar.

        Fixes:
            - ExtInst should not end with SPV_OPERAND_VARIABLE_ID.
            https://github.com/KhronosGroup/SPIRV-Tools/issues/233
        """
        if (self.opname == 'ExtInst'
            and self.operands[-1] == 'SPV_OPERAND_TYPE_VARIABLE_ID'):
           self.operands.pop()


    def __str__(self):
        template = ['{{"{opname}"', 'SpvOp{opname}', '{caps_mask}',
                    '{num_operands}', '{{{operands}}}',
                    '{def_result_id}', '{ref_type_id}}}']
        return ', '.join(template).format(
            opname=self.opname,
            caps_mask=self.caps_mask,
            num_operands=len(self.operands),
            operands=', '.join(self.operands),
            def_result_id=(1 if self.def_result_id else 0),
            ref_type_id=(1 if self.ref_type_id else 0))


class ExtInstInitializer(object):
    """Instances holds a SPIR-V extended instruction suitable for printing as
    the initializer for spv_ext_inst_desc_t."""

    def __init__(self, opname, opcode, caps, operands):
        """Initialization.

        Arguments:
          - opname: opcode name
          - opcode: enumerant value for this opcode
          - caps: a sequence of capability names required by this opcode
          - operands: a sequence of (operand-kind, operand-quantifier) tuples
        """
        self.opname = opname
        self.opcode = opcode
        self.caps_mask = compose_capability_list(caps)
        self.operands = [convert_operand_kind(o) for o in operands]
        self.operands.append('SPV_OPERAND_TYPE_NONE')

    def __str__(self):
        template = ['{{"{opname}"', '{opcode}', '{caps_mask}',
                    '{{{operands}}}}}']
        return ', '.join(template).format(
            opname=self.opname,
            opcode=self.opcode,
            caps_mask=self.caps_mask,
            operands=', '.join(self.operands))


def generate_instruction(inst, is_ext_inst):
    """Returns the C initializer for the given SPIR-V instruction.

    Arguments:
      - inst: a dict containing information about a SPIR-V instruction
      - is_ext_inst: a bool indicating whether |inst| is an extended
                     instruction.

    Returns:
      a string containing the C initializer for spv_opcode_desc_t or
      spv_ext_inst_desc_t
    """
    opname = inst.get('opname')
    opcode = inst.get('opcode')
    caps = inst.get('capabilities', [])
    operands = inst.get('operands', {})
    operands = [(o['kind'], o.get('quantifier', '')) for o in operands]

    assert opname is not None

    if is_ext_inst:
        return str(ExtInstInitializer(opname, opcode, caps, operands))
    else:
        return str(InstInitializer(opname, caps, operands))


def generate_instruction_table(inst_table, is_ext_inst):
    """Returns the info table containing all SPIR-V instructions.

    Arguments:
      - inst_table: a dict containing all SPIR-V instructions.
      - is_ext_inst: a bool indicating whether |inst_table| is for
                     an extended instruction set.
    """
    return ',\n'.join([generate_instruction(inst, is_ext_inst)
                       for inst in inst_table])


class EnumerantInitializer(object):
    """Prints an enumerant as the initializer for spv_operand_desc_t."""

    def __init__(self, enumerant, value, caps, exts, parameters):
        """Initialization.

        Arguments:
          - enumerant: enumerant name
          - value: enumerant value
          - caps: a sequence of capability names required by this enumerant
          - exts: a sequence of names of extensions enabling this enumerant
          - parameters: a sequence of (operand-kind, operand-quantifier) tuples
        """
        self.enumerant = enumerant
        self.value = value
        self.caps = compose_capability_list(caps)
        self.exts = compose_extension_list(exts)
        self.parameters = [convert_operand_kind(p) for p in parameters]

    def __str__(self):
        template = ['{{"{enumerant}"', '{value}',
                    '{caps}', '{exts}', '{{{parameters}}}}}']
        return ', '.join(template).format(
            enumerant=self.enumerant,
            value=self.value,
            caps=self.caps,
            exts=self.exts,
            parameters=', '.join(self.parameters))


def generate_enum_operand_kind_entry(entry):
    """Returns the C initializer for the given operand enum entry.

    Arguments:
      - entry: a dict containing information about an enum entry

    Returns:
      a string containing the C initializer for spv_operand_desc_t
    """
    enumerant = entry.get('enumerant')
    value = entry.get('value')
    caps = entry.get('capabilities', [])
    exts = entry.get('extensions', [])
    params = entry.get('parameters', [])
    params = [p.get('kind') for p in params]
    params = zip(params, [''] * len(params))

    assert enumerant is not None
    assert value is not None

    return str(EnumerantInitializer(enumerant, value, caps, exts, params))


def generate_enum_operand_kind(enum):
    """Returns the C definition for the given operand kind."""
    kind = enum.get('kind')
    assert kind is not None

    name = '{}_{}Entries'.format(PYGEN_VARIABLE_PREFIX, kind)
    entries = ['  {}'.format(generate_enum_operand_kind_entry(e))
               for e in enum.get('enumerants', [])]

    template = ['static const spv_operand_desc_t {name}[] = {{',
                '{entries}', '}};']
    entries = '\n'.join(template).format(
        name=name,
        entries=',\n'.join(entries))

    return kind, name, entries


def generate_operand_kind_table(enums):
    """Returns the info table containing all SPIR-V operand kinds."""
    # We only need to output info tables for those operand kinds that are enums.
    enums = [generate_enum_operand_kind(e)
             for e in enums
             if e.get('category') in ['ValueEnum', 'BitEnum']]
    # We have three operand kinds that requires their optional counterpart to
    # exist in the operand info table.
    three_optional_enums = ['ImageOperands', 'AccessQualifier', 'MemoryAccess']
    three_optional_enums = [e for e in enums if e[0] in three_optional_enums]
    enums.extend(three_optional_enums)

    enum_kinds, enum_names, enum_entries = zip(*enums)
    # Mark the last three as optional ones.
    enum_quantifiers = [''] * (len(enums) - 3) + ['?'] * 3
    # And we don't want redefinition of them.
    enum_entries = enum_entries[:-3]
    enum_kinds = [convert_operand_kind(e)
                  for e in zip(enum_kinds, enum_quantifiers)]
    table_entries = zip(enum_kinds, enum_names, enum_names)
    table_entries = ['  {{{}, ARRAY_SIZE({}), {}}}'.format(*e)
                     for e in table_entries]

    template = [
        'static const spv_operand_desc_group_t {p}_OperandInfoTable[] = {{',
        '{enums}', '}};']
    table = '\n'.join(template).format(
        p=PYGEN_VARIABLE_PREFIX, enums=',\n'.join(table_entries))

    return '\n\n'.join(enum_entries + (table,))


def get_extension_list(operands):
    """Returns extensions as an alphabetically sorted list of strings."""
    enumerants = sum([item.get('enumerants', []) for item in operands
                      if item.get('category') in ['ValueEnum']], [])

    extensions = sum([item.get('extensions', []) for item in enumerants
                      if item.get('extensions')], [])

    return sorted(set(extensions))


def get_capability_list(operands):
    """Returns capabilities as a list of strings in the order of appearance."""
    enumerants = sum([item.get('enumerants', []) for item in operands
                      if item.get('kind') in ['Capability']], [])
    return [item.get('enumerant') for item in enumerants]


def generate_extension_enum(operands):
    """Returns enumeration containing extensions declared in the grammar."""
    extensions = get_extension_list(operands)
    return ',\n'.join(['k' + extension for extension in extensions])


def generate_extension_to_string_table(operands):
    """Returns extension to string mapping table."""
    extensions = get_extension_list(operands)
    entry_template = '  {{Extension::k{extension},\n   "{extension}"}}'
    table_entries = [entry_template.format(extension=extension)
                     for extension in extensions]
    table_template = '{{\n{enums}\n}}'
    return table_template.format(enums=',\n'.join(table_entries))


def generate_string_to_extension_table(operands):
    """Returns string to extension mapping table."""
    extensions = get_extension_list(operands)
    entry_template = '  {{"{extension}",\n   Extension::k{extension}}}'
    table_entries = [entry_template.format(extension=extension)
                     for extension in extensions]
    table_template = '{{\n{enums}\n}}'
    return table_template.format(enums=',\n'.join(table_entries))


def generate_capability_to_string_table(operands):
    """Returns capability to string mapping table."""
    capabilities = get_capability_list(operands)
    entry_template = '  {{SpvCapability{capability},\n   "{capability}"}}'
    table_entries = [entry_template.format(capability=capability)
                     for capability in capabilities]
    table_template = '{{\n{enums}\n}}'
    return table_template.format(enums=',\n'.join(table_entries))


def generate_extension_to_string_mapping(operands):
    """Returns mapping function from extensions to corresponding strings."""
    extensions = get_extension_list(operands)
    function = 'std::string ExtensionToString(Extension extension) {\n'
    function += '  switch (extension) {\n'
    template = '    case Extension::k{extension}:\n' \
        '      return "{extension}";\n'
    function += ''.join([template.format(extension=extension)
                         for extension in extensions])
    function += '  };\n\n  return "";\n}'
    return function


def generate_string_to_extension_mapping(operands):
    """Returns mapping function from strings to corresponding extensions."""
    function = 'bool GetExtensionFromString(' \
        'const std::string& str, Extension* extension) {\n ' \
        'static const std::unordered_map<std::string, Extension> mapping =\n'
    function += generate_string_to_extension_table(operands)
    function += ';\n\n'
    function += '  const auto it = mapping.find(str);\n\n' \
        '  if (it == mapping.end()) return false;\n\n' \
        '  *extension = it->second;\n  return true;\n}'
    return function


def generate_capability_to_string_mapping(operands):
    """Returns mapping function from capabilities to corresponding strings."""
    capabilities = get_capability_list(operands)
    function = 'std::string CapabilityToString(SpvCapability capability) {\n'
    function += '  switch (capability) {\n'
    template = '    case SpvCapability{capability}:\n' \
        '      return "{capability}";\n'
    function += ''.join([template.format(capability=capability)
                         for capability in capabilities])
    function += '    case SpvCapabilityMax:\n' \
        '      assert(0 && "Attempting to convert SpvCapabilityMax to string");\n' \
        '      return "";\n'
    function += '  };\n\n  return "";\n}'
    return function


def generate_all_string_enum_mappings(operands):
    """Returns all string-to-enum / enum-to-string mapping tables."""
    tables = []
    tables.append(generate_extension_to_string_mapping(operands))
    tables.append(generate_string_to_extension_mapping(operands))
    tables.append(generate_capability_to_string_mapping(operands))
    return '\n\n'.join(tables)


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Generate SPIR-V info tables')
    parser.add_argument('--spirv-core-grammar', metavar='<path>',
                        type=str, required=True,
                        help='input JSON grammar file for core SPIR-V '
                        'instructions')
    parser.add_argument('--extinst-glsl-grammar', metavar='<path>',
                        type=str, required=False, default=None,
                        help='input JSON grammar file for GLSL extended '
                        'instruction set')
    parser.add_argument('--extinst-opencl-grammar', metavar='<path>',
                        type=str, required=False, default=None,
                        help='input JSON grammar file for OpenGL extended '
                        'instruction set')
    parser.add_argument('--core-insts-output', metavar='<path>',
                        type=str, required=False, default=None,
                        help='output file for core SPIR-V instructions')
    parser.add_argument('--glsl-insts-output', metavar='<path>',
                        type=str, required=False, default=None,
                        help='output file for GLSL extended instruction set')
    parser.add_argument('--opencl-insts-output', metavar='<path>',
                        type=str, required=False, default=None,
                        help='output file for OpenCL extended instruction set')
    parser.add_argument('--operand-kinds-output', metavar='<path>',
                        type=str, required=False, default=None,
                        help='output file for operand kinds')
    parser.add_argument('--extension-enum-output', metavar='<path>',
                        type=str, required=False, default=None,
                        help='output file for extension enumeration')
    parser.add_argument('--enum-string-mapping-output', metavar='<path>',
                        type=str, required=False, default=None,
                        help='output file for enum-string mappings')
    args = parser.parse_args()

    if (args.core_insts_output is None) != \
            (args.operand_kinds_output is None):
        print('error: --core-insts-output and --operand_kinds_output '
              'should be specified together.')
        exit(1)
    if (args.glsl_insts_output is None) != \
            (args.extinst_glsl_grammar is None):
        print('error: --glsl-insts-output and --extinst-glsl-grammar '
              'should be specified together.')
        exit(1)
    if (args.opencl_insts_output is None) != \
            (args.extinst_opencl_grammar is None):
        print('error: --opencl-insts-output and --extinst-opencl-grammar '
              'should be specified together.')
        exit(1)
    if all([args.core_insts_output is None,
            args.glsl_insts_output is None,
            args.opencl_insts_output is None,
            args.extension_enum_output is None,
            args.enum_string_mapping_output is None]):
        print('error: at least one output should be specified.')
        exit(1)

    with open(args.spirv_core_grammar) as json_file:
        grammar = json.loads(json_file.read())
        if args.core_insts_output is not None:
            make_path_to_file(args.core_insts_output)
            make_path_to_file(args.operand_kinds_output)
            print(generate_instruction_table(grammar['instructions'], False),
                  file=open(args.core_insts_output, 'w'))
            print(generate_operand_kind_table(grammar['operand_kinds']),
                  file=open(args.operand_kinds_output, 'w'))
        if args.extension_enum_output is not None:
            make_path_to_file(args.extension_enum_output)
            print(generate_extension_enum(grammar['operand_kinds']),
                  file=open(args.extension_enum_output, 'w'))
        if args.enum_string_mapping_output is not None:
            make_path_to_file(args.enum_string_mapping_output)
            print(generate_all_string_enum_mappings(
                      grammar['operand_kinds']),
                  file=open(args.enum_string_mapping_output, 'w'))

    if args.extinst_glsl_grammar is not None:
        with open(args.extinst_glsl_grammar) as json_file:
            grammar = json.loads(json_file.read())
            make_path_to_file(args.glsl_insts_output)
            print(generate_instruction_table(grammar['instructions'], True),
                  file=open(args.glsl_insts_output, 'w'))

    if args.extinst_opencl_grammar is not None:
        with open(args.extinst_opencl_grammar) as json_file:
            grammar = json.loads(json_file.read())
            make_path_to_file(args.opencl_insts_output)
            print(generate_instruction_table(grammar['instructions'], True),
                  file=open(args.opencl_insts_output, 'w'))


if __name__ == '__main__':
    main()
