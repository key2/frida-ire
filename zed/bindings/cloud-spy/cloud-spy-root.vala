namespace CloudSpy {
	public class Root : Object, RootApi {
		public int age {
			get { return 28; }
		}

		public string[] say_hello_to (string name) throws IOError {
			if (name == "Badger")
				throw new IOError.FAILED ("I won't say hello to Mr Badger");
			return { "Hello " + name, "I am a snake" };
		}
	}
}
