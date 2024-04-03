#include <QApplication>

#include "panda_qt/main_window.hpp"
#include "panda_qt/screen.hpp"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	MainWindow window(&app);

	window.show();
	return app.exec();
}
