#include <stdexcept>
#include <cstdio>

#include <libretro.h>

#include <emulator.hpp>
#include <renderer_gl/renderer_gl.hpp>

static retro_environment_t envCallbacks;
static retro_video_refresh_t videoCallbacks;
static retro_audio_sample_batch_t audioBatchCallback;
static retro_input_poll_t inputPollCallback;
static retro_input_state_t inputStateCallback;

static retro_hw_render_callback hw_render;
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

static void* GetGLProcAddress(const char* name) {
	return (void*)hw_render.get_proc_address(name);
}

static void VideoResetContext() {
#ifdef USING_GLES
	if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(GetGLProcAddress))) {
		Helpers::panic("OpenGL ES init failed");
	}
#else
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(GetGLProcAddress))) {
		Helpers::panic("OpenGL init failed");
	}
#endif

	emulator->initGraphicsContext(nullptr);
}

static void VideoDestroyContext() {
  emulator->deinitGraphicsContext();
}

static bool SetHWRender(retro_hw_context_type type) {
	hw_render.context_type = type;
	hw_render.context_reset = VideoResetContext;
	hw_render.context_destroy = VideoDestroyContext;
	hw_render.bottom_left_origin = true;

	switch (type) {
		case RETRO_HW_CONTEXT_OPENGL_CORE:
			hw_render.version_major = 4;
			hw_render.version_minor = 1;

			if (envCallbacks(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
				return true;
			}
			break;
		case RETRO_HW_CONTEXT_OPENGLES3:
		case RETRO_HW_CONTEXT_OPENGL:
			hw_render.version_major = 3;
			hw_render.version_minor = 1;

			if (envCallbacks(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
				return true;
			}
			break;
		default: break;
	}

	return false;
}

static void videoInit() {
	retro_hw_context_type preferred = RETRO_HW_CONTEXT_NONE;
	envCallbacks(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred);

	if (preferred && SetHWRender(preferred)) return;
	if (SetHWRender(RETRO_HW_CONTEXT_OPENGL_CORE)) return;
	if (SetHWRender(RETRO_HW_CONTEXT_OPENGL)) return;
	if (SetHWRender(RETRO_HW_CONTEXT_OPENGLES3)) return;

	hw_render.context_type = RETRO_HW_CONTEXT_NONE;
}

static bool GetButtonState(uint id) { return inputStateCallback(0, RETRO_DEVICE_JOYPAD, 0, id); }
static float GetAxisState(uint index, uint id) { return inputStateCallback(0, RETRO_DEVICE_ANALOG, index, id); }

static void inputInit() {
	static const retro_controller_description controllers[] = {
		{"Nintendo 3DS", RETRO_DEVICE_JOYPAD},
		{NULL, 0},
	};

	static const retro_controller_info ports[] = {
		{controllers, 1},
		{NULL, 0},
	};

	envCallbacks(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

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

	envCallbacks(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &desc);
}

static std::string FetchVariable(std::string key, std::string def) {
	retro_variable var = {nullptr};
	var.key = key.c_str();

	if (!envCallbacks(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value == nullptr) {
		Helpers::warn("Fetching variable %s failed.", key.c_str());
		return def;
	}

	return std::string(var.value);
}

static bool FetchVariableBool(std::string key, bool def) {
	return FetchVariable(key, def ? "enabled" : "disabled") == "enabled";
}

static void configInit() {
	static const retro_variable values[] = {
		{"panda3ds_use_shader_jit", "Enable shader JIT; enabled|disabled"},
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

	envCallbacks(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)values);
}

static void configUpdate() {
	EmulatorConfig& config = emulator->getConfig();

	config.rendererType = RendererType::OpenGL;
	config.vsyncEnabled = FetchVariableBool("panda3ds_use_vsync", true);
	config.shaderJitEnabled = FetchVariableBool("panda3ds_use_shader_jit", true);
	config.chargerPlugged = FetchVariableBool("panda3ds_use_charger", true);
	config.batteryPercentage = std::clamp(std::stoi(FetchVariable("panda3ds_battery_level", "5")), 0, 100);
	config.dspType = Audio::DSPCore::typeFromString(FetchVariable("panda3ds_dsp_emulation", "null"));
	config.audioEnabled = FetchVariableBool("panda3ds_use_audio", false);
	config.sdCardInserted = FetchVariableBool("panda3ds_use_virtual_sd", true);
	config.sdWriteProtected = FetchVariableBool("panda3ds_write_protect_virtual_sd", false);
	config.accurateShaderMul = FetchVariableBool("panda3ds_accurate_shader_mul", false);
	config.useUbershaders = FetchVariableBool("panda3ds_use_ubershader", true);
	config.forceShadergenForLights = FetchVariableBool("panda3ds_ubershader_lighting_override", true);
	config.lightShadergenThreshold = std::clamp(std::stoi(FetchVariable("panda3ds_ubershader_lighting_override_threshold", "1")), 1, 8);
	config.discordRpcEnabled = false;

	config.save();
}

static void ConfigCheckVariables() {
	bool updated = false;
	envCallbacks(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated);

	if (updated) {
		configUpdate();
	}
}

void retro_get_system_info(retro_system_info* info) {
	info->need_fullpath = true;
	info->valid_extensions = "3ds|3dsx|elf|axf|cci|cxi|app";
	info->library_version = "0.8";
	info->library_name = "Panda3DS";
	info->block_extract = true;
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
	envCallbacks = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
	videoCallbacks = cb;
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
	envCallbacks(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);

	char* save_dir = nullptr;

	if (!envCallbacks(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) || save_dir == nullptr) {
		Helpers::warn("No save directory provided by LibRetro.");
		savePath = std::filesystem::current_path();
	} else {
		savePath = std::filesystem::path(save_dir);
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
	ConfigCheckVariables();

	renderer->setFBO(hw_render.get_current_framebuffer());
	renderer->resetStateManager();

	inputPollCallback();

	HIDService& hid = emulator->getServiceManager().getHID();

	hid.setKey(HID::Keys::A, GetButtonState(RETRO_DEVICE_ID_JOYPAD_A));
	hid.setKey(HID::Keys::B, GetButtonState(RETRO_DEVICE_ID_JOYPAD_B));
	hid.setKey(HID::Keys::X, GetButtonState(RETRO_DEVICE_ID_JOYPAD_X));
	hid.setKey(HID::Keys::Y, GetButtonState(RETRO_DEVICE_ID_JOYPAD_Y));
	hid.setKey(HID::Keys::L, GetButtonState(RETRO_DEVICE_ID_JOYPAD_L));
	hid.setKey(HID::Keys::R, GetButtonState(RETRO_DEVICE_ID_JOYPAD_R));
	hid.setKey(HID::Keys::Start, GetButtonState(RETRO_DEVICE_ID_JOYPAD_START));
	hid.setKey(HID::Keys::Select, GetButtonState(RETRO_DEVICE_ID_JOYPAD_SELECT));
	hid.setKey(HID::Keys::Up, GetButtonState(RETRO_DEVICE_ID_JOYPAD_UP));
	hid.setKey(HID::Keys::Down, GetButtonState(RETRO_DEVICE_ID_JOYPAD_DOWN));
	hid.setKey(HID::Keys::Left, GetButtonState(RETRO_DEVICE_ID_JOYPAD_LEFT));
	hid.setKey(HID::Keys::Right, GetButtonState(RETRO_DEVICE_ID_JOYPAD_RIGHT));

	// Get analog values for the left analog stick (Right analog stick is N3DS-only and unimplemented)
	float xLeft = GetAxisState(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
	float yLeft = GetAxisState(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

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

	videoCallbacks(RETRO_HW_FRAME_BUFFER_VALID, emulator->width, emulator->height, 0);
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
		return 0;
	}

	return 0;
}

void* retro_get_memory_data(uint id) {
	if (id == RETRO_MEMORY_SYSTEM_RAM) {
		return 0;
	}

	return nullptr;
}

void retro_cheat_set(uint index, bool enabled, const char* code) {}
void retro_cheat_reset() {}
