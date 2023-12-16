#include "panda_qt/text_editor.hpp"

#include <QPushButton>
#include <QVBoxLayout>

#include "panda_qt/main_window.hpp"

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

	QPushButton* button = new QPushButton(tr("Load script"), this);
	button->setFixedSize(100, 20);

	connect(button, &QPushButton::pressed, this, [this]() {
		if (parentWidget()) {
			auto buffer = zepWidget.GetEditor().GetMRUBuffer();
			const std::string text = buffer->GetBufferText(buffer->Begin(), buffer->End());

			static_cast<MainWindow*>(parentWidget())->loadLuaScript(text);
		} else {
			// This should be unreachable, only here for safety purposes
			printf("Text editor does not have any parent widget, click doesn't work :(\n");
		}
	});

	mainLayout->addWidget(button);
	mainLayout->addWidget(&zepWidget);
}