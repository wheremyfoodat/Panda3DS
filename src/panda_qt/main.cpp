#include <QApplication>
#include <QtWidgets>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    
    window.resize(320, 240);
    window.show();
    window.setWindowTitle("Alber");
    return app.exec();
}