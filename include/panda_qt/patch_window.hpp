#pragma once
#include <QLabel>
#include <QWidget>
#include <filesystem>

#include "panda_qt/ellided_label.hpp"

class PatchWindow final : public QWidget {
	Q_OBJECT

  public:
	PatchWindow(QWidget* parent = nullptr);
	~PatchWindow() = default;

  private:
	std::filesystem::path inputPath = "";
	std::filesystem::path patchPath = "";

	EllidedLabel* inputPathLabel = nullptr;
	EllidedLabel* patchPathLabel = nullptr;
};
