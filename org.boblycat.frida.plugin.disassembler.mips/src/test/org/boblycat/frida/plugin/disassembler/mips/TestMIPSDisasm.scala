package org.boblycat.frida.plugin.disassembler.mips

import scala.collection.JavaConversions._

/**
 *
 * Created: Mar 20, 2010
 *
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3 
 *
 *
 * @author: karltk@boblycat.org
 */

object TestMIPSDisasm {

  def main(args : Array[String]) {
    val d = new MIPSDisasm()
    val r = d.disassemble(0x19236C, "/home/karltk/source/workspaces/frida-ire/org.boblycat.frida.plugin.disassembler.mips/data/mipsel-example.bin");
    for(i <- r)
      println(i)

  }
}