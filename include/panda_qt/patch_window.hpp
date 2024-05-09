#pragma once
#include <QLabel>
#include <QMessageBox>
#include <QWidget>
#include <filesystem>

#include "panda_qt/elided_label.hpp"

class PatchWindow final : public QWidget {
	Q_OBJECT

  public:
	PatchWindow(QWidget* parent = nullptr);
	~PatchWindow() = default;

  private:
	// Show a message box
	// Title: Title of the message box to display
	// Message: Message to display
	// Icon: The type of icon (error, warning, information, etc) to display
	// IconPath: If non-null, then a path to an icon in our assets to display on the OK button
	void displayMessage(
		const QString& title, const QString& message, QMessageBox::Icon icon = QMessageBox::Icon::Warning, const char* iconPath = nullptr
	);

	std::filesystem::path inputPath = "";
	std::filesystem::path patchPath = "";

	ElidedLabel* inputPathLabel = nullptr;
	ElidedLabel* patchPathLabel = nullptr;
};
