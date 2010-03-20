package org.boblycat.frida.plugin.loader.elf;

import org.boblycat.frida.plugin.loader.elf.ELFStreamReader;
import org.boblycat.frida.plugin.loader.elf.types.*;

import java.io.IOException;
import java.io.InputStream;

/**
 * Created: Mar 19, 2010
 * <p/>
 * Part of Frida IRE.
 * Copyright (c) 2010, Karl Trygve Kalleberg, Ole André Vadla Ravnås
 * Licensed under the GNU General Public License, v3
 *
 * @author: karltk@boblycat.org
 */
public class ELFHeader {
    public static final int NIDENT = 16;

    final UChar[] ident = new UChar[NIDENT];
    final E32Half type;
    final E32Half machine;
    final E32Word version;
    final E32Addr entry;
    final E32Off phoff;
    final E32Off shoff;
    final E32Word flags;
    final E32Half ehsize;
    final E32Half phentsize;
    final E32Half phnum;
    final E32Half shentsize;
    final E32Half shnum;
    final E32Half shtstrndx;

    /*
     * Alternatives for type
     */
    final static int ET_NONE = 0;
    final static int ET_REL = 1;
    final static int ET_EXEC = 2;
    final static int ET_DYN = 3;
    final static int ET_CORE = 4;
    final static int ET_LOPROC = 0xFF00;
    final static int ET_HIPROC = 0x00FF;

    final static String ET_NONE_msg = "No file type";
    final static String ET_REL_msg = "Relocatable file";
    final static String ET_EXEC_msg = "Executable file";
    final static String ET_DYN_msg = "Shared object file";
    final static String ET_CORE_msg = "Core file";
    final static String ET_LOPROC_msg = "Processor-specific";
    final static String ET_HIPROC_msg = "Processor-specific";

    /*
     * Alternatives for machine
     */

    final static int EM_NONE = 0;
    final static int EM_M32 = 1;
    final static int EM_SPARC = 2;
    final static int EM_386 = 3;
    final static int EM_68K = 4;
    final static int EM_88K = 5;
    final static int EM_860 = 7;
    final static int EM_MIPS = 8;
    final static int EM_MIPS_RS4_BE = 10;

    final static String EM_NONE_msg = "No machine";
    final static String EM_M32_msg = "AT&T WE 32100";
    final static String EM_SPARC_msg = "SPARC";
    final static String EM_386_msg = "Intel 80386";
    final static String EM_68K_msg = "Motorola 68000";
    final static String EM_88K_msg = "Motorola 68000";
    final static String EM_860_msg = "Intel 80860";
    final static String EM_MIPS_msg = "MIPS RS3000 Big-Endian";
    final static String EM_MIPS_RS4_BE_msg = "MIPS RS3000 Big-Endian";
    final static String EM_RESERVED_msg = "Reserved for future use";

    final static String[] EM = new String[] {
            EM_NONE_msg,
            EM_M32_msg,
            EM_SPARC_msg,
            EM_386_msg,
            EM_68K_msg,
            EM_88K_msg,
            "<unknown: 6>",
            EM_860_msg,
            EM_MIPS_msg,
            "<unknown: 9>",
            EM_MIPS_RS4_BE_msg
    };
    /*
     * Alternatives for version
     */
    final static int EV_NONE = 0;
    final static int EV_CURRENT = 1;

    final static String EV_NONE_msg = "Invalid version";
    final static String EV_CURRENT_msg = "Current version";

    final static String[] EV  = new String[] {
            EV_NONE_msg,
            EV_CURRENT_msg
    };
    
    private ELFHeader(
            final UChar[] ident,
            final E32Half type,
            final E32Half machine,
            final E32Word version,
            final E32Addr entry,
            final E32Off phoff,
            final E32Off shoff,
            final E32Word flags,
            final E32Half ehsize,
            final E32Half phentsize,
            final E32Half phnum,
            final E32Half shentsize,
            final E32Half shnum,
            final E32Half shtstrndx) {
        for(int i = 0; i < NIDENT; i++)
            this.ident[i] = ident[i];
        this.type = type;
        this.machine = machine;
        this.version = version;
        this.entry = entry;
        this.phoff = phoff;
        this.shoff = shoff;
        this.flags = flags;
        this.ehsize = ehsize;
        this.phentsize = phentsize;
        this.phnum = phnum;
        this.shentsize = shentsize;
        this.shnum = shnum;
        this.shtstrndx = shtstrndx;
    }

    public static ELFHeader loadFromStream(ELFStreamReader esr) throws IOException {
        return new ELFHeader(
                esr.readUChar(16), // ident
                esr.readE32Half(), // type
                esr.readE32Half(), // machine
                esr.readE32Word(), // version
                esr.readE32Addr(), // entry
                esr.readE32Off(), // phoff
                esr.readE32Off(), // shoff
                esr.readE32Word(), // flags
                esr.readE32Half(), // ehsize
                esr.readE32Half(), // phentsize
                esr.readE32Half(), // phnum
                esr.readE32Half(), // shentsize
                esr.readE32Half(), // shnum
                esr.readE32Half() // shtstrndx
        );
    }

    public char[] getELFMagic() {
        char[] r = new char[4];
        for(int i = 0; i < 4; i++)
            r[i] = (char)ident[i].value;
        return r;
    }

    public byte getELFClass() {
        return ident[4].value;
    }

    public byte getELFDataEncoding() {
        return ident[5].value;
    }

    public byte getELFVersion() {
        return ident[6].value;
    }
    
    public final static int ELF_CLASS_NONE = 0;
    public final static int ELF_CLASS_32 = 1;
    public final static int ELF_CLASS_64 = 2;

    public final static String ELF_CLASS_NONE_msg = "Invalid class";
    public final static String ELF_CLASS_32_msg = "32-bit objects";
    public final static String ELF_CLASS_64_msg = "64-bit objects";
    public final static String[] ELF_CLASS = new String[] {
            ELF_CLASS_NONE_msg,
            ELF_CLASS_32_msg,
            ELF_CLASS_64_msg
    };

    public final static int ELF_DATA_NONE = 0;
    public final static int ELF_DATA_LSB = 1;
    public final static int ELF_DATA_MSB = 2;

    public final static String ELF_DATA_NONE_msg = "Invalid class";
    public final static String ELF_DATA_LSB_msg = "Little endian";
    public final static String ELF_DATA_MSB_msg = "Big endian";

    public final static String[] ELF_DATA = new String[] {
            ELF_DATA_NONE_msg,
            ELF_DATA_LSB_msg,
            ELF_DATA_MSB_msg
    };

    public void dump() {
        
        System.out.println("ELF magic         : " + getELFMagic());
        System.out.println("ELF class         : " + getClassDesc(getELFClass()));
        System.out.println("ELF data encoding : " + getDataEncodingDesc(getELFDataEncoding()));
        System.out.println("machine           : " + getMachineDesc(machine.value));
        System.out.println("version           : " + getVersionDesc(version.value));

        System.out.println("base address      : " + entry.value);
        
        System.out.println("Program header");
        System.out.println("  - offset        : " + phoff.value);
        System.out.println("  - # entries     : " + phnum.value);
        System.out.println("  - entry size    : " + phentsize.value);

        System.out.println("Section header");
        System.out.println("  - offset        : " + shoff.value);
        System.out.println("  - # entries     : " + shnum.value);

        System.out.println("String table");
        System.out.println("  - offset        : " + shtstrndx.value);

        System.out.println("ELF header size   : " + ehsize.value);
    }

    private String getVersionDesc(int value) {
        return EV[value];            
    }

    public static String getMachineDesc(short value) {
        if(EM.length < value) {
            return "<unknown: " + value + ">";
        }
        return EM[value];
    }

    public static String getDataEncodingDesc(byte elfDataEncoding) {
        return ELF_DATA[elfDataEncoding];
    }

    public static String getClassDesc(byte elfClass) {
        return ELF_CLASS[elfClass];
    }

}