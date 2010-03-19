package org.boblycat.frida.core;

import org.boblycat.frida.core.disassembler.Instr;

import java.util.Iterator;

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class CodeChunk implements Iterable<Instr> {
    private final long baseAddress;
    private final Instr[] instructions;


    public CodeChunk(long baseAddress, Instr[] instructions) {
        this.baseAddress = baseAddress;
        this.instructions = instructions;
    }

    public Instr[] getInstructions() { return instructions; }
    public long getBaseAddress() { return baseAddress; }
    public int length() { return instructions.length; }

    @Override
    public Iterator<Instr> iterator() {
        return new Iterator<Instr>() {
            private int pos = 0;
            @Override
            public boolean hasNext() {
                return pos < instructions.length;
            }

            @Override
            public Instr next() {
                return instructions[pos++];
            }

            @Override
            public void remove() {
                throw new UnsupportedOperationException();
            }
        };
    }
}
