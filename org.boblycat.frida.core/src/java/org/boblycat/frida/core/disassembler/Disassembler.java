package org.boblycat.frida.core.disassembler;

import java.io.IOException;
import java.io.RandomAccessFile;

import org.boblycat.frida.core.BinaryCodeChunk;
import org.boblycat.frida.core.CodeChunk;
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
	

    public CodeChunk disassemble(BinaryCodeChunk binaryCodeChunk) {
        return disassemble(binaryCodeChunk.getBaseAddress(), binaryCodeChunk.getBytes());
    }
    
	public CodeChunk disassemble(long baseAddress, String fileName) throws FridaException, IOException {
		
		if(!ensureExistsAndReadable(fileName)) {
			throw new FridaException(String.format("File %s does not exist or is not readable", fileName));
		}
		
		byte[] binChunk = loadBytes(fileName);
		return disassemble(baseAddress, binChunk);
	}
	
	abstract public CodeChunk disassemble(long baseAddress, byte[] code);
	abstract public String toDisplayString(Instr instr);
}
