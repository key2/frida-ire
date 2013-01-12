import frida
import sys

def on_close():
	print "on_close!"
def on_message(message, data):
	print "on_message: message=%s data='%s'" % (message, data)

pid = int(sys.argv[1])
session = frida.attach(pid)
session.on('close', on_close)
script = session.create_script("""
var value = 1337;
setInterval(function() {
	send({name: '+ping', payload: value++});
	function onMessage(message) {
		send({name: '+ack', payload: message});
		recv(onMessage);
	}
	recv(onMessage);
}, 3000);
""")
script.on('message', on_message)
script.load()

print "Waiting for messages..."
raw_input()

print "Posting message..."
script.post_message({'name': '+syn'})

print "Waiting for messages..."
raw_input()

print "Goodbye."
