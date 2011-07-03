namespace CloudSpy {
	public class Root : Object, RootApi {
		private Zed.HostSessionService service = new Zed.HostSessionService.with_local_backend_only ();
		private Zed.HostSessionProvider local_provider;
		private Zed.HostSession local_session;

		private Gee.HashMap<uint, Zed.AgentSession> agent_session_by_process_id = new Gee.HashMap<uint, Zed.AgentSession> ();
		private Gee.HashMap<uint, uint> script_id_by_process_id = new Gee.HashMap<uint, uint> ();

		public int attach_to (string process_name, string text) throws IOError {
			var resolve = new ResolvePidTask (this, process_name);
			uint pid = resolve.wait_for_completion ();
			if (pid == 0)
				return -1;

			var attach = new AttachTask (this, pid, text);
			uint script_id = attach.wait_for_completion ();
			if (script_id == 0)
				return -2;

			script_id_by_process_id[pid] = script_id;

			return (int) pid;
		}

		public int detach_from (string process_name) throws IOError {
			var resolve = new ResolvePidTask (this, process_name);
			uint pid = resolve.wait_for_completion ();
			if (pid == 0)
				return -1;

			var script_id = script_id_by_process_id[pid];
			if (script_id == 0)
				return -2;

			var detach = new DetachTask (this, pid, script_id);
			detach.wait_for_completion ();

			return (int) pid;
		}

		protected async Zed.HostSession obtain_host_session () throws IOError {
			if (local_session == null) {
				service = new Zed.HostSessionService.with_local_backend_only ();

				service.provider_available.connect ((p) => {
					local_provider = p;
				});
				yield service.start ();

				/* HACK */
				while (local_provider == null) {
					var timeout = new TimeoutSource (10);
					timeout.set_callback (() => {
						obtain_host_session.callback ();
						return false;
					});
					timeout.attach (MainContext.get_thread_default ());
					yield;
				}

				local_session = yield local_provider.create ();
			}

			return local_session;
		}

		protected async Zed.AgentSession obtain_agent_session (uint pid) throws IOError {
			yield obtain_host_session ();

			var agent_session = agent_session_by_process_id[pid];
			if (agent_session == null) {
				var agent_session_id = yield local_session.attach_to (pid);
				agent_session = yield local_provider.obtain_agent_session (agent_session_id);
				agent_session.message_from_script.connect ((sid, msg) => {
					stdout.printf ("message from script %u:\n\t%s\n", sid.handle, msg);
				});
				agent_session_by_process_id[pid] = agent_session;
			}

			return agent_session;
		}

		private class ResolvePidTask : AsyncTask<uint> {
			private string process_name;

			public ResolvePidTask (Root parent, string process_name) {
				base (parent);

				this.process_name = process_name;
			}

			protected override async void perform_operation () {
				try {
					var host_session = yield parent.obtain_host_session ();

					uint pid = 0, match_count = 0;

					var spec = new PatternSpec (process_name);
					foreach (var pi in yield host_session.enumerate_processes ()) {
						if (spec.match_string (pi.name)) {
							pid = pi.pid;
							match_count++;
						}
					}

					if (match_count > 1)
						pid = 0;

					success (pid);

				} catch (Error e) {
					failure (new IOError.FAILED (e.message));
				}
			}
		}

		private class AttachTask : AsyncTask<uint> {
			private uint pid;
			private string text;

			public AttachTask (Root parent, uint pid, string text) {
				base (parent);

				this.pid = pid;
				this.text = text;
			}

			protected override async void perform_operation () {
				try {
					var agent_session = yield parent.obtain_agent_session (pid);

					var script_id = yield agent_session.load_script (
						"var replacementText = '" + text + "';" +
						"var replacementTextUtf16 = Memory.allocUtf16String(replacementText);" +
						"" +
						"var drawTextImpl = Process.findModuleExportByName('user32.dll', 'DrawTextExW');" +
						"Interceptor.attach(drawTextImpl, {" +
						" onEnter: function(args) {" +
						"	args[1] = replacementTextUtf16;" +
						"	args[2] = replacementText.length;" +
						" }" +
						"});");

					success (script_id.handle);
				} catch (Error e) {
					failure (new IOError.FAILED (e.message));
				}
			}
		}

		private class DetachTask : AsyncTask<uint> {
			private uint pid;
			private uint script_id;

			public DetachTask (Root parent, uint pid, uint script_id) {
				base (parent);

				this.pid = pid;
				this.script_id = script_id;
			}

			protected override async void perform_operation () {
				try {
					var agent_session = yield parent.obtain_agent_session (pid);
					yield agent_session.unload_script (Zed.AgentScriptId (script_id));
					success (0);
				} catch (Error e) {
					failure (new IOError.FAILED (e.message));
				}
			}
		}

		private abstract class AsyncTask<T> : GLib.Object {
			public Root parent {
				get;
				construct;
			}

			private T result;
			private Error error;

			private MainLoop loop;

			public AsyncTask (Root parent) {
				GLib.Object (parent: parent);

				this.loop = new MainLoop (MainContext.get_thread_default (), true);

				start ();
			}

			private void start () {
				var idle = new IdleSource ();
				idle.set_callback (() => {
					perform_operation ();
					return false;
				});
				idle.attach (MainContext.get_thread_default ());
			}

			public T wait_for_completion () throws IOError {
				loop.run ();
				if (error != null)
					throw new IOError.FAILED (error.message);
				return result;
			}

			public void success (T result) {
				this.result = result;
				loop.quit ();
			}

			public void failure (Error error) {
				this.error = error;
				loop.quit ();
			}

			protected abstract async void perform_operation ();
		}
	}
}
