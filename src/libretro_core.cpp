#include <stdexcept>
#include <cstdio>
#include <regex>

#include <libretro.h>

#include <version.hpp>
#include <emulator.hpp>
#include <renderer_gl/renderer_gl.hpp>

static retro_environment_t envCallback;
static retro_video_refresh_t videoCallback;
static retro_audio_sample_batch_t audioBatchCallback;
static retro_input_poll_t inputPollCallback;
static retro_input_state_t inputStateCallback;

static retro_hw_render_callback hwRender;
static std::filesystem::path savePath;

static bool screenTouched;

std::unique_ptr<Emulator> emulator;
RendererGL* renderer;

std::filesystem::path Emulator::getConfigPath() {
	return std::filesystem::path(savePath / "config.toml");
}

std::filesystem::path Emulator::getAppDataRoot() {
	return std::filesystem::path(savePath / "Emulator Files");
}

static void* getGLProcAddress(const char* name) {
	return (void*)hwRender.get_proc_address(name);
}

static void videoResetContext() {
#ifdef USING_GLES
	if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(getGLProcAddress))) {
		Helpers::panic("OpenGL ES init failed");
	}
#else
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(getGLProcAddress))) {
		Helpers::panic("OpenGL init failed");
	}
#endif

	emulator->initGraphicsContext(nullptr);
}

static void videoDestroyContext() {
	emulator->deinitGraphicsContext();
}

static bool setHWRender(retro_hw_context_type type) {
	hwRender.context_type = type;
	hwRender.context_reset = videoResetContext;
	hwRender.context_destroy = videoDestroyContext;
	hwRender.bottom_left_origin = true;

	switch (type) {
		case RETRO_HW_CONTEXT_OPENGL_CORE:
			hwRender.version_major = 4;
			hwRender.version_minor = 1;

			if (envCallback(RETRO_ENVIRONMENT_SET_HW_RENDER, &hwRender)) {
				return true;
			}
			break;
		case RETRO_HW_CONTEXT_OPENGLES3:
		case RETRO_HW_CONTEXT_OPENGL:
			hwRender.version_major = 3;
			hwRender.version_minor = 1;

			if (envCallback(RETRO_ENVIRONMENT_SET_HW_RENDER, &hwRender)) {
				return true;
			}
			break;
		default: break;
	}

	return false;
}

static void videoInit() {
	retro_hw_context_type preferred = RETRO_HW_CONTEXT_NONE;
	envCallback(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred);

	if (preferred && setHWRender(preferred)) return;
	if (setHWRender(RETRO_HW_CONTEXT_OPENGL_CORE)) return;
	if (setHWRender(RETRO_HW_CONTEXT_OPENGL)) return;
	if (setHWRender(RETRO_HW_CONTEXT_OPENGLES3)) return;

	hwRender.context_type = RETRO_HW_CONTEXT_NONE;
}

static bool getButtonState(uint id) { return inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, id); }
static float getAxisState(uint index, uint id) { return inputStateCallback(0, RETRO_DEVICE_ANALOG, index, id); }

static void inputInit() {
	static const retro_controller_description controllers[] = {
		{"Nintendo 3DS", RETRO_DEVICE_JOYPAD},
		{NULL, 0},
	};

	static const retro_controller_info ports[] = {
		{controllers, 1},
		{NULL, 0},
	};

	envCallback(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

	retro_input_descriptor desc[] = {
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
		{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Circle Pad X"},
		{0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Circle Pad Y"},
		{0},
	};

	envCallback(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &desc);
}

static std::string fetchVariable(std::string key, std::string def) {
	retro_variable var = {nullptr};
	var.key = key.c_str();

	if (!envCallback(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value == nullptr) {
		Helpers::warn("Fetching variable %s failed.", key.c_str());
		return def;
	}

	return std::string(var.value);
}

static int fetchVariableInt(std::string key, int def) {
	std::string value = fetchVariable(key, std::to_string(def));

	if (!value.empty() && std::isdigit(value[0])) {
		return std::stoi(value);
	}

	return 0;
}

static bool fetchVariableBool(std::string key, bool def) {
	return fetchVariable(key, def ? "enabled" : "disabled") == "enabled";
}

static int fetchVariableRange(std::string key, int min, int max) {
	return std::clamp(fetchVariableInt(key, min), min, max);
}

static void configInit() {
	static const retro_variable values[] = {
		{"panda3ds_use_shader_jit", EmulatorConfig::shaderJitDefault ? "Enable shader JIT; enabled|disabled" : "Enable shader JIT; disabled|enabled"},
		{"panda3ds_accelerate_shaders",
		 EmulatorConfig::accelerateShadersDefault ? "Run 3DS shaders on the GPU; enabled|disabled" : "Run 3DS shaders on the GPU; disabled|enabled"},
		{"panda3ds_accurate_shader_mul", "Enable accurate shader multiplication; disabled|enabled"},
		{"panda3ds_use_ubershader", EmulatorConfig::ubershaderDefault ? "Use ubershaders (No stutter, maybe slower); enabled|disabled"
																	  : "Use ubershaders (No stutter, maybe slower); disabled|enabled"},
		{"panda3ds_use_vsync", "Enable VSync; enabled|disabled"},
		{"panda3ds_dsp_emulation", "DSP emulation; Null|HLE|LLE"},
		{"panda3ds_use_audio", "Enable audio; disabled|enabled"},
		{"panda3ds_use_virtual_sd", "Enable virtual SD card; enabled|disabled"},
		{"panda3ds_write_protect_virtual_sd", "Write protect virtual SD card; disabled|enabled"},
		{"panda3ds_battery_level", "Battery percentage; 5|10|20|30|50|70|90|100"},
		{"panda3ds_use_charger", "Charger plugged; enabled|disabled"},
		{"panda3ds_ubershader_lighting_override", "Force shadergen when rendering lights; enabled|disabled"},
		{"panda3ds_ubershader_lighting_override_threshold", "Light threshold for forcing shadergen; 1|2|3|4|5|6|7|8"},
		{nullptr, nullptr},
	};

	envCallback(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)values);
}

static void configUpdate() {
	EmulatorConfig& config = emulator->getConfig();

	config.rendererType = RendererType::OpenGL;
	config.vsyncEnabled = fetchVariableBool("panda3ds_use_vsync", true);
	config.shaderJitEnabled = fetchVariableBool("panda3ds_use_shader_jit", EmulatorConfig::shaderJitDefault);
	config.chargerPlugged = fetchVariableBool("panda3ds_use_charger", true);
	config.batteryPercentage = fetchVariableRange("panda3ds_battery_level", 5, 100);
	config.dspType = Audio::DSPCore::typeFromString(fetchVariable("panda3ds_dsp_emulation", "null"));
	config.audioEnabled = fetchVariableBool("panda3ds_use_audio", false);
	config.sdCardInserted = fetchVariableBool("panda3ds_use_virtual_sd", true);
	config.sdWriteProtected = fetchVariableBool("panda3ds_write_protect_virtual_sd", false);
	config.accurateShaderMul = fetchVariableBool("panda3ds_accurate_shader_mul", false);
	config.useUbershaders = fetchVariableBool("panda3ds_use_ubershader", EmulatorConfig::ubershaderDefault);
	config.accelerateShaders = fetchVariableBool("panda3ds_accelerate_shaders", EmulatorConfig::accelerateShadersDefault);

	config.forceShadergenForLights = fetchVariableBool("panda3ds_ubershader_lighting_override", true);
	config.lightShadergenThreshold = fetchVariableRange("panda3ds_ubershader_lighting_override_threshold", 1, 8);
	config.discordRpcEnabled = false;

	config.save();
}

static void configCheckVariables() {
	bool updated = false;
	envCallback(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated);

	if (updated) {
		configUpdate();
	}
}

void retro_get_system_info(retro_system_info* info) {
	info->need_fullpath = true;
	info->valid_extensions = "3ds|3dsx|elf|axf|cci|cxi|app";
	info->library_version = PANDA3DS_VERSION;
	info->library_name = "Panda3DS";
	info->block_extract = false;
}

void retro_get_system_av_info(retro_system_av_info* info) {
	info->geometry.base_width = emulator->width;
	info->geometry.base_height = emulator->height;

	info->geometry.max_width = info->geometry.base_width;
	info->geometry.max_height = info->geometry.base_height;

	info->geometry.aspect_ratio = float(5.0 / 6.0);
	info->timing.fps = 60.0;
	info->timing.sample_rate = 32768;
}

void retro_set_environment(retro_environment_t cb) {
	envCallback = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
	videoCallback = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
	audioBatchCallback = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) {}

void retro_set_input_poll(retro_input_poll_t cb) {
	inputPollCallback = cb;
}

void retro_set_input_state(retro_input_state_t cb) {
	inputStateCallback = cb;
}

void retro_init() {
	enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;
	envCallback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);

	char* saveDir = nullptr;

	if (!envCallback(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &saveDir) || saveDir == nullptr) {
		Helpers::warn("No save directory provided by LibRetro.");
		savePath = std::filesystem::current_path();
	} else {
		savePath = std::filesystem::path(saveDir);
	}

	emulator = std::make_unique<Emulator>();
}

void retro_deinit() {
	emulator = nullptr;
}

bool retro_load_game(const retro_game_info* game) {
	configInit();
	configUpdate();

	if (emulator->getRendererType() != RendererType::OpenGL) {
		Helpers::panic("Libretro: Renderer is not OpenGL");
	}

	renderer = static_cast<RendererGL*>(emulator->getRenderer());
	emulator->setOutputSize(emulator->width, emulator->height);

	inputInit();
	videoInit();

	return emulator->loadROM(game->path);
}

bool retro_load_game_special(uint type, const retro_game_info* info, usize num) { return false; }

void retro_unload_game() {
	renderer->setFBO(0);
	renderer = nullptr;
}

void retro_reset() {
	emulator->reset(Emulator::ReloadOption::Reload);
}

void retro_run() {
	configCheckVariables();

	renderer->setFBO(hwRender.get_current_framebuffer());
	renderer->resetStateManager();

	inputPollCallback();

	HIDService& hid = emulator->getServiceManager().getHID();

	hid.setKey(HID::Keys::A, getButtonState(RETRO_DEVICE_ID_JOYPAD_A));
	hid.setKey(HID::Keys::B, getButtonState(RETRO_DEVICE_ID_JOYPAD_B));
	hid.setKey(HID::Keys::X, getButtonState(RETRO_DEVICE_ID_JOYPAD_X));
	hid.setKey(HID::Keys::Y, getButtonState(RETRO_DEVICE_ID_JOYPAD_Y));
	hid.setKey(HID::Keys::L, getButtonState(RETRO_DEVICE_ID_JOYPAD_L));
	hid.setKey(HID::Keys::R, getButtonState(RETRO_DEVICE_ID_JOYPAD_R));
	hid.setKey(HID::Keys::Start, getButtonState(RETRO_DEVICE_ID_JOYPAD_START));
	hid.setKey(HID::Keys::Select, getButtonState(RETRO_DEVICE_ID_JOYPAD_SELECT));
	hid.setKey(HID::Keys::Up, getButtonState(RETRO_DEVICE_ID_JOYPAD_UP));
	hid.setKey(HID::Keys::Down, getButtonState(RETRO_DEVICE_ID_JOYPAD_DOWN));
	hid.setKey(HID::Keys::Left, getButtonState(RETRO_DEVICE_ID_JOYPAD_LEFT));
	hid.setKey(HID::Keys::Right, getButtonState(RETRO_DEVICE_ID_JOYPAD_RIGHT));

	// Get analog values for the left analog stick (Right analog stick is N3DS-only and unimplemented)
	float xLeft = getAxisState(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
	float yLeft = getAxisState(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

	hid.setCirclepadX((xLeft / +32767) * 0x9C);
	hid.setCirclepadY((yLeft / -32767) * 0x9C);

	bool touchScreen = false;

	const int posX = inputStateCallback(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
	const int posY = inputStateCallback(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

	const int newX = static_cast<int>((posX + 0x7fff) / (float)(0x7fff * 2) * emulator->width);
	const int newY = static_cast<int>((posY + 0x7fff) / (float)(0x7fff * 2) * emulator->height);

	const int offsetX = 40;
	const int offsetY = emulator->height / 2;

	const bool inScreenX = newX >= offsetX && newX <= emulator->width - offsetX;
	const bool inScreenY = newY >= offsetY && newY <= emulator->height;

	if (inScreenX && inScreenY) {
		touchScreen |= inputStateCallback(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
		touchScreen |= inputStateCallback(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
	}

	if (touchScreen) {
		u16 x = static_cast<u16>(newX - offsetX);
		u16 y = static_cast<u16>(newY - offsetY);

		hid.setTouchScreenPress(x, y);
		screenTouched = true;
	} else if (screenTouched) {
		hid.releaseTouchScreen();
		screenTouched = false;
	}

	hid.updateInputs(emulator->getTicks());
	emulator->runFrame();

	videoCallback(RETRO_HW_FRAME_BUFFER_VALID, emulator->width, emulator->height, 0);
}

void retro_set_controller_port_device(uint port, uint device) {}

usize retro_serialize_size() {
	usize size = 0;
	return size;
}

bool retro_serialize(void* data, usize size) { return false; }
bool retro_unserialize(const void* data, usize size) { return false; }

uint retro_get_region() { return RETRO_REGION_NTSC; }
uint retro_api_version() { return RETRO_API_VERSION; }

usize retro_get_memory_size(uint id) {
	if (id == RETRO_MEMORY_SYSTEM_RAM) {
		return Memory::FCRAM_SIZE;
	}

	return 0;
}

void* retro_get_memory_data(uint id) {
	if (id == RETRO_MEMORY_SYSTEM_RAM) {
		return emulator->getMemory().getFCRAM();
	}

	return nullptr;
}

void retro_cheat_set(uint index, bool enabled, const char* code) {
	std::string cheatCode = std::regex_replace(code, std::regex("[^0-9a-fA-F]"), "");
	std::vector<u8> bytes;

	for (usize i = 0; i < cheatCode.size(); i += 2) {
		std::string hex = cheatCode.substr(i, 2);
		bytes.push_back((u8)std::stoul(hex, nullptr, 16));
	}

	u32 id = emulator->getCheats().addCheat(bytes.data(), bytes.size());

	if (enabled) {
		emulator->getCheats().enableCheat(id);
	} else {
		emulator->getCheats().disableCheat(id);
	}
}

void retro_cheat_reset() {
	emulator->getCheats().reset();
}
