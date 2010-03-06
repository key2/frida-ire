package org.boblycat.frida.core.disassembler;

import java.io.IOException;
import java.io.RandomAccessFile;

import org.boblycat.frida.core.FridaException;

abstract public class Disassembler {

	private boolean ensureExistsAndReadable(String fileName) {
		return true;
	}
	
	public byte[] loadBytes(String fileName) throws IOException {
		RandomAccessFile ra = new RandomAccessFile(fileName, "r");
		byte[] r = new byte[(int)ra.length()];
		ra.read(r);
		return r;
	}
	
	
	public Instr[] disassemble(String fileName) throws FridaException, IOException {
		
		if(!ensureExistsAndReadable(fileName)) {
			throw new FridaException(String.format("File %s does not exist or is not readable", fileName));
		}
		
		byte[] binChunk = loadBytes(fileName);
		return disassemble(binChunk);
	}
	
	abstract public Instr[] disassemble(byte[] code);
	abstract public String toDisplayString(Instr instr);
}
