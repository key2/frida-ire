namespace CloudSpy {
	public class Root : Object, RootApi {
		public int age {
			get { return 28; }
		}

		public string[] foo () throws IOError {
			return { "I am a badger", "I am a snake" };
		}
	}
}
