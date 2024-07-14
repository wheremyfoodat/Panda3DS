#include <QPushButton>
#include <QVBoxLayout>

#include "panda_qt/main_window.hpp"
#include "panda_qt/shader_editor.hpp"

using namespace Zep;

ShaderEditorWindow::ShaderEditorWindow(QWidget* parent, const std::string& filename, const std::string& initialText)
	: QDialog(parent), zepWidget(this, qApp->applicationDirPath().toStdString(), fontSize) {
	resize(600, 600);

	// Register our extensions
	ZepRegressExCommand::Register(zepWidget.GetEditor());
	ZepReplExCommand::Register(zepWidget.GetEditor(), &replProvider);

	// Default to standard mode instead of vim mode, initialize text box
	zepWidget.GetEditor().InitWithText(filename, initialText);
	zepWidget.GetEditor().SetGlobalMode(Zep::ZepMode_Standard::StaticName());

	// Layout for widgets
	QVBoxLayout* mainLayout = new QVBoxLayout();
	setLayout(mainLayout);

	QPushButton* button = new QPushButton(tr("Reload shader"), this);
	button->setFixedSize(100, 20);

	// When the Load Script button is pressed, send the current text to the MainWindow, which will upload it to the emulator's lua object
	connect(button, &QPushButton::pressed, this, [this]() {
		if (parentWidget()) {
			auto buffer = zepWidget.GetEditor().GetMRUBuffer();
			const std::string text = buffer->GetBufferText(buffer->Begin(), buffer->End());

			static_cast<MainWindow*>(parentWidget())->reloadShader(text);
		} else {
			// This should be unreachable, only here for safety purposes
			printf("Text editor does not have any parent widget, click doesn't work :(\n");
		}
	});

	mainLayout->addWidget(button);
	mainLayout->addWidget(&zepWidget);
}

void ShaderEditorWindow::setEnable(bool enable) {
	supported = enable;

	if (enable) {
		setDisabled(false);
	} else {
		setDisabled(true);
		setText("Shader editor window is not available for this renderer backend");
	}
}
