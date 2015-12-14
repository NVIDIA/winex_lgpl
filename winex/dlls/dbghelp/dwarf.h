/*
 * dwarf2 definitions
 *
 * Copyright (C) 2005, Raphael Junqueira
 * Copyright (c) 2010-2015 NVIDIA CORPORATION. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

typedef struct
{
  unsigned char length[4];
  unsigned char version[2];
  unsigned char abbrev_offset[4];
  unsigned char word_size[1];
} dwarf2_comp_unit_stream_t;

typedef struct
{
  unsigned long  length;
  unsigned short version;
  unsigned long  abbrev_offset;
  unsigned char  word_size;
} dwarf2_comp_unit_t;

typedef struct
{
  unsigned int   length;
  unsigned short version;
  unsigned int   prologue_length;
  unsigned char  min_insn_length;
  unsigned char  default_is_stmt;
  int            line_base;
  unsigned char  line_range;
  unsigned char  opcode_base;
} dwarf2_line_info_t;

typedef enum dwarf_tag_e
{
  DW_TAG_padding                        = 0x00,
  DW_TAG_array_type                     = 0x01,
  DW_TAG_class_type                     = 0x02,
  DW_TAG_entry_point                    = 0x03,
  DW_TAG_enumeration_type               = 0x04,
  DW_TAG_formal_parameter               = 0x05,
  DW_TAG_imported_declaration           = 0x08,
  DW_TAG_label                          = 0x0a,
  DW_TAG_lexical_block                  = 0x0b,
  DW_TAG_member                         = 0x0d,
  DW_TAG_pointer_type                   = 0x0f,
  DW_TAG_reference_type                 = 0x10,
  DW_TAG_compile_unit                   = 0x11,
  DW_TAG_string_type                    = 0x12,
  DW_TAG_structure_type                 = 0x13,
  DW_TAG_subroutine_type                = 0x15,
  DW_TAG_typedef                        = 0x16,
  DW_TAG_union_type                     = 0x17,
  DW_TAG_unspecified_parameters         = 0x18,
  DW_TAG_variant                        = 0x19,
  DW_TAG_common_block                   = 0x1a,
  DW_TAG_common_inclusion               = 0x1b,
  DW_TAG_inheritance                    = 0x1c,
  DW_TAG_inlined_subroutine             = 0x1d,
  DW_TAG_module                         = 0x1e,
  DW_TAG_ptr_to_member_type             = 0x1f,
  DW_TAG_set_type                       = 0x20,
  DW_TAG_subrange_type                  = 0x21,
  DW_TAG_with_stmt                      = 0x22,
  DW_TAG_access_declaration             = 0x23,
  DW_TAG_base_type                      = 0x24,
  DW_TAG_catch_block                    = 0x25,
  DW_TAG_const_type                     = 0x26,
  DW_TAG_constant                       = 0x27,
  DW_TAG_enumerator                     = 0x28,
  DW_TAG_file_type                      = 0x29,
  DW_TAG_friend                         = 0x2a,
  DW_TAG_namelist                       = 0x2b,
  DW_TAG_namelist_item                  = 0x2c,
  DW_TAG_packed_type                    = 0x2d,
  DW_TAG_subprogram                     = 0x2e,
  DW_TAG_template_type_param            = 0x2f,
  DW_TAG_template_value_param           = 0x30,
  DW_TAG_thrown_type                    = 0x31,
  DW_TAG_try_block                      = 0x32,
  DW_TAG_variant_part                   = 0x33,
  DW_TAG_variable                       = 0x34,
  DW_TAG_volatile_type                  = 0x35,
  /** extensions */
  DW_TAG_MIPS_loop                      = 0x4081,
  DW_TAG_format_label                   = 0x4101,
  DW_TAG_function_template              = 0x4102,
  DW_TAG_class_template                 = 0x4103
} dwarf_tag_t;

typedef enum dwarf_attribute_e
{
  DW_AT_sibling                         = 0x01,
  DW_AT_location                        = 0x02,
  DW_AT_name                            = 0x03,
  DW_AT_ordering                        = 0x09,
  DW_AT_subscr_data                     = 0x0a,
  DW_AT_byte_size                       = 0x0b,
  DW_AT_bit_offset                      = 0x0c,
  DW_AT_bit_size                        = 0x0d,
  DW_AT_element_list                    = 0x0f,
  DW_AT_stmt_list                       = 0x10,
  DW_AT_low_pc                          = 0x11,
  DW_AT_high_pc                         = 0x12,
  DW_AT_language                        = 0x13,
  DW_AT_member                          = 0x14,
  DW_AT_discr                           = 0x15,
  DW_AT_discr_value                     = 0x16,
  DW_AT_visibility                      = 0x17,
  DW_AT_import                          = 0x18,
  DW_AT_string_length                   = 0x19,
  DW_AT_common_reference                = 0x1a,
  DW_AT_comp_dir                        = 0x1b,
  DW_AT_const_value                     = 0x1c,
  DW_AT_containing_type                 = 0x1d,
  DW_AT_default_value                   = 0x1e,
  DW_AT_inline                          = 0x20,
  DW_AT_is_optional                     = 0x21,
  DW_AT_lower_bound                     = 0x22,
  DW_AT_producer                        = 0x25,
  DW_AT_prototyped                      = 0x27,
  DW_AT_return_addr                     = 0x2a,
  DW_AT_start_scope                     = 0x2c,
  DW_AT_stride_size                     = 0x2e,
  DW_AT_upper_bound                     = 0x2f,
  DW_AT_abstract_origin                 = 0x31,
  DW_AT_accessibility                   = 0x32,
  DW_AT_address_class                   = 0x33,
  DW_AT_artificial                      = 0x34,
  DW_AT_base_types                      = 0x35,
  DW_AT_calling_convention              = 0x36,
  DW_AT_count                           = 0x37,
  DW_AT_data_member_location            = 0x38,
  DW_AT_decl_column                     = 0x39,
  DW_AT_decl_file                       = 0x3a,
  DW_AT_decl_line                       = 0x3b,
  DW_AT_declaration                     = 0x3c,
  DW_AT_discr_list                      = 0x3d,
  DW_AT_encoding                        = 0x3e,
  DW_AT_external                        = 0x3f,
  DW_AT_frame_base                      = 0x40,
  DW_AT_friend                          = 0x41,
  DW_AT_identifier_case                 = 0x42,
  DW_AT_macro_info                      = 0x43,
  DW_AT_namelist_items                  = 0x44,
  DW_AT_priority                        = 0x45,
  DW_AT_segment                         = 0x46,
  DW_AT_specification                   = 0x47,
  DW_AT_static_link                     = 0x48,
  DW_AT_type                            = 0x49,
  DW_AT_use_location                    = 0x4a,
  DW_AT_variable_parameter              = 0x4b,
  DW_AT_virtuality                      = 0x4c,
  DW_AT_vtable_elem_location            = 0x4d,

  DW_AT_ranges                          = 0x55,
  /* extensions */
  DW_AT_MIPS_fde                        = 0x2001,
  DW_AT_MIPS_loop_begin                 = 0x2002,
  DW_AT_MIPS_tail_loop_begin            = 0x2003,
  DW_AT_MIPS_epilog_begin               = 0x2004,
  DW_AT_MIPS_loop_unroll_factor         = 0x2005,
  DW_AT_MIPS_software_pipeline_depth    = 0x2006,
  DW_AT_MIPS_linkage_name               = 0x2007,
  DW_AT_MIPS_stride                     = 0x2008,
  DW_AT_MIPS_abstract_name              = 0x2009,
  DW_AT_MIPS_clone_origin               = 0x200a,
  DW_AT_MIPS_has_inlines                = 0x200b,
  DW_AT_sf_names                        = 0x2101,
  DW_AT_src_info                        = 0x2102,
  DW_AT_mac_info                        = 0x2103,
  DW_AT_src_coords                      = 0x2104,
  DW_AT_body_begin                      = 0x2105,
  DW_AT_body_end                        = 0x2106
} dwarf_attribute_t;

typedef enum dwarf_form_e
{
  DW_FORM_addr                          = 0x01,
  DW_FORM_block2                        = 0x03,
  DW_FORM_block4                        = 0x04,
  DW_FORM_data2                         = 0x05,
  DW_FORM_data4                         = 0x06,
  DW_FORM_data8                         = 0x07,
  DW_FORM_string                        = 0x08,
  DW_FORM_block                         = 0x09,
  DW_FORM_block1                        = 0x0a,
  DW_FORM_data1                         = 0x0b,
  DW_FORM_flag                          = 0x0c,
  DW_FORM_sdata                         = 0x0d,
  DW_FORM_strp                          = 0x0e,
  DW_FORM_udata                         = 0x0f,
  DW_FORM_ref_addr                      = 0x10,
  DW_FORM_ref1                          = 0x11,
  DW_FORM_ref2                          = 0x12,
  DW_FORM_ref4                          = 0x13,
  DW_FORM_ref8                          = 0x14,
  DW_FORM_ref_udata                     = 0x15,
  DW_FORM_indirect                      = 0x16
} dwarf_form_t;

/** type encoding */
typedef enum dwarf_type_e
{
  DW_ATE_void                           = 0x0,
  DW_ATE_address                        = 0x1,
  DW_ATE_boolean                        = 0x2,
  DW_ATE_complex_float                  = 0x3,
  DW_ATE_float                          = 0x4,
  DW_ATE_signed                         = 0x5,
  DW_ATE_signed_char                    = 0x6,
  DW_ATE_unsigned                       = 0x7,
  DW_ATE_unsigned_char                  = 0x8
} dwarf_type_t;

typedef enum dwarf_operation_e
{
  DW_OP_addr                            = 0x03,
  DW_OP_deref                           = 0x06,
  DW_OP_const1u                         = 0x08,
  DW_OP_const1s                         = 0x09,
  DW_OP_const2u                         = 0x0a,
  DW_OP_const2s                         = 0x0b,
  DW_OP_const4u                         = 0x0c,
  DW_OP_const4s                         = 0x0d,
  DW_OP_const8u                         = 0x0e,
  DW_OP_const8s                         = 0x0f,
  DW_OP_constu                          = 0x10,
  DW_OP_consts                          = 0x11,
  DW_OP_dup                             = 0x12,
  DW_OP_drop                            = 0x13,
  DW_OP_over                            = 0x14,
  DW_OP_pick                            = 0x15,
  DW_OP_swap                            = 0x16,
  DW_OP_rot                             = 0x17,
  DW_OP_xderef                          = 0x18,
  DW_OP_abs                             = 0x19,
  DW_OP_and                             = 0x1a,
  DW_OP_div                             = 0x1b,
  DW_OP_minus                           = 0x1c,
  DW_OP_mod                             = 0x1d,
  DW_OP_mul                             = 0x1e,
  DW_OP_neg                             = 0x1f,
  DW_OP_not                             = 0x20,
  DW_OP_or                              = 0x21,
  DW_OP_plus                            = 0x22,
  DW_OP_plus_uconst                     = 0x23,
  DW_OP_shl                             = 0x24,
  DW_OP_shr                             = 0x25,
  DW_OP_shra                            = 0x26,
  DW_OP_xor                             = 0x27,
  DW_OP_bra                             = 0x28,
  DW_OP_eq                              = 0x29,
  DW_OP_ge                              = 0x2a,
  DW_OP_gt                              = 0x2b,
  DW_OP_le                              = 0x2c,
  DW_OP_lt                              = 0x2d,
  DW_OP_ne                              = 0x2e,
  DW_OP_skip                            = 0x2f,
  DW_OP_lit0                            = 0x30,
  DW_OP_lit1                            = 0x31,
  DW_OP_lit2                            = 0x32,
  DW_OP_lit3                            = 0x33,
  DW_OP_lit4                            = 0x34,
  DW_OP_lit5                            = 0x35,
  DW_OP_lit6                            = 0x36,
  DW_OP_lit7                            = 0x37,
  DW_OP_lit8                            = 0x38,
  DW_OP_lit9                            = 0x39,
  DW_OP_lit10                           = 0x3a,
  DW_OP_lit11                           = 0x3b,
  DW_OP_lit12                           = 0x3c,
  DW_OP_lit13                           = 0x3d,
  DW_OP_lit14                           = 0x3e,
  DW_OP_lit15                           = 0x3f,
  DW_OP_lit16                           = 0x40,
  DW_OP_lit17                           = 0x41,
  DW_OP_lit18                           = 0x42,
  DW_OP_lit19                           = 0x43,
  DW_OP_lit20                           = 0x44,
  DW_OP_lit21                           = 0x45,
  DW_OP_lit22                           = 0x46,
  DW_OP_lit23                           = 0x47,
  DW_OP_lit24                           = 0x48,
  DW_OP_lit25                           = 0x49,
  DW_OP_lit26                           = 0x4a,
  DW_OP_lit27                           = 0x4b,
  DW_OP_lit28                           = 0x4c,
  DW_OP_lit29                           = 0x4d,
  DW_OP_lit30                           = 0x4e,
  DW_OP_lit31                           = 0x4f,
  DW_OP_reg0                            = 0x50,
  DW_OP_reg1                            = 0x51,
  DW_OP_reg2                            = 0x52,
  DW_OP_reg3                            = 0x53,
  DW_OP_reg4                            = 0x54,
  DW_OP_reg5                            = 0x55,
  DW_OP_reg6                            = 0x56,
  DW_OP_reg7                            = 0x57,
  DW_OP_reg8                            = 0x58,
  DW_OP_reg9                            = 0x59,
  DW_OP_reg10                           = 0x5a,
  DW_OP_reg11                           = 0x5b,
  DW_OP_reg12                           = 0x5c,
  DW_OP_reg13                           = 0x5d,
  DW_OP_reg14                           = 0x5e,
  DW_OP_reg15                           = 0x5f,
  DW_OP_reg16                           = 0x60,
  DW_OP_reg17                           = 0x61,
  DW_OP_reg18                           = 0x62,
  DW_OP_reg19                           = 0x63,
  DW_OP_reg20                           = 0x64,
  DW_OP_reg21                           = 0x65,
  DW_OP_reg22                           = 0x66,
  DW_OP_reg23                           = 0x67,
  DW_OP_reg24                           = 0x68,
  DW_OP_reg25                           = 0x69,
  DW_OP_reg26                           = 0x6a,
  DW_OP_reg27                           = 0x6b,
  DW_OP_reg28                           = 0x6c,
  DW_OP_reg29                           = 0x6d,
  DW_OP_reg30                           = 0x6e,
  DW_OP_reg31                           = 0x6f,
  DW_OP_breg0                           = 0x70,
  DW_OP_breg1                           = 0x71,
  DW_OP_breg2                           = 0x72,
  DW_OP_breg3                           = 0x73,
  DW_OP_breg4                           = 0x74,
  DW_OP_breg5                           = 0x75,
  DW_OP_breg6                           = 0x76,
  DW_OP_breg7                           = 0x77,
  DW_OP_breg8                           = 0x78,
  DW_OP_breg9                           = 0x79,
  DW_OP_breg10                          = 0x7a,
  DW_OP_breg11                          = 0x7b,
  DW_OP_breg12                          = 0x7c,
  DW_OP_breg13                          = 0x7d,
  DW_OP_breg14                          = 0x7e,
  DW_OP_breg15                          = 0x7f,
  DW_OP_breg16                          = 0x80,
  DW_OP_breg17                          = 0x81,
  DW_OP_breg18                          = 0x82,
  DW_OP_breg19                          = 0x83,
  DW_OP_breg20                          = 0x84,
  DW_OP_breg21                          = 0x85,
  DW_OP_breg22                          = 0x86,
  DW_OP_breg23                          = 0x87,
  DW_OP_breg24                          = 0x88,
  DW_OP_breg25                          = 0x89,
  DW_OP_breg26                          = 0x8a,
  DW_OP_breg27                          = 0x8b,
  DW_OP_breg28                          = 0x8c,
  DW_OP_breg29                          = 0x8d,
  DW_OP_breg30                          = 0x8e,
  DW_OP_breg31                          = 0x8f,
  DW_OP_regx                            = 0x90,
  DW_OP_fbreg                           = 0x91,
  DW_OP_bregx                           = 0x92,
  DW_OP_piece                           = 0x93,
  DW_OP_deref_size                      = 0x94,
  DW_OP_xderef_size                     = 0x95,
  DW_OP_nop                             = 0x96,
  /** Dwarf3 new values */
  DW_OP_push_object_address             = 0x97,
  DW_OP_call2                           = 0x98,
  DW_OP_call4                           = 0x99,
  DW_OP_call_ref                        = 0x9a,
  DW_OP_form_tls_address                = 0x9b,
  DW_OP_call_frame_cfa                  = 0x9c,
  DW_OP_bit_piece                       = 0x9d,

  /* Implementation defined extensions */
  DW_OP_lo_user                         = 0xe0,
  DW_OP_hi_user                         = 0xff,

  /* GNU extensions */
  DW_OP_GNU_push_tls_address            = 0xe0,
  DW_OP_GNU_uninit                      = 0xf0,
  DW_OP_GNU_encoded_addr                = 0xf1,
} dwarf_operation_t;

enum dwarf_calling_convention
{
    DW_CC_normal                        = 0x1,
    DW_CC_program                       = 0x2,
    DW_CC_nocall                        = 0x3
};

#define DW_CC_lo_user 0x40
#define DW_CC_hi_user 0xff

#define DW_LNS_extended_op              0x00
#define DW_LNS_copy                     0x01
#define DW_LNS_advance_pc               0x02
#define DW_LNS_advance_line             0x03
#define DW_LNS_set_file                 0x04
#define DW_LNS_set_column               0x05
#define DW_LNS_negate_stmt              0x06
#define DW_LNS_set_basic_block          0x07
#define DW_LNS_const_add_pc             0x08
#define DW_LNS_fixed_advance_pc         0x09

#define DW_LNE_end_sequence             0x01
#define DW_LNE_set_address              0x02
#define DW_LNE_define_file              0x03
