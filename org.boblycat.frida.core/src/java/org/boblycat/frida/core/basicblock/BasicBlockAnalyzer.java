package org.boblycat.frida.core.basicblock;

import org.boblycat.frida.core.BasicBlock;
import org.boblycat.frida.core.CodeChunk;
import org.boblycat.frida.core.disassembler.Instr;
import org.boblycat.frida.core.disassembler.JumpInstr;

import java.util.*;

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class BasicBlockAnalyzer {

    /*
     * Slow and naive basic two-pass basic block computation
     */
    public BasicBlock analyze(CodeChunk codeChunk) {
        final Instr[] instrs = codeChunk.getInstructions();

        final Set<Long> starts = new HashSet<Long>();

        for(int index = 0; index < instrs.length; index++) {
            if(instrs[index] instanceof JumpInstr) {
                starts.add(((JumpInstr)instrs[index]).getDestination());
            }
        }

        long spanStart = codeChunk.getOffset(0);
        for(int index = 0; index < instrs.length; index++) {
            final Instr instr = instrs[index];
            final long currentOffset = instrs[index].address();
            if(instr instanceof JumpInstr || starts.contains(currentOffset)) {
                System.out.println(String.format("0x%08x -> 0x%08x", spanStart, currentOffset));
                spanStart = currentOffset;
            }
        }
        System.out.println(String.format("0x%08x -> 0x%08x", spanStart, codeChunk.getOffset(codeChunk.length() - 1)));
        return null;
    }
}
