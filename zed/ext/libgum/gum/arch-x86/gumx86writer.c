/*
 * Copyright (C) 2009-2010 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gumx86writer.h"

#include "gummemory.h"

#include <string.h>

#define GUM_MAX_LABEL_COUNT (10 * 1000)
#define GUM_MAX_LREF_COUNT  (3 * GUM_MAX_LABEL_COUNT)

#define IS_WITHIN_INT8_RANGE(i) ((i) >= -128 && (i) <= 127)
#define IS_WITHIN_INT32_RANGE(i) ((i) >= G_MININT32 && (i) <= G_MAXINT32)

typedef struct _GumArgument
{
  GumArgType type;

  union
  {
    GumCpuReg reg;
    gpointer pointer;
  } value;
} GumArgument;

typedef enum _GumMetaReg
{
  GUM_META_REG_XAX = 0,
  GUM_META_REG_XCX,
  GUM_META_REG_XDX,
  GUM_META_REG_XBX,
  GUM_META_REG_XSP,
  GUM_META_REG_XBP,
  GUM_META_REG_XSI,
  GUM_META_REG_XDI,
  GUM_META_REG_R8,
  GUM_META_REG_R9,
  GUM_META_REG_R10,
  GUM_META_REG_R11,
  GUM_META_REG_R12,
  GUM_META_REG_R13,
  GUM_META_REG_R14,
  GUM_META_REG_R15
} GumMetaReg;

typedef struct _GumCpuRegInfo GumCpuRegInfo;

struct _GumCpuRegInfo
{
  GumMetaReg meta;
  guint width;
  guint index;
  gboolean index_is_extended;
};

typedef enum _GumLabelRefSize
{
  GUM_LREF_SHORT,
  GUM_LREF_NEAR
} GumLabelRefSize;

struct _GumLabelMapping
{
  gconstpointer id;
  gpointer address;
};

struct _GumLabelRef
{
  gconstpointer id;
  guint8 * address;
  GumLabelRefSize size;
};

static guint8 * gum_x86_writer_lookup_address_for_label_id (
    GumX86Writer * self, gconstpointer id);
static void gum_x86_writer_describe_cpu_reg (GumX86Writer * self,
    GumCpuReg reg, GumCpuRegInfo * ri);

static GumMetaReg gum_meta_reg_from_cpu_reg (GumCpuReg reg);

static void gum_x86_writer_put_prefix_for_reg_info (GumX86Writer * self,
    const GumCpuRegInfo * ri, guint operand_index);
static void gum_x86_writer_put_prefix_for_registers (GumX86Writer * self,
    const GumCpuRegInfo * width_reg, guint default_width, ...);

void
gum_x86_writer_init (GumX86Writer * writer,
                     gpointer code_address)
{
  writer->id_to_address = gum_new (GumLabelMapping, GUM_MAX_LABEL_COUNT);
  writer->label_refs = gum_new (GumLabelRef, GUM_MAX_LREF_COUNT);

  gum_x86_writer_reset (writer, code_address);
}

void
gum_x86_writer_reset (GumX86Writer * writer,
                      gpointer code_address)
{
#if GLIB_SIZEOF_VOID_P == 4
  writer->target_cpu = GUM_CPU_IA32;
#else
  writer->target_cpu = GUM_CPU_AMD64;
#endif
  writer->target_abi = GUM_NATIVE_ABI;

  writer->base = (guint8 *) code_address;
  writer->code = (guint8 *) code_address;

  writer->id_to_address_len = 0;
  writer->label_refs_len = 0;
}

void
gum_x86_writer_free (GumX86Writer * writer)
{
  gum_x86_writer_flush (writer);

  gum_free (writer->id_to_address);
  gum_free (writer->label_refs);
}

void
gum_x86_writer_set_target_cpu (GumX86Writer * writer,
                               GumCpuType cpu_type)
{
  writer->target_cpu = cpu_type;
}

void
gum_x86_writer_set_target_abi (GumX86Writer * writer,
                               GumAbiType abi_type)
{
  writer->target_abi = abi_type;
}

gpointer
gum_x86_writer_cur (GumX86Writer * self)
{
  return self->code;
}

guint
gum_x86_writer_offset (GumX86Writer * self)
{
  return self->code - self->base;
}

void
gum_x86_writer_flush (GumX86Writer * self)
{
  guint i;

  for (i = 0; i < self->label_refs_len; i++)
  {
    GumLabelRef * r = &self->label_refs[i];
    gpointer target_address;
    gint32 distance;

    target_address = gum_x86_writer_lookup_address_for_label_id (self, r->id);
    g_assert (target_address != NULL);

    distance = (gssize) target_address - (gssize) r->address;

    if (r->size == GUM_LREF_SHORT)
    {
      g_assert (IS_WITHIN_INT8_RANGE (distance));
      *((gint8 *) (r->address - 1)) = distance;
    }
    else
    {
      *((gint32 *) (r->address - 4)) = distance;
    }
  }

  self->label_refs_len = 0;
}

GumCpuReg
gum_x86_writer_get_cpu_register_for_nth_argument (GumX86Writer * self,
                                                  guint n)
{
  if (self->target_cpu == GUM_CPU_AMD64)
  {
    if (self->target_abi == GUM_ABI_UNIX)
    {
      static const GumCpuReg amd64_unix_reg_by_index[] = {
        GUM_REG_RDI,
        GUM_REG_RSI,
        GUM_REG_RDX,
        GUM_REG_RCX,
        GUM_REG_R8,
        GUM_REG_R9
      };

      if (n < G_N_ELEMENTS (amd64_unix_reg_by_index))
        return amd64_unix_reg_by_index[n];
    }
    else if (self->target_abi == GUM_ABI_WINDOWS)
    {
      static const GumCpuReg amd64_windows_reg_by_index[] = {
        GUM_REG_RCX,
        GUM_REG_RDX,
        GUM_REG_R8,
        GUM_REG_R9
      };

      if (n < G_N_ELEMENTS (amd64_windows_reg_by_index))
        return amd64_windows_reg_by_index[n];
    }
  }
  else if (self->target_cpu == GUM_CPU_IA32)
  {
    static const GumCpuReg fastcall_reg_by_index[] = {
      GUM_REG_ECX,
      GUM_REG_EDX,
    };

    if (n < G_N_ELEMENTS (fastcall_reg_by_index))
      return fastcall_reg_by_index[n];
  }

  return GUM_REG_NONE;
}

static guint8 *
gum_x86_writer_lookup_address_for_label_id (GumX86Writer * self,
                                            gconstpointer id)
{
  guint i;

  for (i = 0; i < self->id_to_address_len; i++)
  {
    GumLabelMapping * map = &self->id_to_address[i];
    if (map->id == id)
      return map->address;
  }

  return NULL;
}

static void
gum_x86_writer_add_address_for_label_id (GumX86Writer * self,
                                         gconstpointer id,
                                         gpointer address)
{
  GumLabelMapping * map = &self->id_to_address[self->id_to_address_len++];

  g_assert_cmpuint (self->id_to_address_len, <=, GUM_MAX_LABEL_COUNT);

  map->id = id;
  map->address = address;
}

void
gum_x86_writer_put_label (GumX86Writer * self,
                          gconstpointer id)
{
  g_assert (gum_x86_writer_lookup_address_for_label_id (self, id) == NULL);
  gum_x86_writer_add_address_for_label_id (self, id, self->code);
}

static void
gum_x86_writer_add_label_reference_here (GumX86Writer * self,
                                         gconstpointer id,
                                         GumLabelRefSize size)
{
  GumLabelRef * r = &self->label_refs[self->label_refs_len++];

  g_assert_cmpuint (self->label_refs_len, <=, GUM_MAX_LREF_COUNT);

  r->id = id;
  r->address = self->code;
  r->size = size;
}

static void
gum_x86_writer_put_argument_list_setup (GumX86Writer * self,
                                        GumCallingConvention conv,
                                        guint n_args,
                                        va_list vl)
{
  GumArgument args[4];
  gint arg_index;

  g_return_if_fail (n_args <= 4);

  (void) conv;

  for (arg_index = 0; arg_index != (gint) n_args; arg_index++)
  {
    GumArgument * arg = &args[arg_index];

    arg->type = va_arg (vl, GumArgType);
    if (arg->type == GUM_ARG_POINTER)
      arg->value.pointer = va_arg (vl, gpointer);
    else if (arg->type == GUM_ARG_REGISTER)
      arg->value.reg = va_arg (vl, GumCpuReg);
    else
      g_assert_not_reached ();
  }

  if (self->target_cpu == GUM_CPU_IA32)
  {
    for (arg_index = n_args - 1; arg_index >= 0; arg_index--)
    {
      GumArgument * arg = &args[arg_index];

      if (arg->type == GUM_ARG_POINTER)
      {
        gum_x86_writer_put_push_u32 (self, GPOINTER_TO_SIZE (
            arg->value.pointer));
      }
      else
      {
        gum_x86_writer_put_push_reg (self, arg->value.reg);
      }
    }
  }
  else
  {
    static const GumCpuReg reg_for_arg_unix[4] = {
      GUM_REG_RDI,
      GUM_REG_RSI,
      GUM_REG_RDX,
      GUM_REG_RCX
    };
    static const GumCpuReg reg_for_arg_windows[4] = {
      GUM_REG_RCX,
      GUM_REG_RDX,
      GUM_REG_R8,
      GUM_REG_R9
    };
    const GumCpuReg * reg_for_arg;

    if (self->target_abi == GUM_ABI_UNIX)
      reg_for_arg = reg_for_arg_unix;
    else
      reg_for_arg = reg_for_arg_windows;

    for (arg_index = n_args - 1; arg_index >= 0; arg_index--)
    {
      GumArgument * arg = &args[arg_index];

      if (arg->type == GUM_ARG_POINTER)
      {
        gum_x86_writer_put_mov_reg_u64 (self, reg_for_arg[arg_index],
            GPOINTER_TO_SIZE (arg->value.pointer));
      }
      else if (gum_meta_reg_from_cpu_reg (arg->value.reg) !=
          gum_meta_reg_from_cpu_reg (reg_for_arg[arg_index]))
      {
        gum_x86_writer_put_mov_reg_reg (self, reg_for_arg[arg_index],
            arg->value.reg);
      }
    }

    gum_x86_writer_put_sub_reg_imm (self, GUM_REG_RSP, 32);
  }
}

static void
gum_x86_writer_put_argument_list_teardown (GumX86Writer * self,
                                           GumCallingConvention conv,
                                           guint n_args)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    if (conv == GUM_CALL_CAPI && n_args != 0)
    {
      gum_x86_writer_put_add_reg_imm (self, GUM_REG_ESP,
          n_args * sizeof (guint32));
    }
  }
  else
  {
    gum_x86_writer_put_add_reg_imm (self, GUM_REG_RSP, 32);
  }
}

void
gum_x86_writer_put_call_with_arguments (GumX86Writer * self,
                                        gpointer func,
                                        guint n_args,
                                        ...)
{
  GumCallingConvention conv = GUM_CALL_CAPI;
  va_list vl;

  va_start (vl, n_args);
  gum_x86_writer_put_argument_list_setup (self, conv, n_args, vl);
  va_end (vl);

  gum_x86_writer_put_call (self, func);

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);
}

void
gum_x86_writer_put_call_reg_with_arguments (GumX86Writer * self,
                                            GumCallingConvention conv,
                                            GumCpuReg reg,
                                            guint n_args,
                                            ...)
{
  va_list vl;

  va_start (vl, n_args);
  gum_x86_writer_put_argument_list_setup (self, conv, n_args, vl);
  va_end (vl);

  gum_x86_writer_put_call_reg (self, reg);

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);
}

void
gum_x86_writer_put_call_reg_offset_ptr_with_arguments (GumX86Writer * self,
                                                       GumCallingConvention conv,
                                                       GumCpuReg reg,
                                                       gssize offset,
                                                       guint n_args,
                                                       ...)
{
  va_list vl;

  va_start (vl, n_args);
  gum_x86_writer_put_argument_list_setup (self, conv, n_args, vl);
  va_end (vl);

  gum_x86_writer_put_call_reg_offset_ptr (self, reg, offset);

  gum_x86_writer_put_argument_list_teardown (self, conv, n_args);
}

void
gum_x86_writer_put_call (GumX86Writer * self,
                         gconstpointer target)
{
  gint64 distance;
  gboolean distance_fits_in_i32;

  distance = (gssize) target - (gssize) (self->code + 5);
  distance_fits_in_i32 = (distance >= G_MININT32 && distance <= G_MAXINT32);

  if (distance_fits_in_i32)
  {
    self->code[0] = 0xe8;
    *((gint32 *) (self->code + 1)) = distance;
    self->code += 5;
  }
  else
  {
    g_assert (self->target_cpu == GUM_CPU_AMD64);

    gum_x86_writer_put_mov_reg_u64 (self, GUM_REG_RAX,
        GPOINTER_TO_SIZE (target));
    gum_x86_writer_put_call_reg (self, GUM_REG_RAX);
  }
}

void
gum_x86_writer_put_call_reg (GumX86Writer * self,
                             GumCpuReg reg)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (ri.width == 32 && !ri.index_is_extended);
  else
    g_return_if_fail (ri.width == 64);

  if (ri.index_is_extended)
    *self->code++ = 0x41;
  self->code[0] = 0xff;
  self->code[1] = 0xd0 | ri.index;
  self->code += 2;
}

void
gum_x86_writer_put_call_reg_offset_ptr (GumX86Writer * self,
                                        GumCpuReg reg,
                                        gssize offset)
{
  GumCpuRegInfo ri;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  offset_fits_in_i8 = IS_WITHIN_INT8_RANGE (offset);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (ri.width == 32 && !ri.index_is_extended);
  else
    g_return_if_fail (ri.width == 64);

  if (ri.index_is_extended)
    *self->code++ = 0x41;
  self->code[0] = 0xff;
  self->code[1] = (offset_fits_in_i8 ? 0x50 : 0x90) | ri.index;
  self->code += 2;
  if (ri.index_is_extended || ri.meta == GUM_META_REG_XSP)
    *self->code++ = 0x24;

  if (offset_fits_in_i8)
  {
    *((gint8 *) self->code) = offset;
    self->code++;
  }
  else
  {
    *((gint32 *) self->code) = offset;
    self->code += 4;
  }
}

void
gum_x86_writer_put_call_indirect (GumX86Writer * self,
                                  gconstpointer * addr)
{
  self->code[0] = 0xff;
  self->code[1] = 0x15;
  *((gconstpointer **) (self->code + 2)) = addr;
  self->code += 6;
}

void
gum_x86_writer_put_call_near_label (GumX86Writer * self,
                                    gconstpointer label_id)
{
  gum_x86_writer_put_call (self, self->code);
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_NEAR);
}

void
gum_x86_writer_put_ret (GumX86Writer * self)
{
  self->code[0] = 0xc3;
  self->code++;
}

void
gum_x86_writer_put_jmp (GumX86Writer * self,
                        gconstpointer target)
{
  gint64 distance;

  distance = (gssize) target - (gssize) (self->code + 2);

  if (IS_WITHIN_INT8_RANGE (distance))
  {
    self->code[0] = 0xeb;
    *((gint8 *) (self->code + 1)) = distance;
    self->code += 2;
  }
  else
  {
    distance = (gssize) target - (gssize) (self->code + 5);

    if (IS_WITHIN_INT32_RANGE (distance))
    {
      self->code[0] = 0xe9;
      *((gint32 *) (self->code + 1)) = distance;
      self->code += 5;
    }
    else
    {
      g_assert_cmpint (self->target_cpu, ==, GUM_CPU_AMD64);

      self->code[0] = 0xff;
      self->code[1] = 0x25;
      *((gint32 *) (self->code + 2)) = 0; /* rip + 0 */
      *((gconstpointer *) (self->code + 6)) = target;
      self->code += 14;
    }
  }
}

void
gum_x86_writer_put_jmp_short_label (GumX86Writer * self,
                                    gconstpointer label_id)
{
  gum_x86_writer_put_jmp (self, self->code);
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_SHORT);
}

void
gum_x86_writer_put_jcc_short_label (GumX86Writer * self,
                                    guint8 opcode,
                                    gconstpointer label_id)
{
  self->code[0] = opcode;
  *((gint8 *) (self->code + 1)) = (gint8) -2;
  self->code += 2;

  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_SHORT);
}

void
gum_x86_writer_put_jcc_near (GumX86Writer * self,
                             guint8 opcode,
                             gconstpointer target)
{
  gint64 distance;

  distance = (gssize) target - (gssize) (self->code + 6);
  g_assert (IS_WITHIN_INT32_RANGE (distance));

  self->code[0] = 0x0f;
  self->code[1] = opcode;
  *((gint32 *) (self->code + 2)) = distance;
  self->code += 6;
}

void
gum_x86_writer_put_jmp_reg (GumX86Writer * self,
                            GumCpuReg reg)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (ri.width == 32 && !ri.index_is_extended);
  else
    g_return_if_fail (ri.width == 64);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL);

  self->code[0] = 0xff;
  self->code[1] = 0xe0 | ri.index;
  self->code += 2;
}

void
gum_x86_writer_put_jmp_reg_ptr (GumX86Writer * self,
                                GumCpuReg reg)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (ri.width == 32 && !ri.index_is_extended);
  else
    g_return_if_fail (ri.width == 64);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL);

  self->code[0] = 0xff;
  self->code[1] = 0x20 | ri.index;
  self->code += 2;

  if (ri.meta == GUM_META_REG_XSP)
    *self->code++ = 0x24;
}

void
gum_x86_writer_put_jz (GumX86Writer * self,
                       gconstpointer target,
                       GumBranchHint hint)
{
  gint32 distance;

  distance = GPOINTER_TO_SIZE (target) - GPOINTER_TO_SIZE (self->code + 3);

  g_assert (IS_WITHIN_INT8_RANGE (distance)); /* for now */

  if (hint != GUM_NO_HINT)
    *self->code++ = (hint == GUM_LIKELY) ? 0x3e : 0x2e;
  self->code[0] = 0x74;
  self->code[1] = distance;
  self->code += 2;
}

void
gum_x86_writer_put_jz_label (GumX86Writer * self,
                             gconstpointer label_id,
                             GumBranchHint hint)
{
  gum_x86_writer_put_jz (self, self->code, hint);
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_SHORT);
}

void
gum_x86_writer_put_jle (GumX86Writer * self,
                        gconstpointer target,
                        GumBranchHint hint)
{
  gint32 distance;

  distance = GPOINTER_TO_SIZE (target) - GPOINTER_TO_SIZE (self->code + 3);

  g_assert (IS_WITHIN_INT8_RANGE (distance)); /* for now */

  self->code[0] = (hint == GUM_LIKELY) ? 0x3e : 0x2e;
  self->code[1] = 0x7e;
  self->code[2] = distance;
  self->code += 3;
}

void
gum_x86_writer_put_jle_label (GumX86Writer * self,
                              gconstpointer label_id,
                              GumBranchHint hint)
{
  gum_x86_writer_put_jle (self, self->code, hint);
  gum_x86_writer_add_label_reference_here (self, label_id, GUM_LREF_SHORT);
}

static void
gum_x86_writer_put_add_or_sub_reg_imm (GumX86Writer * self,
                                       GumCpuReg reg,
                                       gssize imm_value,
                                       gboolean add)
{
  GumCpuRegInfo ri;
  gboolean immediate_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  immediate_fits_in_i8 = IS_WITHIN_INT8_RANGE (imm_value);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL);

  if (ri.meta == GUM_META_REG_XAX && !immediate_fits_in_i8)
  {
    *self->code++ = add ? 0x05 : 0x2d;
  }
  else
  {
    self->code[0] = (immediate_fits_in_i8 ? 0x83 : 0x81);
    self->code[1] = (add ? 0xc0 : 0xe8) | ri.index;
    self->code += 2;
  }

  if (immediate_fits_in_i8)
  {
    *((gint8 *) self->code) = imm_value;
    self->code++;
  }
  else
  {
    *((gint32 *) self->code) = imm_value;
    self->code += 4;
  }
}

void
gum_x86_writer_put_add_reg_imm (GumX86Writer * self,
                                GumCpuReg reg,
                                gssize imm_value)
{
  gum_x86_writer_put_add_or_sub_reg_imm (self, reg, imm_value, TRUE);
}

void
gum_x86_writer_put_add_reg_reg (GumX86Writer * self,
                                GumCpuReg dst_reg,
                                GumCpuReg src_reg)
{
  GumCpuRegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  g_return_if_fail (src.width == dst.width);

  gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, &src, NULL);

  self->code[0] = 0x01;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  self->code += 2;
}

void
gum_x86_writer_put_sub_reg_imm (GumX86Writer * self,
                                GumCpuReg reg,
                                gssize imm_value)
{
  gum_x86_writer_put_add_or_sub_reg_imm (self, reg, imm_value, FALSE);
}

void
gum_x86_writer_put_sub_reg_reg (GumX86Writer * self,
                                GumCpuReg dst_reg,
                                GumCpuReg src_reg)
{
  self->code[0] = 0x29;
  self->code[1] = 0xc0 | (src_reg << 3) | dst_reg;
  self->code += 2;
}

void
gum_x86_writer_put_inc_reg (GumX86Writer * self,
                            GumCpuReg reg)
{
  self->code[0] = 0xff;
  self->code[1] = 0xc0 | reg;
  self->code += 2;
}

void
gum_x86_writer_put_dec_reg (GumX86Writer * self,
                            GumCpuReg reg)
{
  self->code[0] = 0xff;
  self->code[1] = 0xc8 | reg;
  self->code += 2;
}

static void
gum_x86_writer_put_inc_or_dec_reg_ptr (GumX86Writer * self,
                                       GumPtrTarget target,
                                       GumCpuReg reg,
                                       gboolean increment)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_AMD64)
  {
    if (target == GUM_PTR_QWORD)
      *self->code++ = 0x48 | (ri.index_is_extended) ? 0x01 : 0x00;
    else if (ri.index_is_extended)
      *self->code++ = 0x41;
  }

  switch (target)
  {
    case GUM_PTR_BYTE:
      *self->code++ = 0xfe;
      break;
    case GUM_PTR_QWORD:
      g_return_if_fail (self->target_cpu == GUM_CPU_AMD64);
    case GUM_PTR_DWORD:
      *self->code++ = 0xff;
      break;
  }

  *self->code++ = ((increment) ? 0x00 : 0x08) | ri.index;
}

void
gum_x86_writer_put_inc_reg_ptr (GumX86Writer * self,
                                GumPtrTarget target,
                                GumCpuReg reg)
{
  gum_x86_writer_put_inc_or_dec_reg_ptr (self, target, reg, TRUE);
}

void
gum_x86_writer_put_dec_reg_ptr (GumX86Writer * self,
                                GumPtrTarget target,
                                GumCpuReg reg)
{
  gum_x86_writer_put_inc_or_dec_reg_ptr (self, target, reg, FALSE);
}

void
gum_x86_writer_put_lock_xadd_reg_ptr_reg (GumX86Writer * self,
                                          GumCpuReg dst_reg,
                                          GumCpuReg src_reg)
{
  GumCpuRegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  *self->code++ = 0xf0; /* lock prefix */

  gum_x86_writer_put_prefix_for_registers (self, &src, 32, &dst, &src, NULL);

  self->code[0] = 0x0f;
  self->code[1] = 0xc1;
  self->code[2] = 0x00 | (src.index << 3) | dst.index;
  self->code += 3;

  if (dst.meta == GUM_META_REG_XSP)
  {
    *self->code++ = 0x24;
  }
  else if (dst.meta == GUM_META_REG_XBP)
  {
    self->code[-1] |= 0x40;
    *self->code++ = 0x00;
  }
}

void
gum_x86_writer_put_lock_cmpxchg_reg_ptr_reg (GumX86Writer * self,
                                             GumCpuReg dst_reg,
                                             GumCpuReg src_reg)
{
  GumCpuRegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (dst.width == 32);
  else
    g_return_if_fail (dst.width == 64);
  g_return_if_fail (!dst.index_is_extended);
  g_return_if_fail (src.width == 32 && !src.index_is_extended);

  self->code[0] = 0xf0; /* lock prefix */
  self->code[1] = 0x0f;
  self->code[2] = 0xb1;
  self->code[3] = 0x00 | (src.index << 3) | dst.index;
  self->code += 4;

  if (dst.meta == GUM_META_REG_XSP)
  {
    *self->code++ = 0x24;
  }
  else if (dst.meta == GUM_META_REG_XBP)
  {
    self->code[-1] |= 0x40;
    *self->code++ = 0x00;
  }
}

static void
gum_x86_writer_put_lock_inc_or_dec_imm32_ptr (GumX86Writer * self,
                                              gpointer target,
                                              gboolean increment)
{
  self->code[0] = 0xf0;
  self->code[1] = 0xff;
  self->code[2] = increment ? 0x05 : 0x0d;

  if (self->target_cpu == GUM_CPU_IA32)
  {
    *((guint32 *) (self->code + 3)) = GPOINTER_TO_SIZE (target);
  }
  else
  {
    gint64 distance = (gssize) target - (gssize) (self->code + 7);
    g_assert (IS_WITHIN_INT32_RANGE (distance));
    *((gint32 *) (self->code + 3)) = distance;
  }

  self->code += 7;
}

void
gum_x86_writer_put_lock_inc_imm32_ptr (GumX86Writer * self,
                                       gpointer target)
{
  gum_x86_writer_put_lock_inc_or_dec_imm32_ptr (self, target, TRUE);
}

void
gum_x86_writer_put_lock_dec_imm32_ptr (GumX86Writer * self,
                                       gpointer target)
{
  gum_x86_writer_put_lock_inc_or_dec_imm32_ptr (self, target, FALSE);
}

void
gum_x86_writer_put_and_reg_u32 (GumX86Writer * self,
                                GumCpuReg reg,
                                guint32 imm_value)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL);

  if (ri.meta == GUM_META_REG_XAX)
  {
    self->code[0] = 0x25;
    *((guint32 *) (self->code + 1)) = imm_value;
    self->code += 5;
  }
  else
  {
    self->code[0] = 0x81;
    self->code[1] = 0xe0 | ri.index;
    *((guint32 *) (self->code + 2)) = imm_value;
    self->code += 6;
  }
}

void
gum_x86_writer_put_shl_reg_u8 (GumX86Writer * self,
                               GumCpuReg reg,
                               guint8 imm_value)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL);

  self->code[0] = 0xc1;
  self->code[1] = 0xe0 | ri.index;
  self->code[2] = imm_value;
  self->code += 3;
}

void
gum_x86_writer_put_xor_reg_reg (GumX86Writer * self,
                                GumCpuReg dst_reg,
                                GumCpuReg src_reg)
{
  GumCpuRegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  g_return_if_fail (dst.width == src.width);
  g_return_if_fail (!dst.index_is_extended && !src.index_is_extended);

  gum_x86_writer_put_prefix_for_reg_info (self, &dst, 0);

  self->code[0] = 0x31;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  self->code += 2;
}

void
gum_x86_writer_put_mov_reg_reg (GumX86Writer * self,
                                GumCpuReg dst_reg,
                                GumCpuReg src_reg)
{
  GumCpuRegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  g_return_if_fail (dst.width == src.width);

  gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, &src, NULL);

  self->code[0] = 0x89;
  self->code[1] = 0xc0 | (src.index << 3) | dst.index;
  self->code += 2;
}

void
gum_x86_writer_put_mov_reg_u32 (GumX86Writer * self,
                                GumCpuReg dst_reg,
                                guint32 imm_value)
{
  GumCpuRegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  g_return_if_fail (dst.width == 32);

  gum_x86_writer_put_prefix_for_reg_info (self, &dst, 0);

  self->code[0] = 0xb8 | dst.index;
  *((guint32 *) (self->code + 1)) = imm_value;
  self->code += 5;
}

void
gum_x86_writer_put_mov_reg_u64 (GumX86Writer * self,
                                GumCpuReg dst_reg,
                                guint64 imm_value)
{
  GumCpuRegInfo dst;

  g_return_if_fail (self->target_cpu == GUM_CPU_AMD64);

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  g_return_if_fail (dst.width == 64);

  gum_x86_writer_put_prefix_for_reg_info (self, &dst, 0);

  self->code[0] = 0xb8 | dst.index;
  *((guint64 *) (self->code + 1)) = imm_value;
  self->code += 9;
}

void
gum_x86_writer_put_mov_reg_address (GumX86Writer * self,
                                    GumCpuReg dst_reg,
                                    GumAddress address)
{
  GumCpuRegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (dst.width == 32)
    gum_x86_writer_put_mov_reg_u32 (self, dst_reg, (guint32) address);
  else
    gum_x86_writer_put_mov_reg_u64 (self, dst_reg, (guint64) address);
}

void
gum_x86_writer_put_mov_reg_ptr_u32 (GumX86Writer * self,
                                    GumCpuReg dst_reg,
                                    guint32 imm_value)
{
  gum_x86_writer_put_mov_reg_offset_ptr_u32 (self, dst_reg, 0, imm_value);
}

void
gum_x86_writer_put_mov_reg_offset_ptr_u32 (GumX86Writer * self,
                                           GumCpuReg dst_reg,
                                           gssize dst_offset,
                                           guint32 imm_value)
{
  GumCpuRegInfo dst;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (dst.width == 32);
  else
    g_return_if_fail (dst.width == 64);

  offset_fits_in_i8 = IS_WITHIN_INT8_RANGE (dst_offset);

  *self->code++ = 0xc7;

  if (dst_offset == 0 && dst.meta != GUM_META_REG_XBP)
  {
    *self->code++ = 0x00 | dst.index;
    if (dst.meta == GUM_META_REG_XSP)
      *self->code++ = 0x24;
  }
  else
  {
    *self->code++ = ((offset_fits_in_i8) ? 0x40 : 0x80) | dst.index;

    if (dst.meta == GUM_META_REG_XSP)
      *self->code++ = 0x24;

    if (offset_fits_in_i8)
    {
      *self->code++ = dst_offset;
    }
    else
    {
      *((gint32 *) self->code) = dst_offset;
      self->code += 4;
    }
  }

  *((guint32 *) self->code) = imm_value;
  self->code += 4;
}

void
gum_x86_writer_put_mov_reg_ptr_reg (GumX86Writer * self,
                                    GumCpuReg dst_reg,
                                    GumCpuReg src_reg)
{
  gum_x86_writer_put_mov_reg_offset_ptr_reg (self, dst_reg, 0, src_reg);
}

void
gum_x86_writer_put_mov_reg_offset_ptr_reg (GumX86Writer * self,
                                           GumCpuReg dst_reg,
                                           gssize dst_offset,
                                           GumCpuReg src_reg)
{
  GumCpuRegInfo dst, src;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (dst.width == 32 && src.width == 32);
  else
    g_return_if_fail (dst.width == 64);

  offset_fits_in_i8 = IS_WITHIN_INT8_RANGE (dst_offset);

  gum_x86_writer_put_prefix_for_reg_info (self, &src, 1);

  *self->code++ = 0x89;

  if (dst_offset == 0 && dst.meta != GUM_META_REG_XBP)
  {
    *self->code++ = 0x00 | (src.index << 3) | dst.index;
    if (dst.meta == GUM_META_REG_XSP)
      *self->code++ = 0x24;
  }
  else
  {
    *self->code++ = ((offset_fits_in_i8) ? 0x40 : 0x80) |
        (src.index << 3) | dst.index;

    if (dst.meta == GUM_META_REG_XSP)
      *self->code++ = 0x24;

    if (offset_fits_in_i8)
    {
      *((gint8 *) self->code) = dst_offset;
      self->code++;
    }
    else
    {
      *((gint32 *) self->code) = dst_offset;
      self->code += 4;
    }
  }
}

void
gum_x86_writer_put_mov_reg_reg_ptr (GumX86Writer * self,
                                    GumCpuReg dst_reg,
                                    GumCpuReg src_reg)
{
  gum_x86_writer_put_mov_reg_reg_offset_ptr (self, dst_reg, src_reg, 0);
}

void
gum_x86_writer_put_mov_reg_reg_offset_ptr (GumX86Writer * self,
                                           GumCpuReg dst_reg,
                                           GumCpuReg src_reg,
                                           gssize src_offset)
{
  GumCpuRegInfo dst, src;
  gboolean offset_fits_in_i8;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (dst.width == 32 && src.width == 32);
  else
    g_return_if_fail (src.width == 64);

  offset_fits_in_i8 = IS_WITHIN_INT8_RANGE (src_offset);

  gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &src, &dst, NULL);

  self->code[0] = 0x8b;
  self->code[1] = ((offset_fits_in_i8) ? 0x40 : 0x80)
      | (dst.index << 3) | src.index;
  self->code += 2;

  if (src.meta == GUM_META_REG_XSP)
    *self->code++ = 0x24;

  if (offset_fits_in_i8)
  {
    *((gint8 *) self->code) = src_offset;
    self->code++;
  }
  else
  {
    *((gint32 *) self->code) = src_offset;
    self->code += 4;
  }
}

void
gum_x86_writer_put_mov_reg_base_index_scale_offset_ptr (GumX86Writer * self,
                                                        GumCpuReg dst_reg,
                                                        GumCpuReg base_reg,
                                                        GumCpuReg index_reg,
                                                        guint8 scale,
                                                        gssize offset)
{
  GumCpuRegInfo dst, base, index;
  gboolean offset_fits_in_i8;
  const guint8 scale_lookup[] = {
      /* 0: */ 0xff,
      /* 1: */    0,
      /* 2: */    1,
      /* 3: */ 0xff,
      /* 4: */    2,
      /* 5: */ 0xff,
      /* 6: */ 0xff,
      /* 7: */ 0xff,
      /* 8: */    3
  };

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, base_reg, &base);
  gum_x86_writer_describe_cpu_reg (self, index_reg, &index);

  g_return_if_fail (!dst.index_is_extended);
  g_return_if_fail (base.width == index.width);
  g_return_if_fail (!base.index_is_extended && !index.index_is_extended);
  g_return_if_fail (index.meta != GUM_META_REG_XSP);
  g_return_if_fail (scale == 1 || scale == 2 || scale == 4 || scale == 8);

  offset_fits_in_i8 = IS_WITHIN_INT8_RANGE (offset);

  if (self->target_cpu == GUM_CPU_AMD64)
  {
    g_return_if_fail (dst.width == 64);
    g_return_if_fail (base.width == 64 && index.width == 64);

    *self->code++ = 0x48;
  }

  self->code[0] = 0x8b;
  self->code[1] = (offset_fits_in_i8 ? 0x40 : 0x80) | (dst.index << 3) | 0x04;
  self->code[2] = (scale_lookup[scale] << 6) | (index.index << 3) | base.index;
  self->code += 3;

  if (offset_fits_in_i8)
  {
    *((gint8 *) self->code) = offset;
    self->code++;
  }
  else
  {
    *((gint32 *) self->code) = offset;
    self->code += 4;
  }
}

static void
gum_x86_writer_put_mov_reg_imm_ptr (GumX86Writer * self,
                                    GumCpuReg dst_reg,
                                    guint32 address)
{
  GumCpuRegInfo dst;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);

  gum_x86_writer_put_prefix_for_registers (self, &dst, 32, &dst, NULL);

  self->code[0] = 0x8b;
  self->code[1] = (dst.index << 3) | 0x04;
  self->code[2] = 0x25;
  *((guint32 *) (self->code + 3)) = address;
  self->code += 7;
}

static void
gum_x86_writer_put_mov_imm_ptr_reg (GumX86Writer * self,
                                    guint32 address,
                                    GumCpuReg src_reg)
{
  GumCpuRegInfo src;

  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  gum_x86_writer_put_prefix_for_registers (self, &src, 32, &src, NULL);

  self->code[0] = 0x89;
  self->code[1] = (src.index << 3) | 0x04;
  self->code[2] = 0x25;
  *((guint32 *) (self->code + 3)) = address;
  self->code += 7;
}

void
gum_x86_writer_put_mov_fs_u32_ptr_reg (GumX86Writer * self,
                                       guint32 fs_offset,
                                       GumCpuReg src_reg)
{
  gum_x86_writer_put_byte (self, 0x64);
  gum_x86_writer_put_mov_imm_ptr_reg (self, fs_offset, src_reg);
}

void
gum_x86_writer_put_mov_reg_fs_u32_ptr (GumX86Writer * self,
                                       GumCpuReg dst_reg,
                                       guint32 fs_offset)
{
  gum_x86_writer_put_byte (self, 0x64);
  gum_x86_writer_put_mov_reg_imm_ptr (self, dst_reg, fs_offset);
}

void
gum_x86_writer_put_mov_gs_u32_ptr_reg (GumX86Writer * self,
                                       guint32 fs_offset,
                                       GumCpuReg src_reg)
{
  gum_x86_writer_put_byte (self, 0x65);
  gum_x86_writer_put_mov_imm_ptr_reg (self, fs_offset, src_reg);
}

void
gum_x86_writer_put_mov_reg_gs_u32_ptr (GumX86Writer * self,
                                       GumCpuReg dst_reg,
                                       guint32 fs_offset)
{
  gum_x86_writer_put_byte (self, 0x65);
  gum_x86_writer_put_mov_reg_imm_ptr (self, dst_reg, fs_offset);
}

void
gum_x86_writer_put_movq_xmm0_esp_offset_ptr (GumX86Writer * self,
                                             gint8 offset)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x0f;
  self->code[2] = 0x7e;
  self->code[3] = 0x44;
  self->code[4] = 0x24;
  self->code[5] = offset;
  self->code += 6;
}

void
gum_x86_writer_put_movq_eax_offset_ptr_xmm0 (GumX86Writer * self,
                                             gint8 offset)
{
  self->code[0] = 0x66;
  self->code[1] = 0x0f;
  self->code[2] = 0xd6;
  self->code[3] = 0x40;
  self->code[4] = offset;
  self->code += 5;
}

void
gum_x86_writer_put_movdqu_xmm0_esp_offset_ptr (GumX86Writer * self,
                                               gint8 offset)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x0f;
  self->code[2] = 0x6f;
  self->code[3] = 0x44;
  self->code[4] = 0x24;
  self->code[5] = offset;
  self->code += 6;
}

void
gum_x86_writer_put_movdqu_eax_offset_ptr_xmm0 (GumX86Writer * self,
                                               gint8 offset)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x0f;
  self->code[2] = 0x7f;
  self->code[3] = 0x40;
  self->code[4] = offset;
  self->code += 5;
}

void
gum_x86_writer_put_lea_reg_reg_offset (GumX86Writer * self,
                                       GumCpuReg dst_reg,
                                       GumCpuReg src_reg,
                                       gssize src_offset)
{
  GumCpuRegInfo dst, src;

  gum_x86_writer_describe_cpu_reg (self, dst_reg, &dst);
  gum_x86_writer_describe_cpu_reg (self, src_reg, &src);

  g_return_if_fail (!dst.index_is_extended && !src.index_is_extended);

  if (self->target_cpu == GUM_CPU_AMD64)
  {
    if (src.width == 32)
      *self->code++ = 0x67;
    if (dst.width == 64)
      *self->code++ = 0x48;
  }

  self->code[0] = 0x8d;
  self->code[1] = 0x80 | (dst.index << 3) | src.index;
  self->code += 2;

  if (src.meta == GUM_META_REG_XSP)
    *self->code++ = 0x24;

  *((gint32 *) self->code) = src_offset;
  self->code += 4;
}

void
gum_x86_writer_put_xchg_reg_reg_ptr (GumX86Writer * self,
                                     GumCpuReg left_reg,
                                     GumCpuReg right_reg)
{
  GumCpuRegInfo left, right;

  gum_x86_writer_describe_cpu_reg (self, left_reg, &left);
  gum_x86_writer_describe_cpu_reg (self, right_reg, &right);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (right.width == 32);
  else
    g_return_if_fail (right.width == 64);

  gum_x86_writer_put_prefix_for_reg_info (self, &left, 1);

  self->code[0] = 0x87;
  self->code[1] = 0x00 | (left.index << 3) | right.index;
  self->code += 2;

  if (right.meta == GUM_META_REG_XSP)
  {
    *self->code++ = 0x24;
  }
  else if (right.meta == GUM_META_REG_XBP)
  {
    self->code[-1] |= 0x40;
    *self->code++ = 0x00;
  }
}

void
gum_x86_writer_put_push_u32 (GumX86Writer * self,
                             guint32 imm_value)
{
  self->code[0] = 0x68;
  *((guint32 *) (self->code + 1)) = imm_value;
  self->code += 5;
}

void
gum_x86_writer_put_push_reg (GumX86Writer * self,
                             GumCpuReg reg)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (ri.width == 32);
  else
    g_return_if_fail (ri.width == 64);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL);

  *self->code++ = 0x50 | ri.index;
}

void
gum_x86_writer_put_pop_reg (GumX86Writer * self,
                            GumCpuReg reg)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  if (self->target_cpu == GUM_CPU_IA32)
    g_return_if_fail (ri.width == 32);
  else
    g_return_if_fail (ri.width == 64);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 64, &ri, NULL);

  *self->code++ = 0x58 | ri.index;
}

void
gum_x86_writer_put_push_imm_ptr (GumX86Writer * self,
                                 gconstpointer imm_ptr)
{
  self->code[0] = 0xff;
  self->code[1] = 0x35;
  *((gconstpointer *) (self->code + 2)) = imm_ptr;
  self->code += 6;
}

void
gum_x86_writer_put_pushax (GumX86Writer * self)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    self->code[0] = 0x60;
    self->code++;
  }
  else
  {
    gum_x86_writer_put_push_reg (self, GUM_REG_RAX);
    gum_x86_writer_put_push_reg (self, GUM_REG_RCX);
    gum_x86_writer_put_push_reg (self, GUM_REG_RDX);
    gum_x86_writer_put_push_reg (self, GUM_REG_RBX);

    gum_x86_writer_put_lea_reg_reg_offset (self, GUM_REG_RAX,
        GUM_REG_RSP, 4 * 8);
    gum_x86_writer_put_push_reg (self, GUM_REG_RAX);
    gum_x86_writer_put_mov_reg_reg_offset_ptr (self, GUM_REG_RAX,
        GUM_REG_RSP, 4 * 8);

    gum_x86_writer_put_push_reg (self, GUM_REG_RBP);
    gum_x86_writer_put_push_reg (self, GUM_REG_RSI);
    gum_x86_writer_put_push_reg (self, GUM_REG_RDI);

    gum_x86_writer_put_push_reg (self, GUM_REG_R8);
    gum_x86_writer_put_push_reg (self, GUM_REG_R9);
    gum_x86_writer_put_push_reg (self, GUM_REG_R10);
    gum_x86_writer_put_push_reg (self, GUM_REG_R11);
    gum_x86_writer_put_push_reg (self, GUM_REG_R12);
    gum_x86_writer_put_push_reg (self, GUM_REG_R13);
    gum_x86_writer_put_push_reg (self, GUM_REG_R14);
    gum_x86_writer_put_push_reg (self, GUM_REG_R15);
  }
}

void
gum_x86_writer_put_popax (GumX86Writer * self)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    self->code[0] = 0x61;
    self->code++;
  }
  else
  {
    gum_x86_writer_put_pop_reg (self, GUM_REG_R15);
    gum_x86_writer_put_pop_reg (self, GUM_REG_R14);
    gum_x86_writer_put_pop_reg (self, GUM_REG_R13);
    gum_x86_writer_put_pop_reg (self, GUM_REG_R12);
    gum_x86_writer_put_pop_reg (self, GUM_REG_R11);
    gum_x86_writer_put_pop_reg (self, GUM_REG_R10);
    gum_x86_writer_put_pop_reg (self, GUM_REG_R9);
    gum_x86_writer_put_pop_reg (self, GUM_REG_R8);

    gum_x86_writer_put_pop_reg (self, GUM_REG_RDI);
    gum_x86_writer_put_pop_reg (self, GUM_REG_RSI);
    gum_x86_writer_put_pop_reg (self, GUM_REG_RBP);
    gum_x86_writer_put_lea_reg_reg_offset (self, GUM_REG_RSP, GUM_REG_RSP, 8);
    gum_x86_writer_put_pop_reg (self, GUM_REG_RBX);
    gum_x86_writer_put_pop_reg (self, GUM_REG_RDX);
    gum_x86_writer_put_pop_reg (self, GUM_REG_RCX);
    gum_x86_writer_put_pop_reg (self, GUM_REG_RAX);
  }
}

void
gum_x86_writer_put_pushfx (GumX86Writer * self)
{
  self->code[0] = 0x9c;
  self->code++;
}

void
gum_x86_writer_put_popfx (GumX86Writer * self)
{
  self->code[0] = 0x9d;
  self->code++;
}

void
gum_x86_writer_put_test_reg_reg (GumX86Writer * self,
                                 GumCpuReg reg_a,
                                 GumCpuReg reg_b)
{
  GumCpuRegInfo a, b;

  gum_x86_writer_describe_cpu_reg (self, reg_a, &a);
  gum_x86_writer_describe_cpu_reg (self, reg_b, &b);

  g_return_if_fail (a.width == b.width);

  gum_x86_writer_put_prefix_for_registers (self, &a, 32, &a, &b, NULL);

  self->code[0] = 0x85;
  self->code[1] = 0xc0 | (b.index << 3) | a.index;
  self->code += 2;
}

void
gum_x86_writer_put_cmp_reg_i32 (GumX86Writer * self,
                                GumCpuReg reg,
                                gint32 imm_value)
{
  GumCpuRegInfo ri;

  gum_x86_writer_describe_cpu_reg (self, reg, &ri);

  gum_x86_writer_put_prefix_for_registers (self, &ri, 32, &ri, NULL);

  if (ri.meta == GUM_META_REG_XAX)
  {
    *self->code++ = 0x3d;
  }
  else
  {
    self->code[0] = 0x81;
    self->code[1] = 0xf8 | ri.index;
    self->code += 2;
  }

  *((gint32 *) self->code) = imm_value;
  self->code += 4;
}

void
gum_x86_writer_put_cmp_imm_ptr_imm_u32 (GumX86Writer * self,
                                        gconstpointer imm_ptr,
                                        guint32 imm_value)
{
  self->code[0] = 0x81;
  self->code[1] = 0x3d;
  *((gconstpointer *) (self->code + 2)) = imm_ptr;
  *((guint32 *) (self->code + 6)) = imm_value;
  self->code += 10;
}

void
gum_x86_writer_put_clc (GumX86Writer * self)
{
  *self->code++ = 0xf8;
}

void
gum_x86_writer_put_stc (GumX86Writer * self)
{
  *self->code++ = 0xf9;
}

void
gum_x86_writer_put_cpuid (GumX86Writer * self)
{
  self->code[0] = 0x0f;
  self->code[1] = 0xa2;
  self->code += 2;
}

void
gum_x86_writer_put_lfence (GumX86Writer * self)
{
  self->code[0] = 0x0f;
  self->code[1] = 0xae;
  self->code[2] = 0xe8;
  self->code += 3;
}

void
gum_x86_writer_put_rdtsc (GumX86Writer * self)
{
  self->code[0] = 0x0f;
  self->code[1] = 0x31;
  self->code += 2;
}

void
gum_x86_writer_put_pause (GumX86Writer * self)
{
  self->code[0] = 0xf3;
  self->code[1] = 0x90;
  self->code += 2;
}

void
gum_x86_writer_put_nop (GumX86Writer * self)
{
  self->code[0] = 0x90;
  self->code++;
}

void
gum_x86_writer_put_int3 (GumX86Writer * self)
{
  self->code[0] = 0xcc;
  self->code++;
}

void
gum_x86_writer_put_byte (GumX86Writer * self,
                         guint8 b)
{
  self->code[0] = b;
  self->code++;
}

void
gum_x86_writer_put_bytes (GumX86Writer * self,
                          const guint8 * data,
                          guint n)
{
  memcpy (self->code, data, n);
  self->code += n;
}

static void
gum_x86_writer_describe_cpu_reg (GumX86Writer * self,
                                 GumCpuReg reg,
                                 GumCpuRegInfo * ri)
{
  if (reg >= GUM_REG_XAX && reg <= GUM_REG_XDI)
  {
    if (self->target_cpu == GUM_CPU_IA32)
      reg = (GumCpuReg) (GUM_REG_EAX + reg - GUM_REG_XAX);
    else
      reg = (GumCpuReg) (GUM_REG_RAX + reg - GUM_REG_XAX);
  }

  ri->meta = gum_meta_reg_from_cpu_reg (reg);

  if (reg >= GUM_REG_RAX && reg <= GUM_REG_R15)
  {
    ri->width = 64;

    if (reg < GUM_REG_R8)
    {
      ri->index = reg - GUM_REG_RAX;
      ri->index_is_extended = FALSE;
    }
    else
    {
      ri->index = reg - GUM_REG_R8;
      ri->index_is_extended = TRUE;
    }
  }
  else
  {
    ri->width = 32;

    if (reg < GUM_REG_R8D)
    {
      ri->index = reg - GUM_REG_EAX;
      ri->index_is_extended = FALSE;
    }
    else
    {
      ri->index = reg - GUM_REG_R8D;
      ri->index_is_extended = TRUE;
    }
  }
}

static GumMetaReg
gum_meta_reg_from_cpu_reg (GumCpuReg reg)
{
  if (reg >= GUM_REG_EAX && reg <= GUM_REG_R15D)
    return (GumMetaReg) (GUM_META_REG_XAX + reg - GUM_REG_EAX);
  else if (reg >= GUM_REG_RAX && reg <= GUM_REG_R15)
    return (GumMetaReg) (GUM_META_REG_XAX + reg - GUM_REG_RAX);
  else if (reg >= GUM_REG_XAX && reg <= GUM_REG_XDI)
    return (GumMetaReg) (GUM_META_REG_XAX + reg - GUM_REG_XAX);
  else
    g_assert_not_reached ();
}

static void
gum_x86_writer_put_prefix_for_reg_info (GumX86Writer * self,
                                        const GumCpuRegInfo * ri,
                                        guint operand_index)
{
  if (self->target_cpu == GUM_CPU_IA32)
  {
    g_return_if_fail (ri->width == 32 && !ri->index_is_extended);
  }
  else
  {
    guint mask;

    mask = 1 << (operand_index * 2);

    if (ri->width == 32)
    {
      if (ri->index_is_extended)
        *self->code++ = 0x40 | mask;
    }
    else
    {
      *self->code++ = (ri->index_is_extended) ? 0x48 | mask : 0x48;
    }
  }
}

/* TODO: improve this function and get rid of the one above */
static void
gum_x86_writer_put_prefix_for_registers (GumX86Writer * self,
                                         const GumCpuRegInfo * width_reg,
                                         guint default_width,
                                         ...)
{
  const GumCpuRegInfo * ra, * rb, * rc;
  va_list vl;

  va_start (vl, default_width);

  ra = va_arg (vl, const GumCpuRegInfo *);
  g_assert (ra != NULL);

  rb = va_arg (vl, const GumCpuRegInfo *);
  if (rb != NULL)
  {
    rc = va_arg (vl, const GumCpuRegInfo *);
  }
  else
  {
    rc = NULL;
  }

  if (self->target_cpu == GUM_CPU_IA32)
  {
    g_return_if_fail (ra->width == 32 && !ra->index_is_extended);
    if (rb != NULL)
      g_return_if_fail (rb->width == 32 && !rb->index_is_extended);
    if (rc != NULL)
      g_return_if_fail (rc->width == 32 && !rc->index_is_extended);
  }
  else
  {
    guint nibble = 0;

    if (width_reg->width != default_width)
      nibble |= 0x8;
    if (rb != NULL && rb->index_is_extended)
      nibble |= 0x4;
    if (rc != NULL && rc->index_is_extended)
      nibble |= 0x2;
    if (ra->index_is_extended)
      nibble |= 0x1;

    if (nibble != 0)
      *self->code++ = 0x40 | nibble;
  }
}
