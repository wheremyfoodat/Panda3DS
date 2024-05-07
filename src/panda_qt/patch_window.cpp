#include "panda_qt/patch_window.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
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
	inputPathLabel = new EllidedLabel("");
	inputPathLabel->setFixedWidth(200);

	inputLayout->addWidget(inputText);
	inputLayout->addWidget(inputButton);
	inputLayout->addWidget(inputPathLabel);
	inputBox->setLayout(inputLayout);

	QWidget* patchBox = new QWidget;
	QHBoxLayout* patchLayout = new QHBoxLayout;
	QLabel* patchText = new QLabel(tr("Select patch file"));
	QPushButton* patchButton = new QPushButton(tr("Select"));
	patchPathLabel = new EllidedLabel("");
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
			printf("Pls set paths properly");
			return;
		}

		auto path = QFileDialog::getSaveFileName(this, tr("Select file"), QString::fromStdU16String(inputPath.u16string()), tr("All files (*.*)"));
		std::filesystem::path outputPath = std::filesystem::path(path.toStdU16String());

		if (outputPath.empty()) {
			printf("Pls set paths properly");
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
			printf("Unknown patch format\n");
			return;
		}

		// Read input and patch files into buffers
		IOFile input(inputPath, "rb");
		IOFile patch(patchPath, "rb");

		if (!input.isOpen() || !patch.isOpen()) {
			printf("Failed to open input or patch file.\n");
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
	});
}