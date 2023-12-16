#pragma once

#include <QApplication>
#include <QMenuBar>
#include <QtWidgets>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#include "emulator.hpp"
#include "panda_qt/about_window.hpp"
#include "panda_qt/config_window.hpp"
#include "panda_qt/screen.hpp"
#include "panda_qt/text_editor.hpp"
#include "services/hid.hpp"

class MainWindow : public QMainWindow {
	Q_OBJECT

  private:
	// Types of messages we might send from the GUI thread to the emulator thread
	enum class MessageType { LoadROM, Reset, Pause, Resume, TogglePause, DumpRomFS, PressKey, ReleaseKey, LoadLuaScript };

	// Tagged union representing our message queue messages
	struct EmulatorMessage {
		MessageType type;

		union {
			struct {
				std::filesystem::path* p;
			} path;

			struct {
				u32 key;
			} key;

			struct {
				std::string* str;
			} string;
		};
	};

	// This would normally be an std::unique_ptr but it's shared between threads so definitely not
	Emulator* emu = nullptr;
	std::thread emuThread;

	std::atomic<bool> appRunning = true;  // Is the application itself running?
	// Used for synchronizing messages between the emulator and UI
	std::mutex messageQueueMutex;
	std::vector<EmulatorMessage> messageQueue;

	ScreenWidget screen;
	AboutWindow* aboutWindow;
	ConfigWindow* configWindow;
	TextEditorWindow* luaEditor;
	QMenuBar* menuBar = nullptr;

	void swapEmuBuffer();
	void emuThreadMainLoop();
	void selectROM();
	void dumpRomFS();
	void openLuaEditor();
	void showAboutMenu();
	void sendMessage(const EmulatorMessage& message);
	void dispatchMessage(const EmulatorMessage& message);

	// Tracks whether we are using an OpenGL-backed renderer or a Vulkan-backed renderer
	bool usingGL = false;
	bool usingVk = false;

  public:
	MainWindow(QApplication* app, QWidget* parent = nullptr);
	~MainWindow();

	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void loadLuaScript(const std::string& code);
};