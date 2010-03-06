package org.boblycat.frida.plugin.codematcher;

import org.boblycat.frida.plugin.disassembler.mips.MIPSDisasm
import org.boblycat.frida.core.disassembler.DisassemblerFactory

object Main {

	def main(args : Array[String]) {
		org.boblycat.frida.plugin.disassembler.mips.MIPSDisasm.register
		val dis = DisassemblerFactory.create("mipsel");
		for(i <- dis.disassemble("data/mipsel-example.bin")) {
			println(i);
		}
	}
}
