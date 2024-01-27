#include "panda_qt/cheats_window.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTimer>
#include <functional>

#include "cheats.hpp"
#include "emulator.hpp"
#include "panda_qt/main_window.hpp"

MainWindow* mainWindow = nullptr;

struct CheatMetadata {
	u32 handle = badCheatHandle;
	std::string name = "New cheat";
	std::string code;
	bool enabled = true;
};

void dispatchToMainThread(std::function<void()> callback)
{
    QTimer* timer = new QTimer();
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [=]()
    {
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}

class CheatEntryWidget : public QWidget {
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

	const CheatMetadata& GetMetadata() { return metadata; }

	void SetMetadata(const CheatMetadata& metadata) { this->metadata = metadata; }

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

CheatEntryWidget::CheatEntryWidget(Emulator* emu, CheatMetadata metadata, QListWidget* parent)
	: QWidget(), emu(emu), metadata(metadata), cheatList(parent) {
	QHBoxLayout* layout = new QHBoxLayout;

	enabled = new QCheckBox;
	enabled->setChecked(metadata.enabled);

	name = new QLabel(metadata.name.c_str());
	QPushButton* buttonEdit = new QPushButton(tr("Edit"));

	connect(enabled, &QCheckBox::stateChanged, this, &CheatEntryWidget::checkboxChanged);
	connect(buttonEdit, &QPushButton::clicked, this, &CheatEntryWidget::editClicked);

	layout->addWidget(enabled);
	layout->addWidget(name);
	layout->addWidget(buttonEdit);
	setLayout(layout);

	listItem = new QListWidgetItem;
	listItem->setSizeHint(sizeHint());
	parent->addItem(listItem);
	parent->setItemWidget(listItem, this);
}

void CheatEntryWidget::checkboxChanged(int state) {
	bool enabled = state == Qt::Checked;
	if (metadata.handle == badCheatHandle) {
		printf("Cheat handle is bad, this shouldn't happen\n");
		return;
	}

	if (enabled) {
		emu->getCheats().enableCheat(metadata.handle);
		metadata.enabled = true;
	} else {
		emu->getCheats().disableCheat(metadata.handle);
		metadata.enabled = false;
	}
}

void CheatEntryWidget::editClicked() {
	CheatEditDialog* dialog = new CheatEditDialog(emu, *this);
	dialog->show();
}

CheatEditDialog::CheatEditDialog(Emulator* emu, CheatEntryWidget& cheatEntry) : QDialog(), emu(emu), cheatEntry(cheatEntry) {
	setAttribute(Qt::WA_DeleteOnClose);
	setModal(true);

	QVBoxLayout* layout = new QVBoxLayout;
	const CheatMetadata& metadata = cheatEntry.GetMetadata();
	codeEdit = new QTextEdit;
	nameEdit = new QLineEdit;
	nameEdit->setText(metadata.name.c_str());
	nameEdit->setPlaceholderText(tr("Cheat name"));
	layout->addWidget(nameEdit);

	QFont font;
	font.setFamily("Courier");
	font.setFixedPitch(true);
	font.setPointSize(10);
	codeEdit->setFont(font);

	if (metadata.code.size() != 0) {
		// Nicely format it like so:
		// 01234567 89ABCDEF
		// 01234567 89ABCDEF
		std::string formattedCode;
		for (size_t i = 0; i < metadata.code.size(); i += 2) {
			if (i != 0) {
				if (i % 8 == 0 && i % 16 != 0) {
					formattedCode += " ";
				} else if (i % 16 == 0) {
					formattedCode += "\n";
				}
			}

			formattedCode += metadata.code[i];
			formattedCode += metadata.code[i + 1];
		}
		codeEdit->setText(formattedCode.c_str());
	}

	layout->addWidget(codeEdit);
	setLayout(layout);

	auto buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
	QDialogButtonBox* button_box = new QDialogButtonBox(buttons);
	layout->addWidget(button_box);

	connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this, &QDialog::rejected, this, &CheatEditDialog::rejected);
	connect(this, &QDialog::accepted, this, &CheatEditDialog::accepted);
}

void CheatEditDialog::accepted() {
	QString code = codeEdit->toPlainText();
	code.replace(QRegularExpression("[^0-9a-fA-F]"), "");

	CheatMetadata metadata = cheatEntry.GetMetadata();
	metadata.name = nameEdit->text().toStdString();
	metadata.code = code.toStdString();

	std::vector<uint8_t> bytes;
	for (size_t i = 0; i < metadata.code.size(); i += 2) {
		std::string hex = metadata.code.substr(i, 2);
		bytes.push_back((uint8_t)std::stoul(hex, nullptr, 16));
	}

	mainWindow->editCheat(cheatEntry.GetMetadata().handle, bytes, [this](u32 handle) {
        dispatchToMainThread([this, handle]() {
            if (handle == badCheatHandle) {
                cheatEntry.Remove();
                return;
            } else {
                CheatMetadata metadata = cheatEntry.GetMetadata();
                metadata.handle = handle;
                cheatEntry.SetMetadata(metadata);
                cheatEntry.Update();
            }
        });
	});
}

void CheatEditDialog::rejected() {
	bool isEditing = cheatEntry.GetMetadata().handle != badCheatHandle;
	if (!isEditing) {
		// Was adding a cheat but pressed cancel
		cheatEntry.Remove();
	}
}

CheatsWindow::CheatsWindow(Emulator* emu, const std::filesystem::path& cheatPath, QWidget* parent)
	: QWidget(parent, Qt::Window), emu(emu), cheatPath(cheatPath) {
    mainWindow = static_cast<MainWindow*>(parent);

	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(6, 6, 6, 6);
	setLayout(layout);

	cheatList = new QListWidget;
	layout->addWidget(cheatList);

	QWidget* buttonBox = new QWidget;
	QHBoxLayout* buttonLayout = new QHBoxLayout;

	QPushButton* buttonAdd = new QPushButton(tr("Add"));
	QPushButton* buttonRemove = new QPushButton(tr("Remove"));

	connect(buttonAdd, &QPushButton::clicked, this, &CheatsWindow::addEntry);
	connect(buttonRemove, &QPushButton::clicked, this, &CheatsWindow::removeClicked);

	buttonLayout->addWidget(buttonAdd);
	buttonLayout->addWidget(buttonRemove);
	buttonBox->setLayout(buttonLayout);

	layout->addWidget(buttonBox);

	// TODO: load cheats from saved cheats per game
	// for (const CheatMetadata& metadata : getSavedCheats())
	// {
	//     new CheatEntryWidget(emu, metadata, cheatList);
	// }
}

void CheatsWindow::addEntry() {
	// CheatEntryWidget is added to the list when it's created
	CheatEntryWidget* entry = new CheatEntryWidget(emu, {badCheatHandle, "New cheat", "", true}, cheatList);
	CheatEditDialog* dialog = new CheatEditDialog(emu, *entry);
	dialog->show();
}

void CheatsWindow::removeClicked() {
	QListWidgetItem* item = cheatList->currentItem();
	if (item == nullptr) {
		return;
	}

	CheatEntryWidget* entry = static_cast<CheatEntryWidget*>(cheatList->itemWidget(item));
	entry->Remove();
}
