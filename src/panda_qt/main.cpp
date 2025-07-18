#include <QApplication>

#include "panda_qt/main_window.hpp"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	MainWindow window(&app);

	return app.exec();
}
