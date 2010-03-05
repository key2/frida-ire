package frida.matcher

class CodeChunk {}
class Instr {}

object CodeMatcher {
	
	def matchPattern(code : CodeChunk, pattern : CodePattern) {}
}

abstract class CodePattern {

	
	def oneOrMore(ins : Instr)
	def one(ins : Instr)
	def before(p : CodePattern)
	def after(p : CodePattern)
	def readFrom()
	def writeTo()
	def callTo()
	def jumpTo()
	
}

