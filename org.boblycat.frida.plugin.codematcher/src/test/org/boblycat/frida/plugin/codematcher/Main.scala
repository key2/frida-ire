package org.boblycat.frida.plugin.codematcher;

import scala.collection.JavaConversions._
import org.boblycat.frida.core.disassembler.DisassemblerFactory

object Main {

	import CodeMatcher._
	
	def main(args : Array[String]) {
		org.boblycat.frida.plugin.disassembler.mips.MIPSDisasm.register
		val dis = DisassemblerFactory.create("mipsel");
		val code = dis.disassemble(0, "data/mipsel-example.bin") 
		for(i <- code) {
			println(i);
		}
		
		println(" --- ")
		
		val pat = pattern.oneOrMore(skip).instr("ADDIU").instr("ADDIU").instr("ADDIU").stop;
		println(pat)
		for(m <- pat matches chunk(code.getInstructions)) 
			println(m.map(_.toString).reduceLeft(_ + ", " + _)) 
	}
}
