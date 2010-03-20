package org.boblycat.frida.core.disassembler;

public interface Instr {

	public abstract String instruction();
    public abstract long address();
    public abstract String args();
    public abstract String comment();
}
