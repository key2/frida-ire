public class SessionManager : Object {
	public MainContext main_context {
		get;
		private set;
	}

	private bool closed = false;
#if !WINDOWS
	private Server server;

	private Zed.TcpHostSessionProvider local_provider;
#else
	private Zed.WindowsHostSessionProvider local_provider;
#endif
	private Zed.HostSession local_session;

	private Gee.HashMap<uint, Session> session_by_pid = new Gee.HashMap<uint, Session> ();
	private Gee.HashMap<uint, Session> session_by_handle = new Gee.HashMap<uint, Session> ();

	public SessionManager (MainContext main_context) {
		this.main_context = main_context;
	}

	public override void dispose () {
		try {
			(create<CloseTask> () as CloseTask).start_and_wait_for_completion ();
		} catch (Error e) {
			assert_not_reached ();
		}

		base.dispose ();
	}

	public Session obtain_session_for (uint pid) throws Error {
		var task = create<ObtainSessionTask> () as ObtainSessionTask;
		task.pid = pid;
		return task.start_and_wait_for_completion ();
	}

	public void _release_session (Session session) {
		var session_did_exist = session_by_pid.unset (session.pid);
		assert (session_did_exist);

		uint handle = 0;
		foreach (var entry in session_by_handle.entries) {
			if (entry.value == session)
				handle = entry.key;
		}
		assert (handle != 0);
		session_by_handle.unset (handle);
	}

	private Object create<T> () {
		return Object.new (typeof (T), main_context: main_context, parent: this);
	}

	private class CloseTask : ManagerTask<void> {
		protected override void validate_operation () throws Error {
		}

		protected override async void perform_operation () throws Error {
			if (parent.closed)
				return;
			parent.closed = true;

			if (parent.local_session == null)
				return;

			foreach (var session in parent.session_by_pid.values.to_array ())
				yield session._do_close (true);
			parent.session_by_pid = null;
			parent.session_by_handle = null;

			parent.local_session = null;
			yield parent.local_provider.close ();
			parent.local_provider = null;
#if !WINDOWS
			parent.server.destroy ();
			parent.server = null;
#endif

		}
	}

	private class ObtainSessionTask : ManagerTask<Session> {
		public uint pid;

		protected override async Session perform_operation () throws Error {
			var session = parent.session_by_pid[pid];
			if (session == null) {
				yield parent.ensure_host_session_is_available ();

				var agent_session_id = yield parent.local_session.attach_to (pid);
				var agent_session = yield parent.local_provider.obtain_agent_session (agent_session_id);
				session = new Session (parent, pid, agent_session);
				parent.session_by_pid[pid] = session;
				parent.session_by_handle[agent_session_id.handle] = session;
			}

			return session;
		}
	}

	private abstract class ManagerTask<T> : AsyncTask<T> {
		public weak SessionManager parent {
			get;
			construct;
		}

		protected override void validate_operation () throws Error {
			if (parent.closed)
				throw new IOError.FAILED ("invalid operation (manager is closed)");
		}
	}

	protected async Zed.HostSession ensure_host_session_is_available () throws IOError {
		if (local_session == null) {
#if !WINDOWS
			server = new Server ();
			local_provider = new Zed.TcpHostSessionProvider.for_address (server.address);
#else
			local_provider = new Zed.WindowsHostSessionProvider ();
#endif
			local_provider.agent_session_closed.connect (on_agent_session_closed);

			local_session = yield local_provider.create ();
		}

		return local_session;
	}

	private void on_agent_session_closed (Zed.AgentSessionId id, Error? error) {
		var session = session_by_handle[id.handle];
		if (session != null)
			session._do_close (false);
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
			var blob = PyFrida.Data.get_zed_server_blob ();
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

public class Session : Object {
	private weak SessionManager manager;

	public uint pid {
		get;
		private set;
	}

	public Zed.AgentSession internal_session {
		get;
		private set;
	}

	public MainContext main_context {
		get;
		private set;
	}

	private Gee.HashMap<uint, Script> script_by_id = new Gee.HashMap<uint, Script> ();

	public signal void closed ();

	public Session (SessionManager manager, uint pid, Zed.AgentSession agent_session) {
		this.manager = manager;
		this.pid = pid;
		this.internal_session = agent_session;
		this.main_context = manager.main_context;

		internal_session.message_from_script.connect (on_message_from_script);
	}

	public void close () {
		try {
			(create<CloseTask> () as CloseTask).start_and_wait_for_completion ();
		} catch (Error e) {
			assert_not_reached ();
		}
	}

	public Script create_script (string source) throws Error {
		var task = create<CreateScriptTask> () as CreateScriptTask;
		task.source = source;
		return task.start_and_wait_for_completion ();
	}

	private void on_message_from_script (Zed.AgentScriptId sid, string msg) {
		var script = script_by_id[sid.handle];
		if (script != null)
			script.message (msg);
	}

	public void _release_script (Zed.AgentScriptId sid) {
		var script_did_exist = script_by_id.unset (sid.handle);
		assert (script_did_exist);
	}

	private Object create<T> () {
		return Object.new (typeof (T), main_context: main_context, parent: this);
	}

	private class CloseTask : SessionTask<void> {
		protected override void validate_operation () throws Error {
		}

		protected override async void perform_operation () throws Error {
			yield parent._do_close (true);
		}
	}

	public async void _do_close (bool may_block) {
		if (manager == null)
			return;

		manager._release_session (this);
		manager = null;

		foreach (var script in script_by_id.values.to_array ())
			yield script._do_unload (may_block);

		if (may_block) {
			try {
				yield internal_session.close ();
			} catch (IOError ignored_error) {
			}
		}
		internal_session = null;

		closed ();
	}

	private class CreateScriptTask : SessionTask<Script> {
		public string source;

		protected override async Script perform_operation () throws Error {
			var sid = yield parent.internal_session.create_script (source);
			var script = new Script (parent, sid);
			parent.script_by_id[sid.handle] = script;
			return script;
		}
	}

	private abstract class SessionTask<T> : AsyncTask<T> {
		public weak Session parent {
			get;
			construct;
		}

		protected override void validate_operation () throws Error {
			if (parent.manager == null)
				throw new IOError.FAILED ("invalid operation (session is closed)");
		}
	}
}

public class Script : Object {
	private weak Session session;

	private Zed.AgentScriptId script_id;

	private MainContext main_context;

	public signal void message (string msg);

	public Script (Session session, Zed.AgentScriptId script_id) {
		this.session = session;
		this.script_id = script_id;
		this.main_context = session.main_context;
	}

	public void load () throws Error {
		(create<LoadTask> () as LoadTask).start_and_wait_for_completion ();
	}

	public void unload () throws Error {
		(create<UnloadTask> () as UnloadTask).start_and_wait_for_completion ();
	}

	public void post_message (string msg) throws Error {
		var task = create<PostMessageTask> () as PostMessageTask;
		task.msg = msg;
		task.start_and_wait_for_completion ();
	}

	private Object create<T> () {
		return Object.new (typeof (T), main_context: main_context, parent: this);
	}

	private class LoadTask : ScriptTask<void> {
		protected override async void perform_operation () throws Error {
			yield parent.session.internal_session.load_script (parent.script_id);
		}
	}

	private class UnloadTask : ScriptTask<void> {
		protected override async void perform_operation () throws Error {
			yield parent._do_unload (true);
		}
	}

	private class PostMessageTask : ScriptTask<void> {
		public string msg;

		protected override async void perform_operation () throws Error {
			yield parent.session.internal_session.post_message_to_script (parent.script_id, msg);
		}
	}

	public async void _do_unload (bool may_block) {
		var s = session;
		session = null;

		var sid = script_id;

		s._release_script (sid);

		if (may_block) {
			try {
				yield s.internal_session.destroy_script (sid);
			} catch (IOError ignored_error) {
			}
		}
	}

	private abstract class ScriptTask<T> : AsyncTask<T> {
		public weak Script parent {
			get;
			construct;
		}

		protected override void validate_operation () throws Error {
			if (parent.session == null)
				throw new IOError.FAILED ("invalid operation (script is destroyed)");
		}
	}
}

private abstract class AsyncTask<T> : Object {
	public MainContext main_context {
		get;
		construct;
	}

	private bool completed;
	private Mutex mutex = new Mutex ();
	private Cond cond = new Cond ();

	private T result;
	private Error error;

	public T start_and_wait_for_completion () throws Error {
		var source = new IdleSource ();
		source.set_callback (() => {
			do_perform_operation ();
			return false;
		});
		source.attach (main_context);

		mutex.lock ();
		while (!completed)
			cond.wait (mutex);
		mutex.unlock ();

		if (error != null)
			throw error;

		return result;
	}

	private async void do_perform_operation () {
		try {
			validate_operation ();
			result = yield perform_operation ();
		} catch (Error e) {
			error = new IOError.FAILED (e.message);
		}

		mutex.lock ();
		completed = true;
		cond.signal ();
		mutex.unlock ();
	}

	protected abstract void validate_operation () throws Error;
	protected abstract async T perform_operation () throws Error;
}
