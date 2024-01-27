#pragma once

#include <filesystem>
#include <memory>
#include <QAction>
#include <QWidget>
#include "emulator.hpp"

class QListWidget;

class CheatsWindow final : public QWidget
{
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
