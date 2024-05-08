#include "panda_qt/patch_window.hpp"

#include <QAbstractButton>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

#include "hips.hpp"
#include "io_file.hpp"

PatchWindow::PatchWindow(QWidget* parent) : QWidget(parent, Qt::Window) {
	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(6, 6, 6, 6);
	setLayout(layout);

	QWidget* inputBox = new QWidget;
	QHBoxLayout* inputLayout = new QHBoxLayout;
	QLabel* inputText = new QLabel(tr("Select input file"));
	QPushButton* inputButton = new QPushButton(tr("Select"));
	inputPathLabel = new ElidedLabel("");
	inputPathLabel->setFixedWidth(200);

	inputLayout->addWidget(inputText);
	inputLayout->addWidget(inputButton);
	inputLayout->addWidget(inputPathLabel);
	inputBox->setLayout(inputLayout);

	QWidget* patchBox = new QWidget;
	QHBoxLayout* patchLayout = new QHBoxLayout;
	QLabel* patchText = new QLabel(tr("Select patch file"));
	QPushButton* patchButton = new QPushButton(tr("Select"));
	patchPathLabel = new ElidedLabel("");
	patchPathLabel->setFixedWidth(200);

	patchLayout->addWidget(patchText);
	patchLayout->addWidget(patchButton);
	patchLayout->addWidget(patchPathLabel);
	patchBox->setLayout(patchLayout);

	QWidget* actionBox = new QWidget;
	QHBoxLayout* actionLayout = new QHBoxLayout;
	QPushButton* applyPatchButton = new QPushButton(tr("Apply patch"));
	actionLayout->addWidget(applyPatchButton);
	actionBox->setLayout(actionLayout);

	layout->addWidget(inputBox);
	layout->addWidget(patchBox);
	layout->addWidget(actionBox);

	connect(inputButton, &QPushButton::clicked, this, [this]() {
		auto path = QFileDialog::getOpenFileName(this, tr("Select file to patch"), "", tr("All files (*.*)"));
		inputPath = std::filesystem::path(path.toStdU16String());

		inputPathLabel->setText(path);
	});

	connect(patchButton, &QPushButton::clicked, this, [this]() {
		auto path = QFileDialog::getOpenFileName(this, tr("Select patch file"), "", tr("Patch files (*.ips *.ups *.bps)"));
		patchPath = std::filesystem::path(path.toStdU16String());

		patchPathLabel->setText(path);
	});

	connect(applyPatchButton, &QPushButton::clicked, this, [this]() {
		if (inputPath.empty() || patchPath.empty()) {
			displayMessage(tr("Paths not provided correctly"), tr("Please provide paths for both the input file and the patch file"));
			return;
		}

		// std::filesystem::path only has += and not + for reasons unknown to humanity
		auto defaultPath = inputPath.parent_path() / inputPath.stem();
		defaultPath += "-patched";
		defaultPath += inputPath.extension();

		auto path = QFileDialog::getSaveFileName(this, tr("Select file"), QString::fromStdU16String(defaultPath.u16string()), tr("All files (*.*)"));
		std::filesystem::path outputPath = std::filesystem::path(path.toStdU16String());

		if (outputPath.empty()) {
			displayMessage(tr("No output path"), tr("No path was provided for the output file, no patching was done"));
			return;
		}

		Hips::PatchType patchType;
		auto extension = patchPath.extension();

		// Figure out what sort of patch we're dealing with
		if (extension == ".ips") {
			patchType = Hips::PatchType::IPS;
		} else if (extension == ".ups") {
			patchType = Hips::PatchType::UPS;
		} else if (extension == ".bps") {
			patchType = Hips::PatchType::BPS;
		} else {
			displayMessage(tr("Unknown patch format"), tr("Unknown format for patch file. Currently IPS, UPS and BPS are supported"));
			return;
		}

		// Read input and patch files into buffers
		IOFile input(inputPath, "rb");
		IOFile patch(patchPath, "rb");

		if (!input.isOpen() || !patch.isOpen()) {
			displayMessage(tr("Failed to open input files"), tr("Make sure they're in a directory Panda3DS has access to"));
			return;
		}

		// Read the files into arrays
		const auto inputSize = *input.size();
		const auto patchSize = *patch.size();

		std::unique_ptr<uint8_t[]> inputData(new uint8_t[inputSize]);
		std::unique_ptr<uint8_t[]> patchData(new uint8_t[patchSize]);

		input.rewind();
		patch.rewind();
		input.readBytes(inputData.get(), inputSize);
		patch.readBytes(patchData.get(), patchSize);

		auto [bytes, result] = Hips::patch(inputData.get(), inputSize, patchData.get(), patchSize, patchType);

		// Write patched file
		if (!bytes.empty()) {
			IOFile output(outputPath, "wb");
			output.writeBytes(bytes.data(), bytes.size());
		}

		switch (result) {
			case Hips::Result::Success:
				displayMessage(
					tr("Patching Success"), tr("Your file was patched successfully."), QMessageBox::Icon::Information, ":/docs/img/rpog_icon.png"
				);
				break;

			case Hips::Result::ChecksumMismatch:
				displayMessage(
					tr("Checksum mismatch"),
					tr("Patch was applied successfully but a checksum mismatch was detected. The input or output files might not be correct")
				);
				break;

			default: displayMessage(tr("Patching error"), tr("An error occured while patching")); break;
		}
	});
}

void PatchWindow::PatchWindow::displayMessage(const QString& title, const QString& message, QMessageBox::Icon icon, const char* iconPath) {
	QMessageBox messageBox(icon, title, message);
	QAbstractButton* button = messageBox.addButton(tr("OK"), QMessageBox::ButtonRole::YesRole);

	if (iconPath != nullptr) {
		button->setIcon(QIcon(iconPath));
	}

	messageBox.exec();
}