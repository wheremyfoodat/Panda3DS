#include <QApplication>
#include <QtWidgets>

#include "panda_qt/screen.hpp"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QWidget window;

	window.resize(320, 240);
	window.show();
	window.setWindowTitle("Alber");
	ScreenWidget screen(&window);
	screen.show();

	return app.exec();
}