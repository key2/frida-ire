namespace CloudSpy {
	[DBus (name = "com.appspot.cloud-spy.RootApi")]
	public interface RootApi : Object {
		public abstract string[] say_hello_to (string name) throws IOError;
	}

	public class Dispatcher : GLib.Object {
		protected unowned CloudSpy.Object target_object;
		protected DBusMethodInfo ** methods;
		protected DBusInterfaceMethodCallFunc dispatch_func;

		public Dispatcher.for_object (CloudSpy.Object obj) {
			init_with_object (obj);
		}

		private extern void init_with_object (CloudSpy.Object obj);

		public bool has_method (string name) {
			return find_method_by_name (name) != null;
		}

		public Variant? invoke (string name, Variant? args) throws IOError {
			var method = find_method_by_name (name);
			if (method == null)
				throw new IOError.NOT_FOUND ("no such method");

			/* FIXME: argument validation */

			var parameters_in = args;
			if (parameters_in == null)
				parameters_in = new Variant.int32 (42);

			var parameters_out = do_invoke (method, parameters_in);
			if (parameters_out == null)
				return null;
			assert (parameters_out.n_children () == 1);
			return parameters_out.get_child_value (0);
		}

		private extern Variant? do_invoke (DBusMethodInfo * method, Variant parameters) throws IOError;

		private DBusMethodInfo * find_method_by_name (string name) {
			if (name.length == 0)
				return null;

			var dbus_name = new StringBuilder ();
			dbus_name.append_unichar (name[0].toupper ());
			dbus_name.append (name.substring (1));

			for (DBusMethodInfo ** method = methods; *method != null; method++) {
				if ((*method)->name == dbus_name.str)
					return *method;
			}

			return null;
		}
	}
}
