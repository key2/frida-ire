package frida.matcher

class CodeChunk {}
class InstrPattern {}


object CodeMatcher {
	
	def matchPattern(code : CodeChunk, pattern : CodePattern) {}
}

abstract class CodePattern {

	
	def instr(ins : InstrPattern)
	def oneOrMore(ins : CodePattern)
	def sequence(ins : CodePattern*)
	def before(p : CodePattern)
	def after(p : CodePattern)
	def readFrom()
	def writeTo()
	def callTo()
	def jumpTo()
	
}

