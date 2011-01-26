namespace CloudSpy {
	public class Root : Object, RootApi {
		public int age {
			get { return 28; }
		}

		public int foo () throws IOError {
			return 1337;
		}
	}
}
