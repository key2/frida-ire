namespace CloudSpy {
	public class Root : Object, RootApi {
		public int attach_to (string process_name) throws IOError {
			var ctx = new AttachContext (this, process_name);
			ctx.start ();
			return ctx.wait_for_completion ();
		}

		private class AttachContext : GLib.Object {
			private MainLoop loop;

			private string process_name;
			private int result;
			private Error error;

			public AttachContext (Object parent, string process_name) {
				this.loop = new MainLoop (MainContext.get_thread_default (), true);

				this.process_name = process_name;
			}

			public void start () {
				var idle = new IdleSource ();
				idle.set_callback (() => {
					do_attach ();
					return false;
				});
				idle.attach (MainContext.get_thread_default ());
			}

			public int wait_for_completion () throws IOError {
				loop.run ();
				if (error != null)
					throw new IOError.FAILED (error.message);
				return result;
			}

			public void success (int result) {
				this.result = result;
				loop.quit ();
			}

			public void failure (Error error) {
				this.error = error;
				loop.quit ();
			}

			private async void do_attach () {
				try {
					var service = new Zed.HostSessionService.with_local_backend_only ();

					Zed.HostSessionProvider provider = null;
					service.provider_available.connect ((p) => {
						provider = p;
					});
					yield service.start ();
					/* HACK */
					while (provider == null) {
						var timeout = new TimeoutSource (10);
						timeout.set_callback (() => do_attach.callback ());
						timeout.attach (loop.get_context ());
						yield;
					}

					var host_session = yield provider.create ();

					int pid = -1;

					var spec = new PatternSpec (process_name);
					foreach (var pi in yield host_session.enumerate_processes ()) {
						if (spec.match_string (pi.name))
							pid = (int) pi.pid;
					}

					yield service.stop ();

					success (pid);
				} catch (Error e) {
					failure (new IOError.FAILED (e.message));
				}
			}
		}
	}
}
