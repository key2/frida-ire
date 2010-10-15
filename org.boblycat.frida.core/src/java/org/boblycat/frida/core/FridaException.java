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
public class FridaException extends Exception {

	private static final long serialVersionUID = 2353323752474058084L;

	public FridaException(String msg) {
		super(msg);
	}
}
