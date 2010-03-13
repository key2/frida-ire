package org.boblycat.frida.plugin.disassembler.mips;

import org.boblycat.frida.core.disassembler.Disassembler
import org.boblycat.frida.core.disassembler.Instr
import org.boblycat.frida.core.disassembler.DisassemblerFactory

object InstructionDescription {
	val R = 1
	val I = 2
	val J = 3
}

class InstructionDescription(
		val op : Byte,
		val funct : Byte,
		val fmt : Int, 
		val name : String,
		val desc : String) {
}

object StringFmt {
	def pad(txt : String, w : Int) = txt + " " * (w - txt.length)
}

abstract class MIPSInstr(
   val address : Int,
   val op : Byte // 6 bits
	) extends Instr { 
	override def toString = MIPSDisasm.instrToString(this)
	override def instruction = MIPSDisasm.instrName(op)
  override def comment = ""
}

class R(
      addr : Int,
	    op : Byte, // 6 bits
	val rs : Byte, // 5 bits
	val rt : Byte, // 5 bits
	val rd : Byte, // 5 bits
	val shamt : Byte, // 5 bits
	val funct : Byte) // 6 bits
	extends MIPSInstr(addr, op) {
  override def args =
    "rs = " + MIPSDisasm.registers(rs) +
            ", rt = " + MIPSDisasm.registers(rt) +
            ", rd = " + MIPSDisasm.registers(rd)
}

class I (
    addr : Int,
	    op : Byte, // 6 bits
	val rs : Byte, // 5 bits
	val rt : Byte, // 5 bits
	val imm : Int) // 16 bits
	extends MIPSInstr(addr, op) {
  override def args =
    "rs = " + MIPSDisasm.registers(rs) +
            ", rt = " + MIPSDisasm.registers(rt) +
            ", imm = " + imm
}

class J(
  addr : Int,
		op : Byte, // 6 bits
    val tgt : Int) // 26 bits
    extends MIPSInstr(addr, op) {
   override def args = String.format("0x%x", new java.lang.Integer(tgt))
}


object MIPSDisasm {
		val registers = Map[Int,String](
			0 -> "$zero",
			1 -> "$?1",
			2 -> "$v0",
			3 -> "$v1",
			4 -> "$a0",
			5 -> "$a1",
			6 -> "$a2",
			7 -> "$a3",
			8 -> "$t0",
			9 -> "$t1",
			10 -> "$t2",
			11 -> "$t3",
			12 -> "$t4",
			13 -> "$t5",
			14 -> "$t6",
			15 -> "$t7",
			16 -> "$s0",
			17 -> "$s1",
			18 -> "$s2",
			19 -> "$s3",
			20 -> "$s4",
			21 -> "$s5",
			22 -> "$s6",
			23 -> "$s7",
			24 -> "$t8",
			25 -> "$t9",
			26 -> "$?26",
			27 -> "$?27",
			28 -> "$gp",
			29 -> "$sp",
			30 -> "$fp",
			31 -> "$ra"
			)
			
	val instrSet = 
		List(
				makeR("ADD", "Add (with overflow)",                    "000000ss ssst tttt dddd d000 0010 0000"),
				makeI("ADDI", "Add immediate (with overflow)",         "001000ss ssst tttt iiii iiii iiii iiii"),
				makeI("ADDIU", "Add immediate unsigned (no overflow)", "001001ss ssst tttt iiii iiii iiii iiii"),
				makeR("ADDU", "Add unsigned (no overflow)",            "000000ss ssst tttt dddd d000 0010 0001"),
				makeR("AND", "Bitwise and",                            "000000ss ssst tttt dddd d000 0010 0100"),
				makeI("ANDI", "Bitwise and immediate",                 "001100ss ssst tttt iiii iiii iiii iiii"),
				makeI("BEQ", "Branch on equal",                        "000100ss ssst tttt iiii iiii iiii iiii"),
				makeI("BGEZ", "Branch on greater than or equal to zero", "000001ss sss0 0001 iiii iiii iiii iiii"),
				makeI("BGEZAL", "Branch on greater than or equal to zero and link", "000001ss sss1 0001 iiii iiii iiii iiii"),
				makeI("BGTZ", "Branch on greater than zero",           "000111ss sss0 0000 iiii iiii iiii iiii"),
				makeI("BLEZ", "Branch on less than or equal to zero",  "000110ss sss0 0000 iiii iiii iiii iiii"),
				makeI("BLTZ", "Branch on less than zero",              "000001ss sss0 0000 iiii iiii iiii iiii"),
				makeI("BLTZAL", "Branch on less than zero and link",   "000001ss sss1 0000 iiii iiii iiii iiii"),
				makeI("BNE", "Branch on not equal",                    "000101ss ssst tttt iiii iiii iiii iiii"),
				makeR("DIV", "Divide",                                 "000000ss ssst tttt 0000 0000 0001 1010"),
				makeR("DIVU", "Divide unsigned",                       "000000ss ssst tttt 0000 0000 0001 1011"),
				makeJ("J", "Jump",                                     "000010ii iiii iiii iiii iiii iiii iiii"),
				makeJ("JAL", "Jump and link",                          "000011ii iiii iiii iiii iiii iiii iiii"),
				makeR("JR", "Jump register",                           "000000ss sss0 0000 0000 0000 0000 1000"),
				makeI("LB", "Load byte",                               "100000ss ssst tttt iiii iiii iiii iiii"),
				makeI("LUI", "Load upper immediate",                   "001111-- ---t tttt iiii iiii iiii iiii"),
				makeI("LW", "Load word",                               "100011ss ssst tttt iiii iiii iiii iiii"),
				makeR("MFHI", "Move from HI", "00000000 0000 0000 dddd d000 0001 0000"),
				makeR("MFLO", "Move from LO", "00000000 0000 0000 dddd d000 0001 0010"),
				makeR("MULT", "Multiply", "000000ss ssst tttt 0000 0000 0001 1000"),
				makeR("MULTU", "Multiply unsigned", "000000ss ssst tttt 0000 0000 0001 1001"),
				makeR("NOOP", "no operation", "00000000 0000 0000 0000 0000 0000 0000"),
				makeR("OR", "Bitwise or", "000000ss ssst tttt dddd d000 0010 0101"),
				makeI("ORI", "Bitwise or immediate", "001101ss ssst tttt iiii iiii iiii iiii"),
				makeI("SB", "Store byte", "101000ss ssst tttt iiii iiii iiii iiii"),
				makeR("SLL", "Shift left logical", "000000ss ssst tttt dddd dhhh hh00 0000"),
				makeR("SLLV", "Shift left logical variable", "000000ss ssst tttt dddd d--- --00 0100"),
				makeR("SLT", "Set on less than (signed)", "000000ss ssst tttt dddd d000 0010 1010"),
				makeI("SLTI", "Set on less than immediate (signed)", "001010ss ssst tttt iiii iiii iiii iiii"),
				makeI("SLTIU", "Set on less than immediate unsigned", "001011ss ssst tttt iiii iiii iiii iiii"),
				makeR("SLTU", "Set on less than unsigned", "000000ss ssst tttt dddd d000 0010 1011"),
				makeR("SRA", "Shift right arithmetic", "000000-- ---t tttt dddd dhhh hh00 0011"),
				makeR("SRL", "Shift right logical", "000000-- ---t tttt dddd dhhh hh00 0010"),
				makeR("SRLV", "Shift right logical variable", "000000ss ssst tttt dddd d000 0000 0110"),
				makeR("SUB", "Subtract", "000000ss ssst tttt dddd d000 0010 0010"),
				makeR("SUBU", "Subtract unsigned", "000000ss ssst tttt dddd d000 0010 0011"),
				makeI("SW", "Store word", "101011ss ssst tttt iiii iiii iiii iiii"),
				makeR("SYSCALL", "System call", "000000-- ---- ---- ---- ---- --00 1100"),
				makeR("XOR", "Bitwise exclusive or", "000000ss ssst tttt dddd d--- --10 0110"),
				makeI("XORI", "Bitwise exclusive or immediate", "001110ss ssst tttt iiii iiii iiii iiii")
		)
	
	import org.boblycat.frida.core.BitUtils.{stringToBinary => toBin}
	
	def makeR(name : String, desc : String, format : String) : InstructionDescription = {
		val fmt = format.replace(" ", "")
		val op = fmt.substring(0, 6)
//		val rs = fmt.substring(6, 11)
//		val rt = fmt.substring(11,16)
//		val rd = fmt.substring(16, 21)
//		val shamt = fmt.substring(21, 26)
		val funct = fmt.substring(26, 32) 
		new InstructionDescription(toBin(op).toByte, toBin(funct).toByte, InstructionDescription.R, name, desc)
	}
	
	def makeI(name : String, desc : String, format : String) : InstructionDescription = {
		val fmt = format.replace(" ", "")
		val op = fmt.substring(0, 6)
//		val rs = fmt.substring(7,11)
//		val rt = fmt.substring(12,16)
//		val imm = fmt.substring(16, 32) 
		new InstructionDescription(toBin(op).toByte, 0.toByte, InstructionDescription.I, name, desc)
	}
	
	def makeJ(name : String, desc : String, format : String) : InstructionDescription = {
		val op = format.substring(0, 6)
//		val tgt = format.substring(7, 32) 
		new InstructionDescription(toBin(op).toByte, 0.toByte, InstructionDescription.J, name, desc)
	}

	def instrName(op : Byte) : String = resolveOp(op, 0).name
	
		def instrToString(ins : MIPSInstr) : String = {
		if(ins.isInstanceOf[R]) {
			val rop = resolveOp(ins.op, ins.asInstanceOf[R].funct)
			val r = ins.asInstanceOf[R]
			StringFmt.pad(rop.name, 8) + "rs = " + registers(r.rs) + ", rt = " + registers(r.rt) + ", rd = " + registers(r.rd)
		} else {
			val rop = resolveOp(ins.op, 0)
			if(ins.isInstanceOf[J]) { 
				StringFmt.pad(rop.name, 8) + String.format("0x%x", new java.lang.Integer(ins.asInstanceOf[J].tgt))
			} else {
				val i = ins.asInstanceOf[I]
				StringFmt.pad(rop.name, 8) + "rs = " + registers(i.rs) + ", rt = " + registers(i.rt) + ", imm = " + i.imm 
			}
		}
			
	}

	def resolveOp(op : Byte, funct : Byte) : InstructionDescription = {
		for(i <- instrSet) {
			if(op == i.op)
				if(op != 0) {
					return i
				} else if(funct == i.funct) {
						return i
				}
		}
		throw new RuntimeException("Unknown instruction : " + op)
	}
	

	DisassemblerFactory.addDisassembler("mipsel", new MIPSDisasm())
	def register = 0
}

class MIPSDisasm extends Disassembler {

	import MIPSDisasm.{resolveOp, instrToString}


	def disassemble(program : Array[Byte]) : Array[Instr] = {
		val r = new Array[Instr](program.length/4)
		for(i <- 0 until program.length/4) {
			val pc = i * 4
			val op = (program(pc+3) & (63 << 2)) >> 2
			var funct = program(pc) & 63 
			val shamt = ((program(pc) & (3 << 6)) >> 3) | (program(pc+1) & 7)
			val rop = resolveOp(op.toByte, funct.toByte)
			
			rop.fmt match {
				case InstructionDescription.R => {
					val rs = (program(pc+3) & 3) | (program(pc+2) & (7 << 5)) >> 5
					val rt = program(pc+2) & 31
					val rd = (program(pc+1) & (31 << 3)) >> 3
					r(i) = new R(pc, op.toByte, rs.toByte, rt.toByte, rd.toByte, shamt.toByte, funct.toByte)
				}
				case InstructionDescription.I => {
					val rs = ((program(pc+3) & 3) << 3) | (program(pc+2) & (7 << 5)) >> 5
					val rt = program(pc+2) & 31
					val imm = program(pc+1) << 8 | program(pc)
					r(i) = new I(pc, op.toByte, rs.toByte, rt.toByte, imm)
				}
				case InstructionDescription.J => { 
					val imm = ((program(pc+3) & 3) << 24) | 
					((program(pc+2) << 16) & 0x00FF0000) | 
					((program(pc+1) << 8) & 0x0000FF00) | 
					(program(pc) & 0x000000FF)
					r(i) = new J(pc, op.toByte, imm)
	//			Console.println(pad(rop.name, 8) + " s=" + registers(rs) + ", t=" + registers(rt) + ", d=" + registers(rd))
//					Console.println(pad(rop.name, 8) + " t=" + registers(rt) + ", s=" + registers(rs) + ", imm=" + String.format("0x%x", new Integer(imm)))
	//			Console.println(pad(rop.name, 8) + " " + String.format("0x%x", new Integer(imm.toInt)))
				}
			}
//			Console.println("i = " + String.format("%02x%02x%02x%02x", 
//					new java.lang.`Byte`(program(pc)),
//					new java.lang.`Byte`(program(pc+1)),
//					new java.lang.`Byte`(program(pc+2)),
//					new java.lang.`Byte`(program(pc+3))))
//			Console.println("op = " + asBinStr(op) + ", funct = " + funct.toBinaryString + ", shamt = " + shamt.toBinaryString + " | " + rop)
		}
		r
	}
	
	
	def toDisplayString(i : Instr) = instrToString(i.asInstanceOf[MIPSInstr])
}