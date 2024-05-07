#pragma once

#include <SDL.h>

#include <QApplication>
#include <QMenuBar>
#include <QtWidgets>
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "emulator.hpp"
#include "input_mappings.hpp"
#include "panda_qt/about_window.hpp"
#include "panda_qt/cheats_window.hpp"
#include "panda_qt/config_window.hpp"
#include "panda_qt/patch_window.hpp"
#include "panda_qt/screen.hpp"
#include "panda_qt/text_editor.hpp"
#include "services/hid.hpp"

struct CheatMessage {
	u32 handle;
	std::vector<uint8_t> cheat;
	std::function<void(u32)> callback;
};

class MainWindow : public QMainWindow {
	Q_OBJECT

  private:
	// Types of messages we might send from the GUI thread to the emulator thread
	enum class MessageType {
		LoadROM,
		Reset,
		Pause,
		Resume,
		TogglePause,
		DumpRomFS,
		PressKey,
		ReleaseKey,
		SetCirclePadX,
		SetCirclePadY,
		LoadLuaScript,
		EditCheat,
		PressTouchscreen,
		ReleaseTouchscreen,
	};

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
				s16 value;
			} circlepad;

			struct {
				std::string* str;
			} string;

			struct {
				CheatMessage* c;
			} cheat;

			struct {
				u16 x;
				u16 y;
			} touchscreen;
		};
	};

	// This would normally be an std::unique_ptr but it's shared between threads so definitely not
	Emulator* emu = nullptr;
	std::thread emuThread;

	std::atomic<bool> appRunning = true;  // Is the application itself running?
	// Used for synchronizing messages between the emulator and UI
	std::mutex messageQueueMutex;
	std::vector<EmulatorMessage> messageQueue;

	QMenuBar* menuBar = nullptr;
	InputMappings keyboardMappings;
	ScreenWidget screen;
	AboutWindow* aboutWindow;
	ConfigWindow* configWindow;
	CheatsWindow* cheatsEditor;
	TextEditorWindow* luaEditor;
	PatchWindow* patchWindow;

	// We use SDL's game controller API since it's the sanest API that supports as many controllers as possible
	SDL_GameController* gameController = nullptr;
	int gameControllerID = 0;

	void swapEmuBuffer();
	void emuThreadMainLoop();
	void selectLuaFile();
	void selectROM();
	void dumpDspFirmware();
	void dumpRomFS();
	void openLuaEditor();
	void openCheatsEditor();
	void openPatchWindow();
	void showAboutMenu();
	void initControllers();
	void pollControllers();
	void sendMessage(const EmulatorMessage& message);
	void dispatchMessage(const EmulatorMessage& message);

	// Tracks whether we are using an OpenGL-backed renderer or a Vulkan-backed renderer
	bool usingGL = false;
	bool usingVk = false;

	// Variables to keep track of whether the user is controlling the 3DS analog stick with their keyboard
	// This is done so when a gamepad is connected, we won't automatically override the 3DS analog stick settings with the gamepad's state
	// And so the user can still use the keyboard to control the analog
	bool keyboardAnalogX = false;
	bool keyboardAnalogY = false;

  public:
	MainWindow(QApplication* app, QWidget* parent = nullptr);
	~MainWindow();

	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

	void loadLuaScript(const std::string& code);
	void editCheat(u32 handle, const std::vector<uint8_t>& cheat, const std::function<void(u32)>& callback);
};
