import frida
import sys

def on_close():
	print "on_close!"
def on_message(message, data):
	print "on_message: message='%s' data='%s'" % (message, data)

manager = frida.SessionManager()
session = manager.obtain_session_for(int(sys.argv[1]))
session.on('close', on_close)
script = session.create_script("""
var value = 1337;
setInterval(function() {
	send(value++);
}, 3000);
""")
script.on('message', on_message)
script.load()

print "Waiting for messages..."
raw_input()
