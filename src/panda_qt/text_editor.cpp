#include "panda_qt/text_editor.hpp"
#include <QVBoxLayout>

using namespace Zep;

TextEditorWindow::TextEditorWindow(QWidget* parent, const std::string& filename, const std::string& initialText)
	: QDialog(parent), zepWidget(this, qApp->applicationDirPath().toStdString(), fontSize) {
	resize(600, 600);

	// Register our extensions
	ZepRegressExCommand::Register(zepWidget.GetEditor());
	ZepReplExCommand::Register(zepWidget.GetEditor(), &replProvider);

	zepWidget.GetEditor().SetGlobalMode(Zep::ZepMode_Standard::StaticName());
	zepWidget.GetEditor().InitWithText(filename, initialText);
	
	QVBoxLayout* mainLayout = new QVBoxLayout();
	setLayout(mainLayout);
	mainLayout->addWidget(&zepWidget);
}