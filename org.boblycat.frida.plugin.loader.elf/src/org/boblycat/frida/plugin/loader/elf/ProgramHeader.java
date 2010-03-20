package org.boblycat.frida.plugin.loader.elf;

import org.boblycat.frida.plugin.loader.elf.types.E32Addr;
import org.boblycat.frida.plugin.loader.elf.types.E32Off;
import org.boblycat.frida.plugin.loader.elf.types.E32Word;

import java.io.IOException;

/**
 * Created: Mar 20, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class ProgramHeader {

    public final E32Word type;
    public final E32Off offset;
    public final E32Addr vaddr;
    public final E32Addr paddr;
    public final E32Word filesz;
    public final E32Word memsz;
    public final E32Word flags;
    public final E32Word align;

    public ProgramHeader(
            E32Word type,
            E32Off offset,
            E32Addr vaddr,
            E32Addr paddr,
            E32Word filesz,
            E32Word memsz,
            E32Word flags,
            E32Word align
    ) {
        this.type = type;
        this.offset = offset;
        this.vaddr = vaddr;
        this.paddr = paddr;
        this.filesz = filesz;
        this.memsz = memsz;
        this.flags = flags;
        this.align = align;
    }

    public static ProgramHeader loadFromStream(ELFStreamReader esr) throws IOException {
        return new ProgramHeader(
                esr.readE32Word(), // type
                esr.readE32Off(), // offset
                esr.readE32Addr(), // vaddr
                esr.readE32Addr(), // paddr
                esr.readE32Word(), // filesz
                esr.readE32Word(), // memsz
                esr.readE32Word(), // flags
                esr.readE32Word() // align
        );
    }

    public final static int PT_NULL = 0;
    public final static int PT_LOAD = 1;
    public final static int PT_DYNAMIC = 2;
    public final static int PT_INTERP = 3;
    public final static int PT_NOTE = 4;
    public final static int PT_SHLIB = 5;
    public final static int PT_PHDR = 6;
    public final static int PT_LOPROC = 0x70000000;
    public final static int PT_HIPROC = 0x7FFFFFFF;
    public final static int PT_STACK =  1685382481; // objdump claims so

    public final static String PT_NULL_msg = "Null";
    public final static String PT_LOAD_msg = "Loadable segment";
    public final static String PT_DYNAMIC_msg = "Dynamic linking information";
    public final static String PT_INTERP_msg = "Interpreter";
    public final static String PT_NOTE_msg = "Note section";
    public final static String PT_SHLIB_msg = "Shared library?";
    public final static String PT_PHDR_msg = "Program header";
    public final static String PT_LOPROC_msg ="LOPROC";
    public final static String PT_HIPROC_msg = "HIPROC";
    public final static String PT_STACK_msg = "Stack";

    public final static String[] PT = new String[] {
            PT_NULL_msg,
            PT_LOAD_msg,
            PT_DYNAMIC_msg,
            PT_INTERP_msg,
            PT_NOTE_msg,
            PT_SHLIB_msg,
            PT_PHDR_msg
    };

    public final static int PF_X = 0x1;
    public final static int PF_W = 0x2;
    public final static int PF_R = 0x4;
    public final static int PF_MASKPROC = 0xF0000000;

    public final static String PF_X_msg = "Execute";
    public final static String PF_W_msg = "Write";
    public final static String PF_R_msg = "Read";

    public final static String getFlagDesc(int flags) {
        String r = "";
        if((flags & PF_X) != 0) r += PF_X_msg + " ";
        if((flags & PF_W) != 0) r += PF_W_msg + " ";
        if((flags & PF_R) != 0) r += PF_R_msg + " ";
        return r;
    }

    public void dump() {
        System.out.println("type    : " + getTypeDesc(type.value));
        System.out.println("flags   : " + getFlagDesc(flags.value));
        System.out.println("vaddr   : " + vaddr.value);
        System.out.println("paddr   : " + paddr.value);
        System.out.println("offset  : " + offset.value);
        System.out.println("filesz  : " + filesz.value);
        System.out.println("memsz   : " + memsz.value);
        System.out.println("align   : " + align.value);
    }

    private String getTypeDesc(int value) {
        if(value < PT.length)
            return PT[value];
        if(value == PT_HIPROC)
            return PT_HIPROC_msg;
        if(value == PT_LOPROC)
            return PT_LOPROC_msg;
        if(value == PT_STACK)
                return PT_STACK_msg;
        return "<unknown type : " + value + ">";
    }
}
