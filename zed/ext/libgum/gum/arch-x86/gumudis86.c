/*
 * Copyright (C) 2009 Ole Andr� Vadla Ravn�s <ole.andre.ravnas@tandberg.com>
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

#include "gumudis86.h"

guint
gum_find_instruction_length (guint8 * code)
{
  ud_t ud_obj;
  guint insn_size;

  ud_init (&ud_obj);
  ud_set_mode (&ud_obj, 32);
  ud_set_input_buffer (&ud_obj, code, 16);

  insn_size = ud_disassemble (&ud_obj);
  g_assert (insn_size != 0);

  return insn_size;
}

gboolean
gum_mnemonic_is_jcc (ud_mnemonic_code_t mnemonic)
{
  switch (mnemonic)
  {
    case UD_Ija:
    case UD_Ijae:
    case UD_Ijb:
    case UD_Ijbe:
    case UD_Ijg:
    case UD_Ijge:
    case UD_Ijl:
    case UD_Ijle:
    case UD_Ijnp:
    case UD_Ijns:
    case UD_Ijnz:
    case UD_Ijo:
    case UD_Ijp:
    case UD_Ijs:
    case UD_Ijz:
      return TRUE;

    default:
      break;
  }

  return FALSE;
}

guint8
gum_jcc_insn_to_short_opcode (guint8 * code)
{
  if (code[0] == 0x0f)
    return code[1] - 0x10;
  else
    return code[0];
}

guint8
gum_jcc_insn_to_near_opcode (guint8 * code)
{
  if (code[0] == 0x0f)
    return code[1];
  else
    return code[0] + 0x10;
}

guint8
gum_jcc_opcode_negate (guint8 opcode)
{
  if (opcode % 2 == 0)
    return opcode + 1;
  else
    return opcode - 1;
}
