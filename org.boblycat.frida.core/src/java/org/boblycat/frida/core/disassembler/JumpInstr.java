package org.boblycat.frida.core.disassembler;

/**
 * Created: Mar 20, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public interface JumpInstr extends Instr {

    public abstract long getDestination();
}
