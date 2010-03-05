package frida.disasm

abstract class Disassembler {

	def disassemble(code : Array[Byte]) : Array[Instr]
	def toDisplayString(i : Instr) : String
}
