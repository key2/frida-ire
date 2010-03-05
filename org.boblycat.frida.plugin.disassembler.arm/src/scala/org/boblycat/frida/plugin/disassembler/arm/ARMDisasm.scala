package org.boblycat.frida.plugin.disassembler.arm

import org.boblycat.frida.core.FridaException
import org.boblycat.frida.core.disassembler.Instr
import org.boblycat.firda.core.disassember.Disassembler
import org.boblycat.frida.core.PrintUtils

class I(
		val id : InstructionDescriptor
		) extends Instr
		
class InstructionDescriptor(
		val name : String,
		val fmt : String,
		val mask : Short,
		val pattern : Short) {
	override def toString = name + ", fmt=" + fmt + ", mask=" + PrintUtils.toBinaryString(mask) + ", pattern=" + PrintUtils.toBinaryString(pattern)
	
	def matches(bits : Short) = (bits & mask) == pattern
	def make(bits : Short) : Instr = {
		new I(this)
	}
}

class ARMInstr extends Instr

class ARMDisasm extends Disassembler {

	private def ID(n : String, f : String) = {
		new InstructionDescriptor(n, f, computeMask(f), computePattern(f))
	}
	
	private def computePattern(fmt : String) : Short = {
		var m = 0
		for(c <- fmt) {
			m <<= 1
			if(c == '1') {
				m |= 1
			} else { 
				m |= 0
			}
		}
		m.toShort
	}
	
	private def computeMask(fmt : String) : Short = {
		var m = 0
		for(c <- fmt) {
			m <<= 1
			if(c == '0' || c == '1')
				m |= 1
		}
		m.toShort
	}
	
	val instrSet = thumbs1Set ::: thumbs2Set
		
	val thumbs1Set = 
		List(
		ID("ADD"  , "0001100nnnsssddd"),
		ID("SUB"  , "0001101nnnsssddd"),
		ID("ADD"  , "0001110iiisssddd"),
		ID("SUB"  , "0001111iiisssddd"),
        
		ID("MOV"  , "00100dddOOOOOOOO"),
		ID("CMP"  , "00101dddOOOOOOOO"),
		ID("ADD"  , "00110dddOOOOOOOO"),
		ID("SUB"  , "00111dddOOOOOOOO"),
		
		ID("AND"  , "0100000000sssddd"),
		ID("EOR"  , "0100000001sssddd"),
		ID("LSL"  , "0100000010sssddd"),
		ID("LSR"  , "0100000011sssddd"),
		ID("ASR"  , "0100000100sssddd"),
		ID("ADC"  , "0100000101sssddd"),
		ID("SBC"  , "0100000110sssddd"),
		ID("ROR"  , "0100000111sssddd"),
		ID("TST"  , "0100001000sssddd"),
		ID("NEG"  , "0100001001sssddd"),
        ID("CMP"  , "0100001010sssddd"),
        ID("CMN"  , "0100001011sssddd"),
        ID("ORR"  , "0100001100sssddd"),
        ID("MUL"  , "0100001101sssddd"),
        ID("BIC"  , "0100001110sssddd"),
        ID("MVN"  , "0100001111sssddd"),
		
        ID("ADD"   ,"01000100hHsssddd"),
        ID("CMP"   ,"01000101hHsssddd"),
        ID("MOV"   ,"01000110hHsssddd"),
        ID("BX"    ,"01000111hHsssddd"),

        ID("LDR"   ,"01001dddIIIIIIII"),
        
        ID("LDR"   ,"0101100ooobbbddd"), // "nnnsssddd"
        ID("STR"   ,"0101000ooobbbddd"),
        ID("LDRB"  ,"0101110ooobbbddd"),
        ID("LDRB"  ,"0101010ooobbbddd"),

        ID("STRH"  ,"0101001ooobbbddd"),
        ID("LDRH"  ,"0101101ooobbbddd"),
        ID("LDSB"  ,"0101011ooobbbddd"),
        ID("LDSH"  ,"0101111ooobbbddd"),
        
        ID("STRH"  ,"10000OOOOObbbddd"),
        ID("LDRH"  ,"10001OOOOObbbddd"),
        
        ID("STR"   ,"10010dddIIIIIIII"),
        ID("LDR"   ,"10011dddIIIIIIII"),

        ID("ADDPC" ,"10100dddIIIIIIII"),
        ID("ADDSP" ,"10101dddIIIIIIII"),
        
        ID("ADDSP" ,"10110000IIIIIIII"),
        
        ID("PUSH"  ,"10110100rrrrrrrr"),
        ID("PUSHPC","10110101rrrrrrrr"),
        ID("POP"   ,"10111100rrrrrrrr"),
        ID("POPPC" ,"10111101rrrrrrrr"),
        
        ID("STMIA" ,"11000bbbrrrrrrrr"),
        ID("LDMIA" ,"11001bbbrrrrrrrr"),

        ID("BEQ"   ,"11010000IIIIIIII"),
        ID("BNE"   ,"11010001IIIIIIII"),
        ID("BCS"   ,"11010010IIIIIIII"), 
        ID("BCC"   ,"11010011IIIIIIII"), 
        ID("BMI"   ,"11010100IIIIIIII"), 
        ID("BPL"   ,"11010100IIIIIIII"), 
        ID("BVS"   ,"11010110IIIIIIII"), 
        ID("BVC"   ,"11010111IIIIIIII"), 
        ID("BHI"   ,"11011000IIIIIIII"), 
        ID("BLS"   ,"11011001IIIIIIII"), 
        ID("BGE"   ,"11011010IIIIIIII"), 
        ID("BLT"   ,"11011011IIIIIIII"), 
        ID("BGT"   ,"11011100IIIIIIII"), 
        ID("BLE"   ,"11011101IIIIIIII"), 
        
        ID("SWI"   ,"11011111IIIIIIII"),
        ID("B"     ,"111100IIIIIIIIII"),
        ID("BL"    ,"1111HIIIIIIIIIII")
        );

	val thumbs2Set = 
		List(
	        ID("PUSHW" ,"11101???????????")
		)

	private def instrToString(ins : ARMInstr) = "foo"
 
	def dumpTable() = {
		for(i <- instrSet) {
			Console.println(i)
		}
	}
	
	private def resolveOp(bits : Short) : Instr = {
		for(i <- instrSet) {
			if(i.matches(bits)) {
				Console.println("M : " + i)
				return i.make(bits)
			}
		}
		throw new FridaException("Unknown opcode " + PrintUtils.toBinaryString(bits))
	}
	
	def disassemble(program : Array[Byte]) : Array[Instr] = {
		val r = new Array[Instr](program.length/2)
		for(pc <- 0 until program.length/2) {
			val bits = ((program(pc*2).toInt & 0xFF) | ((program(pc*2 + 1).toInt << 8) & 0xFF00)).toShort
			Console.println(pc*2  + ": " +
					PrintUtils.toBinaryString(program(pc*2 + 1)) + " " +
					PrintUtils.toBinaryString(program(pc*2)) + " " +
					PrintUtils.toBinaryString(bits))
			r(pc) = resolveOp(bits.toShort)
		}
		r
	}
	
	def toDisplayString(ins : Instr) = instrToString(ins.asInstanceOf[ARMInstr])
}