package org.boblycat.frida.core.disassembler;

abstract public class Disassembler {

	abstract public Instr[] disassemble(byte[] code);
	abstract public String toDisplayString(Instr instr);
}
