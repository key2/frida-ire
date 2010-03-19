package org.boblycat.frida.plugin.codematcher

import org.boblycat.frida.core.disassembler.Instr

class CodeMatch(val seqs : List[Array[Int]]) {
	def foreach(f : (Array[Int]) => Unit) : Unit = {
			for(s <- seqs)
				f(s)
	}
	def merge : PartialFunction[CodeMatch, CodeMatch] = {
		case NoMatch => NoMatch
		case next : CodeMatch => new CodeMatch(seqs ::: next.seqs)
	}
}


class CodeChunk(val instrs : Array[Instr], var pos : Int = 0) {
	def next : Instr = {
		if(pos >= instrs.length)
			return null
		val r = instrs(pos)
		pos += 1
		r
	}
	def prev : Instr = { 
		val r = instrs(pos - 1)
		pos -= 1
		r
	}
	def here = pos
	def there(newPos : Int) = { pos = newPos }
}


class InstrPattern(val name : String) {
	def matches(ins : Instr) : Boolean = name.equals(ins.instruction)
	override def toString = name
}

object CodeMatcher {

	
	val jump = new InstrPattern("J")
	val skip = new Skip(new Stop())
	
	def pattern = new Parent()
	def chunk(instrs : Array[Instr]) = new CodeChunk(instrs)
}

object NoMatch extends CodeMatch(List())

class Parent extends CodePattern(null) {
	override protected val parent : CodePattern = this
	override def matches(code : CodeChunk) = next.matches(code)
	override def toString = next.toString
}

class Stop extends CodePattern(null) {
	override def matches(code : CodeChunk) = new CodeMatch(List())
}

class Skip(parent : CodePattern) extends CodePattern(parent) {
	override def matches(code : CodeChunk) = {
		code.next
		doNext(code) 
	}
	
	override def toString = "skip " + nextToString 
}

class Start(parent : CodePattern) extends CodePattern(parent) {
	override def matches(code : CodeChunk) = next.matches(code)
	override def toString = "start" + nextToString
}

class OneOrMore(parent : CodePattern, p : CodePattern) extends CodePattern(parent) {
	override def matches(code : CodeChunk) : CodeMatch = {
		var allMatch = List[Array[Int]]()
		while(true) {
			val n = doNext(code)
			if(n != NoMatch) {
				return n
			}
			var m = p matches code
			if(p == NoMatch) {
				return allOrNoMatch(allMatch)
			}
			allMatch = allMatch ::: m.seqs
		}
		NoMatch
	}
	
	private def allOrNoMatch(ls : List[Array[Int]]) : CodeMatch = {
		if(ls == List()) {
			NoMatch
		} else {
			new CodeMatch(ls)
		}
	}
	
	override def toString = "oneOrMore[" + p.toString + "] " + next.toString 
}
class Ins(parent : CodePattern, val ins : InstrPattern) extends CodePattern(parent) {
	override def matches(code : CodeChunk) : CodeMatch = {
		val i = code.next
		if(i == null) 
			return NoMatch
		if(ins.matches(i)) {
			new CodeMatch(List(Array(code.here))).merge(doNext(code))
		} else {
			code.prev
			doNext(code)
		}
	}
	
	override def toString = "ins[" + ins.toString + "] " + nextToString
}

class Sequence(parent : CodePattern, val ins : List[CodePattern]) extends CodePattern(parent) {
	override def matches(code : CodeChunk) : CodeMatch = {
		val here = code.here
		var allMatch = List[Array[Int]]()
		for(i <- ins) {
			i.matches(code) match {
				case NoMatch => {
					code.there(here)
					return NoMatch
				}
				case m : CodeMatch => {
					allMatch = allMatch ::: m.seqs
				}
			}
		}
		new CodeMatch(allMatch).merge(doNext(code))
	}
	
	override def toString = "seq[" + (for(i <- ins) yield (i + ",")) + "]" 
}

abstract class CodePattern(protected val parent : CodePattern) {
	protected var next : CodePattern = null
	def matches(code : CodeChunk) : CodeMatch
	
	def start : CodePattern = new Start(parent)
	def stop() = parent
	
	def instr(ins : InstrPattern) : CodePattern = {
		next = new Ins(parent, ins)
		next
	}
	def instr(name : String) : CodePattern = {
		next = new Ins(parent, new InstrPattern(name))
		next
	}
	def oneOrMore(p : CodePattern) : CodePattern = {
		next = new OneOrMore(parent, p)
		next
	}
	def sequence(seq : CodePattern*) : CodePattern = {
		next = new Sequence(parent, seq.toList)
		next
	}
	
	protected def doNext(code : CodeChunk) = if(next == null) NoMatch else next.matches(code)
	protected def nextToString = if (next == null) "" else " " + next.toString 
}

