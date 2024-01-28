#include "panda_qt/main_window.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QString>
#include <cstdio>
#include <fstream>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <SDL.h>

SDL_Window* windowSDL;
SDL_GLContext glContextSDL;

MainWindow::MainWindow(QApplication* app, QWidget* parent) : QMainWindow(parent), screen(this) {
	setWindowTitle("Alber");
	// Enable drop events for loading ROMs
	setAcceptDrops(true);
	resize(800, 240 * 4);
	screen.show();

	appRunning = true;

	// Set our menu bar up
	menuBar = new QMenuBar(this);
	setMenuBar(menuBar);

	// Create menu bar menus
	auto fileMenu = menuBar->addMenu(tr("File"));
	auto emulationMenu = menuBar->addMenu(tr("Emulation"));
	auto toolsMenu = menuBar->addMenu(tr("Tools"));
	auto aboutMenu = menuBar->addMenu(tr("About"));

	// Create and bind actions for them
	auto loadGameAction = fileMenu->addAction(tr("Load game"));
	auto loadLuaAction = fileMenu->addAction(tr("Load Lua script"));
	auto openAppFolderAction = fileMenu->addAction(tr("Open Panda3DS folder"));

	connect(loadGameAction, &QAction::triggered, this, &MainWindow::selectROM);
	connect(loadLuaAction, &QAction::triggered, this, &MainWindow::selectLuaFile);
	connect(openAppFolderAction, &QAction::triggered, this, [this]() {
		QString path = QString::fromStdU16String(emu->getAppDataRoot().u16string());
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	});

	auto pauseAction = emulationMenu->addAction(tr("Pause"));
	auto resumeAction = emulationMenu->addAction(tr("Resume"));
	auto resetAction = emulationMenu->addAction(tr("Reset"));
	auto configureAction = emulationMenu->addAction(tr("Configure"));
	connect(pauseAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Pause}); });
	connect(resumeAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Resume}); });
	connect(resetAction, &QAction::triggered, this, [this]() { sendMessage(EmulatorMessage{.type = MessageType::Reset}); });
	connect(configureAction, &QAction::triggered, this, [this]() { configWindow->show(); });

	auto dumpRomFSAction = toolsMenu->addAction(tr("Dump RomFS"));
	auto luaEditorAction = toolsMenu->addAction(tr("Open Lua Editor"));
	connect(dumpRomFSAction, &QAction::triggered, this, &MainWindow::dumpRomFS);
	connect(luaEditorAction, &QAction::triggered, this, &MainWindow::openLuaEditor);

	auto aboutAction = aboutMenu->addAction(tr("About Panda3DS"));
	connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutMenu);

	// Set up misc objects
	aboutWindow = new AboutWindow(nullptr);
	configWindow = new ConfigWindow(this);
	luaEditor = new TextEditorWindow(this, "script.lua", "");

	emu = new Emulator();
	emu->setOutputSize(screen.surfaceWidth, screen.surfaceHeight);

	auto args = QCoreApplication::arguments();
	if (args.size() > 1) {
		auto romPath = std::filesystem::current_path() / args.at(1).toStdU16String();
		if (!emu->loadROM(romPath)) {
			// For some reason just .c_str() doesn't show the proper path
			Helpers::warn("Failed to load ROM file: %s", romPath.string().c_str());
		}
	}

	// The emulator graphics context for the thread should be initialized in the emulator thread due to how GL contexts work
	emuThread = std::thread([this]() {
		// Setup SDL
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
			printf("Error: %s\n", SDL_GetError());
			return -1;
		}

		    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
		// GL ES 2.0 + GLSL 100
		const char* glslVersion = "#version 100";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
		// GL 3.2 Core + GLSL 150
		const char* glslVersion = "#version 150";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
		// GL 3.0 + GLSL 130
		const char* glslVersion = "#version 130";
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

		// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
		SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

		// Create window with graphics context
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
		windowSDL = SDL_CreateWindow("ImGuiWindow", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, windowFlags);
		if (windowSDL == nullptr) {
			printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
			return -1;
		}
		glContextSDL = SDL_GL_CreateContext(windowSDL);
		SDL_GL_MakeCurrent(windowSDL, glContextSDL);
		SDL_GL_SetSwapInterval(1);  // Enable vsync
		
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		// ImGui::StyleColorsLight();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForOpenGL(windowSDL, glContextSDL);
		ImGui_ImplOpenGL3_Init(glslVersion);

		const RendererType rendererType = emu->getConfig().rendererType;
		usingGL = (rendererType == RendererType::OpenGL || rendererType == RendererType::Software || rendererType == RendererType::Null);
		usingVk = (rendererType == RendererType::Vulkan);

		if (usingGL) {
			// Make GL context current for this thread, enable VSync
			GL::Context* glContext = screen.getGLContext();
			glContext->MakeCurrent();
			glContext->SetSwapInterval(1);

			emu->initGraphicsContext(glContext);
		} else if (usingVk) {
			Helpers::panic("Vulkan on Qt is currently WIP, try the SDL frontend instead!");
		} else {
			Helpers::panic("Unsupported graphics backend for Qt frontend!");
		}

		emuThreadMainLoop();
	});
}

void MainWindow::emuThreadMainLoop() {
	while (appRunning) {
		{
			std::unique_lock lock(messageQueueMutex);

			// Dispatch all messages in the message queue
			if (!messageQueue.empty()) {
				for (const auto& msg : messageQueue) {
					dispatchMessage(msg);
				}

				messageQueue.clear();
			}
		}

		emu->runFrame();
		if (emu->romType != ROMType::None) {
			emu->getServiceManager().getHID().updateInputs(emu->getTicks());
		}

		swapEmuBuffer();

		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse
		// data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the
		// keyboard data. Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		SDL_GL_MakeCurrent(nullptr, nullptr);
		SDL_GL_MakeCurrent(windowSDL, glContextSDL);
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(windowSDL);
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
		// ImGui!).
		bool show_demo_window = true;
		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");           // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);  // Edit bools storing our window open/close state
			if (ImGui::Button("Button"))  // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::End();
		}
		ImGui::ShowDemoWindow(&show_demo_window);
		ImGui::Render();

		ImGuiIO& io = ImGui::GetIO();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(windowSDL);

		screen.getGLContext()->MakeCurrent();
	}

	// Unbind GL context if we're using GL, otherwise some setups seem to be unable to join this thread
	if (usingGL) {
		screen.getGLContext()->DoneCurrent();
	}
}

void MainWindow::swapEmuBuffer() {
	if (usingGL) {
		screen.getGLContext()->SwapBuffers();
	} else {
		Helpers::panic("[Qt] Don't know how to swap buffers for the current rendering backend :(");
	}
}

void MainWindow::selectROM() {
	auto path =
		QFileDialog::getOpenFileName(this, tr("Select 3DS ROM to load"), "", tr("Nintendo 3DS ROMs (*.3ds *.cci *.cxi *.app *.3dsx *.elf *.axf)"));

	if (!path.isEmpty()) {
		std::filesystem::path* p = new std::filesystem::path(path.toStdU16String());

		EmulatorMessage message{.type = MessageType::LoadROM};
		message.path.p = p;
		sendMessage(message);
	}
}

void MainWindow::selectLuaFile() {
	auto path = QFileDialog::getOpenFileName(this, tr("Select Lua script to load"), "", tr("Lua scripts (*.lua *.txt)"));

	if (!path.isEmpty()) {
		std::ifstream file(std::filesystem::path(path.toStdU16String()), std::ios::in);

		if (file.fail()) {
			printf("Failed to load selected lua file\n");
			return;
		}

		// Read whole file into an std::string string
		// Get file size, preallocate std::string to avoid furthermemory allocations
		std::string code;
		file.seekg(0, std::ios::end);
		code.resize(file.tellg());

		// Rewind and read the whole file
		file.seekg(0, std::ios::beg);
		file.read(&code[0], code.size());
		file.close();

		loadLuaScript(code);
		// Copy the Lua script to the Lua editor
		luaEditor->setText(code);
	}
}

// Cleanup when the main window closes
MainWindow::~MainWindow() {
	appRunning = false;  // Set our running atomic to false in order to make the emulator thread stop, and join it

	if (emuThread.joinable()) {
		emuThread.join();
	}

	delete emu;
	delete menuBar;
	delete aboutWindow;
	delete configWindow;
	delete luaEditor;
}

// Send a message to the emulator thread. Lock the mutex and just push back to the vector.
void MainWindow::sendMessage(const EmulatorMessage& message) {
	std::unique_lock lock(messageQueueMutex);
	messageQueue.push_back(message);
}

void MainWindow::dumpRomFS() {
	auto folder = QFileDialog::getExistingDirectory(
		this, tr("Select folder to dump RomFS files to"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (folder.isEmpty()) {
		return;
	}
	std::filesystem::path path(folder.toStdU16String());

	messageQueueMutex.lock();
	RomFS::DumpingResult res = emu->dumpRomFS(path);
	messageQueueMutex.unlock();

	switch (res) {
		case RomFS::DumpingResult::Success: break;  // Yay!
		case RomFS::DumpingResult::InvalidFormat: {
			QMessageBox messageBox(
				QMessageBox::Icon::Warning, tr("Invalid format for RomFS dumping"),
				tr("The currently loaded app is not in a format that supports RomFS")
			);

			QAbstractButton* button = messageBox.addButton(tr("OK"), QMessageBox::ButtonRole::YesRole);
			button->setIcon(QIcon(":/docs/img/rsob_icon.png"));
			messageBox.exec();
			break;
		}

		case RomFS::DumpingResult::NoRomFS:
			QMessageBox::warning(this, tr("No RomFS found"), tr("No RomFS partition was found in the loaded app"));
			break;
	}
}

void MainWindow::showAboutMenu() {
	AboutWindow about(this);
	about.exec();
}

void MainWindow::openLuaEditor() { luaEditor->show(); }

void MainWindow::dispatchMessage(const EmulatorMessage& message) {
	switch (message.type) {
		case MessageType::LoadROM:
			emu->loadROM(*message.path.p);
			// Clean up the allocated path
			delete message.path.p;
			break;

		case MessageType::LoadLuaScript:
			emu->getLua().loadString(*message.string.str);
			delete message.string.str;
			break;

		case MessageType::Pause: emu->pause(); break;
		case MessageType::Resume: emu->resume(); break;
		case MessageType::TogglePause: emu->togglePause(); break;
		case MessageType::Reset: emu->reset(Emulator::ReloadOption::Reload); break;
		case MessageType::PressKey: emu->getServiceManager().getHID().pressKey(message.key.key); break;
		case MessageType::ReleaseKey: emu->getServiceManager().getHID().releaseKey(message.key.key); break;
	}
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
	auto pressKey = [this](u32 key) {
		EmulatorMessage message{.type = MessageType::PressKey};
		message.key.key = key;

		sendMessage(message);
	};

	switch (event->key()) {
		case Qt::Key_L: pressKey(HID::Keys::A); break;
		case Qt::Key_K: pressKey(HID::Keys::B); break;
		case Qt::Key_O: pressKey(HID::Keys::X); break;
		case Qt::Key_I: pressKey(HID::Keys::Y); break;

		case Qt::Key_Q: pressKey(HID::Keys::L); break;
		case Qt::Key_P: pressKey(HID::Keys::R); break;

		case Qt::Key_Right: pressKey(HID::Keys::Right); break;
		case Qt::Key_Left: pressKey(HID::Keys::Left); break;
		case Qt::Key_Up: pressKey(HID::Keys::Up); break;
		case Qt::Key_Down: pressKey(HID::Keys::Down); break;

		case Qt::Key_Return: pressKey(HID::Keys::Start); break;
		case Qt::Key_Backspace: pressKey(HID::Keys::Select); break;
		case Qt::Key_F4: sendMessage(EmulatorMessage{.type = MessageType::TogglePause}); break;
		case Qt::Key_F5: sendMessage(EmulatorMessage{.type = MessageType::Reset}); break;
	}
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
	auto releaseKey = [this](u32 key) {
		EmulatorMessage message{.type = MessageType::ReleaseKey};
		message.key.key = key;

		sendMessage(message);
	};

	switch (event->key()) {
		case Qt::Key_L: releaseKey(HID::Keys::A); break;
		case Qt::Key_K: releaseKey(HID::Keys::B); break;
		case Qt::Key_O: releaseKey(HID::Keys::X); break;
		case Qt::Key_I: releaseKey(HID::Keys::Y); break;

		case Qt::Key_Q: releaseKey(HID::Keys::L); break;
		case Qt::Key_P: releaseKey(HID::Keys::R); break;

		case Qt::Key_Right: releaseKey(HID::Keys::Right); break;
		case Qt::Key_Left: releaseKey(HID::Keys::Left); break;
		case Qt::Key_Up: releaseKey(HID::Keys::Up); break;
		case Qt::Key_Down: releaseKey(HID::Keys::Down); break;

		case Qt::Key_Return: releaseKey(HID::Keys::Start); break;
		case Qt::Key_Backspace: releaseKey(HID::Keys::Select); break;
	}
}

void MainWindow::loadLuaScript(const std::string& code) {
	EmulatorMessage message{.type = MessageType::LoadLuaScript};

	// Make a copy of the code on the heap to send via the message queue
	message.string.str = new std::string(code);
	sendMessage(message);
}