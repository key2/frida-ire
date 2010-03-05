package frida.disasm

import frida.core.FridaException

object DisassemblerFactory {

	def create(machine : String) : Disassembler = {
		machine match {
			case "mipsel" => new mips.MIPSDisasm()
			case "arm" => new arm.ARMDisasm()
			case _ => throw new FridaException("No disassembler found for machine type '" + machine + "'")
		}
	}
}
