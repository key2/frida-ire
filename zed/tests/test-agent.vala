namespace Zed.AgentTest {
	public static void add_tests () {
		GLib.Test.add_func ("/Agent/Memory/query-modules", () => {
			var h = new Harness ((h) => Memory.query_modules (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Agent/Memory/query-module-functions", () => {
			var h = new Harness ((h) => Memory.query_module_functions (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Agent/Memory/scan-module-for-code-pattern", () => {
			var h = new Harness ((h) => Memory.scan_module_for_code_pattern (h as Harness));
			h.run ();
		});

		GLib.Test.add_func ("/Agent/Script/attach-and-receive-messages", () => {
			var h = new Harness ((h) => Script.attach_and_receive_messages (h as Harness));
			h.run ();
		});
	}

	namespace Memory {

		private static async void query_modules (Harness h) {
			var session = yield h.load_agent ();

			AgentModuleInfo[] modules;
			try {
				modules = yield session.query_modules ();
			} catch (IOError attach_error) {
				assert_not_reached ();
			}

			assert (modules.length > 0);
			if (GLib.Test.verbose ()) {
				foreach (var module in modules)
					stdout.printf ("module: '%s'\n", module.name);
			}

			yield h.unload_agent ();

			h.done ();
		}

		private static async void query_module_functions (Harness h) {
			var session = yield h.load_agent ();

			AgentFunctionInfo[] functions;
			try {
				functions = yield session.query_module_functions ("kernel32.dll");
			} catch (IOError attach_error) {
				assert_not_reached ();
			}

			assert (functions.length > 0);
			if (GLib.Test.verbose ()) {
				foreach (var function in functions)
					stdout.printf ("function: '%s'\n", function.name);
			}

			yield h.unload_agent ();

			h.done ();
		}

		private static async void scan_module_for_code_pattern (Harness h) {
			var session = yield h.load_agent ();

			uint64[] matches;
			try {
				matches = yield session.scan_module_for_code_pattern ("kernel32.dll", "55 8b ec");
			} catch (IOError scan_error) {
				assert_not_reached ();
			}

			assert (matches.length > 0);
			if (GLib.Test.verbose ()) {
				uint i = 1;
				foreach (var address in matches)
					stdout.printf ("Match #%u found at 0x%08" + uint64.FORMAT_MODIFIER + "x\n", i++, address);
			}

			yield h.unload_agent ();

			h.done ();
		}

	}

	namespace Script {

		private static async void attach_and_receive_messages (Harness h) {
			var session = yield h.load_agent ();

			AgentScriptInfo script;
			try {
				script = yield session.attach_script_to ("send_int32 (arg0)\nsend_narrow_string (arg1)", (uint64) target_function);
			} catch (IOError attach_error) {
				assert_not_reached ();
			}

			target_function (1337, "Frida rocks");

			var msg = yield h.wait_for_message ();
			assert (msg.sender_id == script.id);
			assert (msg.content.print (false) == "(1337, 'Frida rocks')");

			yield h.unload_agent ();

			h.done ();
		}

		public static uint target_function (int level, string message) {
			var bogus_result = 0;

			for (var i = 0; i != 42; i++)
				bogus_result += i;

			return bogus_result;
		}
	}

	private class Harness : Zed.Test.AsyncHarness {
		private GLib.Module module;
		[CCode (has_target = false)]
		private delegate void AgentMainFunc (string data_string);
		private AgentMainFunc main_impl;
		private string listen_address = "tcp:host=127.0.0.1,port=42042";
		private unowned Thread main_thread;
		private DBusConnection connection;
		private AgentSession session;

		private Gee.LinkedList<ScriptMessage> message_queue = new Gee.LinkedList<ScriptMessage> ();

		public Harness (Zed.Test.AsyncHarness.TestSequenceFunc func) {
			base (func);
		}

		public async AgentSession load_agent () {
			var intermediate_root_dir = Path.get_dirname (Path.get_dirname (Zed.Test.Process.current.filename));
			string agent_filename;
			if (sizeof (void *) == 4)
				agent_filename = Path.build_filename (intermediate_root_dir, "zed-agent-32", "zed-agent-32.dll");
			else
				agent_filename = Path.build_filename (intermediate_root_dir, "zed-agent-64", "zed-agent-64.dll");

			module = GLib.Module.open (agent_filename, 0);
			assert (module != null);

			void * main_func_symbol;
			var main_func_found = module.symbol ("zed_agent_main", out main_func_symbol);
			assert (main_func_found);
			main_impl = (AgentMainFunc) main_func_symbol;

			try {
				main_thread = Thread.create (agent_main_worker, true);
			} catch (ThreadError thread_error) {
				assert_not_reached ();
			}

			for (int i = 0; connection == null; i++) {
				try {
					connection = yield DBusConnection.new_for_address (listen_address, DBusConnectionFlags.AUTHENTICATION_CLIENT);
				} catch (Error conn_error) {
					if (i != 10 - 1) {
						Timeout.add (20, () => {
							load_agent.callback ();
							return false;
						});
						yield;
					} else {
						break;
					}
				}
			}
			assert (connection != null);

			try {
				session = connection.get_proxy_sync (null, ObjectPath.AGENT_SESSION);
			} catch (Error get_proxy_error) {
				assert_not_reached ();
			}

			session.message_from_script.connect ((script_id, msg) => message_queue.add (new ScriptMessage (script_id, msg)));

			return session;
		}

		public async void unload_agent () {
			try {
				yield session.close ();
			} catch (IOError session_error) {
				assert_not_reached ();
			}

			session = null;

			try {
				yield connection.close ();
				connection = null;
			} catch (IOError conn_error) {
				assert_not_reached ();
			}

			main_thread.join ();
			main_thread = null;

			module = null;
		}

		public async ScriptMessage wait_for_message () {
			ScriptMessage msg = null;

			do {
				msg = message_queue.poll ();
				if (msg == null)
					yield process_events ();
			}
			while (msg == null);

			return msg;
		}

		public class ScriptMessage {
			public uint sender_id {
				get;
				private set;
			}

			public Variant content {
				get;
				private set;
			}

			public ScriptMessage (uint sender_id, Variant content) {
				this.sender_id = sender_id;
				this.content = content;
			}
		}

		private void * agent_main_worker () {
			main_impl (listen_address);
			return null;
		}
	}
}
