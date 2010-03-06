package org.boblycat.frida.core.disassembler;

import java.util.HashMap;
import java.util.Map;

public class DisassemblerFactory {

	private static Map<String, Disassembler> knownDisassemblers = new HashMap<String, Disassembler>();
	
	public static void addDisassembler(String name, Disassembler newDisasm) {
		knownDisassemblers.put(name, newDisasm);
	}
	public static Disassembler create(String machine) {
		return knownDisassemblers.get(machine);
	}
}