namespace Zed {
	public class HostSessionService : Object {
		private Gee.ArrayList<HostSessionBackend> backends = new Gee.ArrayList<HostSessionBackend> ();

		public signal void provider_available (HostSessionProvider provider);
		public signal void provider_unavailable (HostSessionProvider provider);

		public HostSessionService.with_default_backends () {
#if WINDOWS
			add_backend (new WindowsHostSessionBackend ());
			add_backend (new FruityHostSessionBackend ());
#endif
			add_backend (new TcpHostSessionBackend ());
		}

		public HostSessionService.with_local_backend_only () {
#if WINDOWS
			add_backend (new WindowsHostSessionBackend ());
#endif
		}

		public async void start () {
			foreach (var backend in backends)
				yield backend.start ();
		}

		public async void stop () {
			foreach (var backend in backends)
				yield backend.stop ();
		}

		public void add_backend (HostSessionBackend backend) {
			backends.add (backend);
			backend.provider_available.connect ((provider) => {
				provider_available (provider);
			});
			backend.provider_unavailable.connect ((provider) => {
				provider_unavailable (provider);
			});
		}

		public void remove_backend (HostSessionBackend backend) {
			backends.remove (backend);
		}
	}

	public interface HostSessionProvider : Object {
		public abstract string name {
			get;
		}

		public abstract ImageData? icon {
			get;
		}

		public abstract HostSessionProviderKind kind {
			get;
		}

		public abstract async HostSession create () throws IOError;

		public abstract async AgentSession obtain_agent_session (AgentSessionId id) throws IOError;
		public signal void agent_session_closed (AgentSessionId id, Error? error);
	}

	public enum HostSessionProviderKind {
		LOCAL_SYSTEM,
		LOCAL_TETHER,
		REMOTE_SYSTEM
	}

	public interface HostSessionBackend : Object {
		public signal void provider_available (HostSessionProvider provider);
		public signal void provider_unavailable (HostSessionProvider provider);

		public abstract async void start ();
		public abstract async void stop ();
	}
}
