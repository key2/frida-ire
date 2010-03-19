package org.boblycat.frida.core;

import org.boblycat.frida.core.disassembler.Instr;

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class BasicBlock {

    private final Instr[] instructions;
    private final BasicBlock previousBlock;
    private final BasicBlock[] nextBlocks;
    private final long startAddress;

    public BasicBlock(Instr[] instructions,
                      BasicBlock previousBlock,
                      BasicBlock[] nextBlocks,
                      long startAddress) {
        this.instructions = instructions;
        this.previousBlock = previousBlock;
        this.nextBlocks = nextBlocks;
        this.startAddress = startAddress;
    }
    
    public BasicBlock previousBlock() { return previousBlock; }
    public BasicBlock[] nextBlocks() { return nextBlocks; }

    public long getStartAddress() { return startAddress; }
    public Instr[] getInstructions() { return instructions; }
    public Instr instrAt(int index) { return instructions[index]; }
    public int length() { return instructions.length; }
}
