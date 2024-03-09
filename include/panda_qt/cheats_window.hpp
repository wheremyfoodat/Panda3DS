#pragma once

#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>
#include <filesystem>
#include <memory>

#include "emulator.hpp"

class QListWidget;

class CheatsWindow final : public QWidget {
	Q_OBJECT

  public:
	CheatsWindow(Emulator* emu, const std::filesystem::path& path, QWidget* parent = nullptr);
	~CheatsWindow() = default;

  private:
	void addEntry();
	void removeClicked();

	QListWidget* cheatList;
	std::filesystem::path cheatPath;
	Emulator* emu;
};

struct CheatMetadata {
	u32 handle = Cheats::badCheatHandle;
	std::string name = "New cheat";
	std::string code;
	bool enabled = true;
};

class CheatEntryWidget : public QWidget {
	Q_OBJECT

  public:
	CheatEntryWidget(Emulator* emu, CheatMetadata metadata, QListWidget* parent);

	void Update() {
		name->setText(metadata.name.c_str());
		enabled->setChecked(metadata.enabled);
		update();
	}

	void Remove() {
		emu->getCheats().removeCheat(metadata.handle);
		cheatList->takeItem(cheatList->row(listItem));
		deleteLater();
	}

	const CheatMetadata& getMetadata() { return metadata; }
	void setMetadata(const CheatMetadata& metadata) { this->metadata = metadata; }

  private:
	void checkboxChanged(int state);
	void editClicked();

	Emulator* emu;
	CheatMetadata metadata;
	u32 handle;
	QLabel* name;
	QCheckBox* enabled;
	QListWidget* cheatList;
	QListWidgetItem* listItem;
};

class CheatEditDialog : public QDialog {
	Q_OBJECT

  public:
	CheatEditDialog(Emulator* emu, CheatEntryWidget& cheatEntry);

	void accepted();
	void rejected();

  private:
	Emulator* emu;
	CheatEntryWidget& cheatEntry;
	QTextEdit* codeEdit;
	QLineEdit* nameEdit;
};