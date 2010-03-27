using Gtk;

int main (string[] args) {
	Gtk.init (ref args);

	var window = new Window (WindowType.TOPLEVEL);
	window.title = "First GTK+ Program";
	window.set_default_size (300, 50);
	window.position = WindowPosition.CENTER;
	window.destroy.connect (Gtk.main_quit);

	var button = new Button.with_label ("Click me!");
	button.clicked.connect ((source) => {
		source.label = "Thank you";
	});

	window.add (button);
	window.show_all ();

	Gtk.main ();
	return 0;
}
