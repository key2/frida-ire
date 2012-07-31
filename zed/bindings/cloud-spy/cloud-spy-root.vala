namespace CloudSpy {
	public class Root : Object, RootApi {
#if !WINDOWS
		private Server server = null;

		private Zed.TcpHostSessionProvider provider;
#else
		private Zed.WindowsHostSessionProvider provider;
#endif
		private Zed.HostSession host_session;

		private Gee.HashMap<uint, uint> process_id_by_agent_session_id = new Gee.HashMap<uint, uint> ();
		private Gee.HashMap<uint, Zed.AgentSession> agent_session_by_process_id = new Gee.HashMap<uint, Zed.AgentSession> ();
		private Gee.HashMap<uint, uint> script_id_by_process_id = new Gee.HashMap<uint, uint> ();

		protected override async void destroy () {
			var agent_sessions = agent_session_by_process_id.values.to_array ();
			foreach (var agent_session in agent_sessions) {
				try {
					yield agent_session.close ();
				} catch (IOError e) {
				}
			}

			if (provider != null) {
				yield provider.close ();
				provider = null;
			}
			host_session = null;

			process_id_by_agent_session_id.clear ();
			agent_session_by_process_id.clear ();
			script_id_by_process_id.clear ();

#if !WINDOWS
			if (server != null) {
				server.destroy ();
				server = null;
			}
#endif
		}

		public async string enumerate_processes () throws IOError {
			var host_session = yield obtain_host_session ();

			var builder = new Json.Builder ();
			builder.begin_array ();
			foreach (var pi in yield host_session.enumerate_processes ()) {
				builder.begin_object ();
				builder.set_member_name ("pid").add_int_value (pi.pid);
				builder.set_member_name ("name").add_string_value (pi.name);
				builder.end_object ();
			}
			builder.end_array ();
			var generator = new Json.Generator ();
			generator.set_root (builder.get_root ());
			return generator.to_data (null);
		}

		public async void attach_to (uint pid, string source) throws IOError {
			var agent_session = yield obtain_agent_session (pid);
			var script_id = yield agent_session.create_script (source);
			yield agent_session.load_script (script_id);

			if (script_id_by_process_id.has_key (pid)) {
				try {
					yield detach_from (pid);
				} catch (IOError e) {
				}
			}

			script_id_by_process_id[pid] = script_id.handle;
		}

		public async void detach_from (uint pid) throws IOError {
			uint script_id;
			if (!script_id_by_process_id.unset (pid, out script_id))
				throw new IOError.FAILED ("no script associated with pid %u".printf (pid));

			var agent_session = yield obtain_agent_session (pid);
			yield agent_session.destroy_script (Zed.AgentScriptId (script_id));
		}

		protected async Zed.HostSession obtain_host_session () throws IOError {
			if (host_session == null) {
#if !WINDOWS
				if (server == null)
					server = new Server ();
				provider = new Zed.TcpHostSessionProvider.for_address (server.address);
#else
				provider = new Zed.WindowsHostSessionProvider ();
#endif
				provider.agent_session_closed.connect ((sid, error) => {
					uint pid;
					if (process_id_by_agent_session_id.unset (sid.handle, out pid)) {
						agent_session_by_process_id.unset (pid);
						script_id_by_process_id.unset (pid);

						detach (pid);
					}
				});
				host_session = yield provider.create ();
			}

			return host_session;
		}

		protected async Zed.AgentSession obtain_agent_session (uint pid) throws IOError {
			yield obtain_host_session ();

			var agent_session = agent_session_by_process_id[pid];
			if (agent_session == null) {
				var agent_session_id = yield host_session.attach_to (pid);
				agent_session = yield provider.obtain_agent_session (agent_session_id);
				agent_session.message_from_script.connect ((sid, msg) => {
					message (pid, msg);
				});
				process_id_by_agent_session_id[agent_session_id.handle] = pid;
				agent_session_by_process_id[pid] = agent_session;
			}

			return agent_session;
		}

#if !WINDOWS
		protected class Server {
			private TemporaryFile executable;

			public string address {
				get;
				private set;
			}

			private const string SERVER_ADDRESS_TEMPLATE = "tcp:host=127.0.0.1,port=%u";

			public Server () throws IOError {
				var blob = CloudSpy.Data.get_zed_server_blob ();
				executable = new TemporaryFile.from_stream ("server", new MemoryInputStream.from_data (blob.data, null));
				try {
					executable.file.set_attribute_uint32 (FILE_ATTRIBUTE_UNIX_MODE, 0755, FileQueryInfoFlags.NONE);
				} catch (Error e) {
					throw new IOError.FAILED (e.message);
				}

				address = SERVER_ADDRESS_TEMPLATE.printf (get_available_port ());

				try {
					string[] argv = new string[] { executable.file.get_path (), address };
					Pid child_pid;
					Process.spawn_async (null, argv, null, 0, null, out child_pid);
				} catch (SpawnError e) {
					executable.destroy ();
					throw new IOError.FAILED (e.message);
				}
			}

			public void destroy () {
				executable.destroy ();
			}

			private uint get_available_port () {
				uint port = 27042;

				bool found_available = false;
				var loopback = new InetAddress.loopback (SocketFamily.IPV4);
				var address_in_use = new IOError.ADDRESS_IN_USE ("");
				while (!found_available) {
					try {
						var socket = new Socket (SocketFamily.IPV4, SocketType.STREAM, SocketProtocol.TCP);
						socket.bind (new InetSocketAddress (loopback, (uint16) port), false);
						socket.close ();
						found_available = true;
					} catch (Error e) {
						if (e.code == address_in_use.code)
							port--;
						else
							found_available = true;
					}
				}

				return port;
			}
		}

		protected class TemporaryFile {
			public File file {
				get;
				private set;
			}

			public TemporaryFile.from_stream (string name, InputStream istream) throws IOError {
				this.file = File.new_for_path (Path.build_filename (Environment.get_tmp_dir (), "cloud-spy-%p-%u-%s".printf (this, Random.next_int (), name)));

				try {
					var ostream = file.create (FileCreateFlags.NONE, null);

					var buf_size = 128 * 1024;
					var buf = new uint8[buf_size];

					while (true) {
						var bytes_read = istream.read (buf);
						if (bytes_read == 0)
							break;
						buf.resize ((int) bytes_read);

						size_t bytes_written;
						ostream.write_all (buf, out bytes_written);
					}

					ostream.close (null);
				} catch (Error e) {
					throw new IOError.FAILED (e.message);
				}
			}

			~TemporaryFile () {
				destroy ();
			}

			public void destroy () {
				try {
					file.delete (null);
				} catch (Error e) {
				}
			}
		}
#endif
	}
}
