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

#include "gumfunctionparser.h"

#include <udis86.h>

void
gum_function_parser_init (GumFunctionParser * fp)
{
}

void
gum_function_parser_parse (GumFunctionParser * fp,
                           gpointer func_address,
                           GumFunctionDetails * details)
{
  ud_t ud_obj;
  guint insn_size;
  const guint buf_size = 4096;

  ud_init (&ud_obj);
  ud_set_mode (&ud_obj, GUM_CPU_MODE);
  /*ud_set_syntax (&ud_obj, UD_SYN_INTEL);*/

  ud_set_input_buffer (&ud_obj, func_address, buf_size);

  while (TRUE)
  {
    insn_size = ud_disassemble (&ud_obj);
    g_assert (insn_size != 0);

    /*g_print ("%s\n", ud_insn_asm (&ud_obj));*/

    if (ud_obj.mnemonic == UD_Iret)
    {
      details->arglist_size = ud_obj.operand[0].lval.udword;
      break;
    }
    else if (ud_obj.mnemonic == UD_Ijmp)
    {
      if (ud_obj.operand[0].type == UD_OP_JIMM &&
          ud_obj.operand[0].base == UD_NONE)
      {
        gpointer target = ud_obj.inp_buff + ud_obj.operand[0].lval.sdword;
        ud_set_input_buffer (&ud_obj, target, buf_size);
      }
      else
      {
        details->arglist_size = -1;
        break;
      }
    }
  }
}

