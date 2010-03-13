package org.boblycat.frida.core.disassembler;

abstract public class Instr {

	public abstract String instruction();
    public abstract int address();
    public abstract String args();
    public abstract String comment();
}
