#!/usr/bin/env python
"""Generates various info tables from SPIR-V JSON grammar."""

from __future__ import print_function

import functools
import json
import re

# Prefix for all C variables generated by this script.
PYGEN_VARIABLE_PREFIX = 'pygen_variable'

CAPABILITY_BIT_MAPPING = {}


def populate_capability_bit_mapping_dict(cap_dict):
    """Populates CAPABILITY_BIT_MAPPING.

    Arguments:
      - cap_dict: a dict containing all capability names and values
    """
    assert cap_dict['category'] == 'ValueEnum'
    assert cap_dict['kind'] == 'Capability'
    for enumerant in cap_dict['enumerants']:
        CAPABILITY_BIT_MAPPING[enumerant['enumerant']] = enumerant['value']


def compose_capability_mask(caps):
    """Returns a bit mask for a sequence of capabilities

    Arguments:
      - caps: a sequence of capability names

    Returns:
      a string containing the hexadecimal value of the bit mask
    """
    assert len(CAPABILITY_BIT_MAPPING) != 0
    bits = [CAPABILITY_BIT_MAPPING[c] for c in caps]
    caps_mask = functools.reduce(lambda m, b: m | (1 << b), bits, 0)
    return '0x{:04x}'.format(caps_mask)


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
    if kind == 'IdType':
        kind = 'TypeId'
    elif kind == 'IdResult':
        kind = 'ResultId'
    elif kind == 'IdMemorySemantics':
        kind = 'MemorySemanticsId'
    elif kind == 'IdScope':
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
    """Prints a SPIR-V instruction as the initializer for spv_opcode_desc_t."""

    def __init__(self, opname, caps, operands):
        """Initialization.

        Arguments:
          - opname: opcode name (with the 'Op' prefix)
          - caps: a sequence of capability names required by this opcode
          - operands: a sequence of (operand-kind, operand-quantifier) tuples
        """
        assert opname.startswith('Op')
        self.opname = opname[2:]  # Remove the "Op" prefix.
        self.caps_mask = compose_capability_mask(caps)
        self.operands = [convert_operand_kind(o) for o in operands]

        operands = [o[0] for o in operands]
        self.ref_type_id = 'IdType' in operands
        self.def_result_id = 'IdResult' in operands

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


def generate_instruction(inst):
    """Returns the C initializer for the given SPIR-V instruction.

    Arguments:
      - inst: a dict containing information about a SPIR-V instruction

    Returns:
      a string containing the C initializer for spv_opcode_desc_t
    """
    opname = inst.get('opname')
    caps = inst.get('capabilities', [])
    operands = inst.get('operands', {})
    operands = [(o['kind'], o.get('quantifier', '')) for o in operands]

    assert opname is not None

    return str(InstInitializer(opname, caps, operands))


def generate_instruction_table(inst_table):
    """Returns the info table containing all SPIR-V instructions.

    Arguments:
      - inst_table: a dict containing all SPIR-V instructions.
    """
    return ',\n'.join([generate_instruction(inst) for inst in inst_table])


class EnumerantInitializer(object):
    """Prints an enumerant as the initializer for spv_operand_desc_t."""

    def __init__(self, enumerant, value, caps, parameters):
        """Initialization.

        Arguments:
          - enumerant: enumerant name
          - value: enumerant value
          - caps: a sequence of capability names required by this enumerant
          - parameters: a sequence of (operand-kind, operand-quantifier) tuples
        """
        self.enumerant = enumerant
        self.value = value
        self.caps_mask = compose_capability_mask(caps)
        self.parameters = [convert_operand_kind(p) for p in parameters]

    def __str__(self):
        template = ['{{"{enumerant}"', '{value}',
                    '{caps_mask}', '{{{parameters}}}}}']
        return ', '.join(template).format(
            enumerant=self.enumerant,
            value=self.value,
            caps_mask=self.caps_mask,
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
    params = entry.get('parameters', [])
    params = [p.get('kind') for p in params]
    params = zip(params, [''] * len(params))

    assert enumerant is not None
    assert value is not None

    return str(EnumerantInitializer(enumerant, value, caps, params))


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


def main():
    import argparse
    parser = argparse.ArgumentParser(description='Generate SPIR-V info tables')
    parser.add_argument('grammar', metavar='<json-grammar-path>', type=str,
                        help='input JSON file path for SPIR-V grammar')
    parser.add_argument('--opcode-file', dest='opcode', metavar='<path>',
                        type=str, required=True,
                        help='output file path for SPIR-V instructions')
    parser.add_argument('--operand-file', dest='operand', metavar='<path>',
                        type=str, required=True,
                        help='output file path for SPIR-V operands')
    args = parser.parse_args()

    with open(args.grammar) as json_file:
        grammar = json.loads(json_file.read())

        # Get the dict for the Capability operand kind.
        cap_dict = [o for o in grammar['operand_kinds']
                    if o['kind'] == 'Capability']
        assert len(cap_dict) == 1
        populate_capability_bit_mapping_dict(cap_dict[0])

        print(generate_instruction_table(grammar['instructions']),
              file=open(args.opcode, 'w'))
        print(generate_operand_kind_table(grammar['operand_kinds']),
              file=open(args.operand, 'w'))

if __name__ == '__main__':
    main()
