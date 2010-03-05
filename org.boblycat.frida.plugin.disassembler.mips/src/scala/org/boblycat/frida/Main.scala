package org.boblycat.frida

import java.io.RandomAccessFile
import java.io.FileReader
import java.io.FileInputStream
import java.io.BufferedReader
import org.boblycat.frida.core.disassembler.DisassemblerFactory

object Main {

	private val version = "0.1.0"
		
	private def about = {
		  "FRIDA - Free, Interactive DisAssembler, v" + version + "\n" +
 		  "Copyright (c) 2010" + "\n" +
		  "Licensed under the GNU General Public License v3"
	}
		
	def main(args : Array[String]) = {
		
		if(args.length < 3) {
			val file = args(1)
			val machine = args(0)
			doDissassemble(file, machine)
		}
	}
	
	def validateMachine : PartialFunction[String, Boolean] = { 
		case "arm" => true
		case "mipsel" => true
		case _ => false
	}
	
	def die(msg : String, args : String*) = {
		Console.err.println(String.format(msg, args))
		System.exit(1)
	}
	
	def ensureExistsAndReadable(fileName : String) = {
		true
	}
	
	def loadBytes(fileName : String) : Array[Byte] = {
		var ra = new RandomAccessFile(fileName, "r")
		val r = new Array[Byte](ra.length.toInt)
		ra.read(r)
		return r
	}
	
	
	private def doDissassemble(file : String, machine : String) {
		
		if(!ensureExistsAndReadable(file)) {
			die("File %s does not exist or is not readable", file)
		}
		
		if(!validateMachine(machine)) {
			die("Architecture %s is not known", machine)
		}
		
		val da = DisassemblerFactory.create(machine)
		//da.asInstanceOf[MIPS].dumpTable()
		val binChunk = loadBytes(file)
		val instrs = da.disassemble(binChunk)
		for(i <- instrs) {
			Console.println("\t" + da.toDisplayString(i))
		}
	}
}
