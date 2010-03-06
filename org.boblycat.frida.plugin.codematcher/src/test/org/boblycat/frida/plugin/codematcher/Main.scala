package org.boblycat.frida.plugin.codematcher;

import org.boblycat.frida.plugin.disassembler.mips.MIPSDisasm
import org.boblycat.frida.core.disassembler.DisassemblerFactory

object Main {

	import CodeMatcher._
	
	def main(args : Array[String]) {
		org.boblycat.frida.plugin.disassembler.mips.MIPSDisasm.register
		val dis = DisassemblerFactory.create("mipsel");
		val instrs = dis.disassemble("data/mipsel-example.bin") 
		for(i <- instrs) {
			println(i);
		}
		
		println(" --- ")
		
		val pat = pattern.oneOrMore(skip).instr("ADDIU").instr("ADDIU").instr("ADDIU").stop;
		println(pat)
		for(m <- pat matches chunk(instrs)) 
			println(m.map(_.toString).reduceLeft(_ + ", " + _)) 
	}
}
