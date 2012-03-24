package org.boblycat.frida.core;

/**
 *
 * Created: Feb 26, 2010
 *
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 *
 * @author: karltk@boblycat.org
 */

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
