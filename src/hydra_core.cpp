#include <emulator.hpp>
#include <hydra/core.hxx>
#include <renderer_gl/renderer_gl.hpp>
#include <stdexcept>

#include "hydra_icon.hpp"

class HC_GLOBAL HydraCore final : public hydra::IBase,
								  public hydra::IOpenGlRendered,
								  public hydra::IFrontendDriven,
								  public hydra::IInput,
								  public hydra::IIcon {
	HYDRA_CLASS
  public:
	HydraCore();

  private:
	// IBase
	bool loadFile(const char* type, const char* path) override;
	void reset() override;
	hydra::Size getNativeSize() override;
	void setOutputSize(hydra::Size size) override;

	// IOpenGlRendered
	void setFbo(unsigned handle) override;
	void setContext(void* context) override;
	void setGetProcAddress(void* function) override;

	// IFrontendDriven
	void runFrame() override;
	uint16_t getFps() override;

	// IInput
	void setPollInputCallback(void (*callback)()) override;
	void setCheckButtonCallback(int32_t (*callback)(uint32_t player, hydra::ButtonType button)) override;

	// IICon
	hydra::Size getIconSize() override;
	const u8* getIconData() override;

	std::unique_ptr<Emulator> emulator;
	RendererGL* renderer;
	void (*pollInputCallback)() = nullptr;
	int32_t (*checkButtonCallback)(uint32_t player, hydra::ButtonType button) = nullptr;
};

HydraCore::HydraCore() : emulator(new Emulator) {
	if (emulator->getRendererType() != RendererType::OpenGL) {
		throw std::runtime_error("HydraCore: Renderer is not OpenGL");
	}
	renderer = static_cast<RendererGL*>(emulator->getRenderer());
}

bool HydraCore::loadFile(const char* type, const char* path) {
	if (std::string(type) == "rom") {
		return emulator->loadROM(path);
	} else {
		return false;
	}
}

void HydraCore::runFrame() {
	renderer->resetStateManager();

	pollInputCallback();
	HIDService& hid = emulator->getServiceManager().getHID();
	hid.setKey(HID::Keys::A, checkButtonCallback(0, hydra::ButtonType::A));
	hid.setKey(HID::Keys::B, checkButtonCallback(0, hydra::ButtonType::B));
	hid.setKey(HID::Keys::X, checkButtonCallback(0, hydra::ButtonType::X));
	hid.setKey(HID::Keys::Y, checkButtonCallback(0, hydra::ButtonType::Y));
	hid.setKey(HID::Keys::L, checkButtonCallback(0, hydra::ButtonType::L1));
	hid.setKey(HID::Keys::R, checkButtonCallback(0, hydra::ButtonType::R1));
	hid.setKey(HID::Keys::Start, checkButtonCallback(0, hydra::ButtonType::Start));
	hid.setKey(HID::Keys::Select, checkButtonCallback(0, hydra::ButtonType::Select));
	hid.setKey(HID::Keys::Up, checkButtonCallback(0, hydra::ButtonType::Keypad1Up));
	hid.setKey(HID::Keys::Down, checkButtonCallback(0, hydra::ButtonType::Keypad1Down));
	hid.setKey(HID::Keys::Left, checkButtonCallback(0, hydra::ButtonType::Keypad1Left));
	hid.setKey(HID::Keys::Right, checkButtonCallback(0, hydra::ButtonType::Keypad1Right));

	int x = !!checkButtonCallback(0, hydra::ButtonType::Analog1Right) - !!checkButtonCallback(0, hydra::ButtonType::Analog1Left);
	int y = !!checkButtonCallback(0, hydra::ButtonType::Analog1Up) - !!checkButtonCallback(0, hydra::ButtonType::Analog1Down);
	hid.setCirclepadX(x * 0x9C);
	hid.setCirclepadY(y * 0x9C);
	hid.updateInputs(emulator->getTicks());

	emulator->runFrame();
}

uint16_t HydraCore::getFps() { return 60; }

void HydraCore::reset() { emulator->reset(Emulator::ReloadOption::Reload); }
hydra::Size HydraCore::getNativeSize() { return {400, 480}; }

// Size doesn't matter as the glBlitFramebuffer call is commented out for the core
void HydraCore::setOutputSize(hydra::Size size) {}

void HydraCore::setGetProcAddress(void* function) {
#ifdef __ANDROID__
	if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(function))) {
		Helpers::panic("OpenGL ES init failed");
	}
#else
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(function))) {
		Helpers::panic("OpenGL init failed");
	}
#endif
	// SDL_Window is not used, so we pass nullptr
	emulator->initGraphicsContext(nullptr);
}

void HydraCore::setContext(void*) {}
void HydraCore::setFbo(unsigned handle) { renderer->setFBO(handle); }

void HydraCore::setPollInputCallback(void (*callback)()) { pollInputCallback = callback; }
void HydraCore::setCheckButtonCallback(int32_t (*callback)(uint32_t player, hydra::ButtonType button)) { checkButtonCallback = callback; }

HC_API hydra::IBase* createEmulator() { return new HydraCore; }
HC_API void destroyEmulator(hydra::IBase* emulator) { delete emulator; }

HC_API const char* getInfo(hydra::InfoType type) {
	switch (type) {
		case hydra::InfoType::CoreName: return "Panda3DS";
		case hydra::InfoType::SystemName: return "Nintendo 3DS";
		case hydra::InfoType::Description: return "HLE 3DS emulator. There's a little Alber in your computer and he runs Nintendo 3DS games.";
		case hydra::InfoType::Author: return "wheremyfoodat (Peach)";
		case hydra::InfoType::Version: return "0.7";
		case hydra::InfoType::License: return "GPLv3";
		case hydra::InfoType::Website: return "https://panda3ds.com/";
		case hydra::InfoType::Extensions: return "3ds,cci,cxi,app,3dsx,elf,axf";
		case hydra::InfoType::Firmware: return "";
		default: return nullptr;
	}
}

HC_API hydra::Size getIconSize() { return {HYDRA_ICON_WIDTH, HYDRA_ICON_HEIGHT}; }
HC_API const u8* getIconData() { return &HYDRA_ICON_DATA[0]; }