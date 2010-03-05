package org.boblycat.frida.core;

public class PrintUtils {

	private static String toBinaryString(int b, int bits) {
		int v = 1 << (bits - 1);
		String s = "";
		for(int i = 0; i < bits; i++) {
			if((b & v) == v) {
				s += "1";
			} else { 
				s += "0";
			}
			v >>= 1;
		}
		return s;
	}
	
	public static String toBinaryString(int b) {
		return toBinaryString(b, 32);
	}
	
	public static String toBinaryString(short b) {
		return toBinaryString(b, 16);
	}
	
	public static String toBinaryString(byte b) {
		return toBinaryString(b, 8);
	}

	public static String toPaddedBinaryString(byte b) {
		return String.format("%08s", toBinaryString(b)); 
	}
}
