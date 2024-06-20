#include <stdexcept>
#include <cstdio>

#include <libretro.h>

#include <emulator.hpp>
#include <renderer_gl/renderer_gl.hpp>

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static struct retro_hw_render_callback hw_render;

std::unique_ptr<Emulator> emulator;
RendererGL* renderer;

static void* GetProcAddress(const char* name) {
  return (void*)hw_render.get_proc_address(name);
}

static void VideoResetContext(void) {
#ifdef USING_GLES
  if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(GetProcAddress))) {
    Helpers::panic("OpenGL ES init failed");
  }
#else
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(GetProcAddress))) {
    Helpers::panic("OpenGL init failed");
  }
#endif

  emulator->initGraphicsContext(nullptr);
}

static void VideoDestroyContext(void) {
  emulator->deinitGraphicsContext();
}

static bool SetHWRender(retro_hw_context_type type) {
  hw_render.context_type       = type;
  hw_render.context_reset      = VideoResetContext;
  hw_render.context_destroy    = VideoDestroyContext;
  hw_render.bottom_left_origin = true;

  switch (type) {
  case RETRO_HW_CONTEXT_OPENGL_CORE:
    hw_render.version_major = 3;
    hw_render.version_minor = 3;

    if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
      return true;
    }
    break;
  case RETRO_HW_CONTEXT_OPENGLES3:
  case RETRO_HW_CONTEXT_OPENGL:
    hw_render.version_major = 3;
    hw_render.version_minor = 0;

    if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
      return true;
    }
    break;
  default:
    break;
  }

  return false;
}

static void VideoInit(void) {
  retro_hw_context_type preferred = RETRO_HW_CONTEXT_NONE;
  environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred);

  if (preferred && SetHWRender(preferred))
    return;
  if (SetHWRender(RETRO_HW_CONTEXT_OPENGL_CORE))
    return;
  if (SetHWRender(RETRO_HW_CONTEXT_OPENGL))
    return;
  if (SetHWRender(RETRO_HW_CONTEXT_OPENGLES3))
    return;

  hw_render.context_type = RETRO_HW_CONTEXT_NONE;
}

static bool GetButtonState(unsigned id) {
  return input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id);
}

static float GetAxisState(unsigned index, unsigned id) {
  return input_state_cb(0, RETRO_DEVICE_ANALOG, index, id);
}

static void InputInit(void) {
  static const struct retro_controller_description controllers[] = {
    { "Nintendo 3DS", RETRO_DEVICE_JOYPAD },
    { NULL, 0 },
  };

  static const struct retro_controller_info ports[] = {
    { controllers, 1 },
    { NULL, 0 },
  };

  environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

  struct retro_input_descriptor desc[] = {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
    { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Circle Pad X" },
    { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Circle Pad Y" },
    { 0 },
  };

  environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &desc);
}

static std::string FetchVariable(std::string key, std::string def) {
  struct retro_variable var = { nullptr };
  var.key = key.c_str();

  if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || var.value == nullptr) {
    Helpers::warn("Fetching variable %s failed.", key);
    return def;
  }

  return std::string(var.value);
}

static bool FetchVariableBool(std::string key, bool def) {
  return FetchVariable(key, def ? "enabled" : "disabled") == "enabled";
}

static void ConfigInit() {
  static const retro_variable values[] = {
    { "panda3ds_use_shader_jit", "Enable shader JIT; enabled|disabled" },
    { "panda3ds_use_vsync", "Enable VSync; enabled|disabled" },
    { "panda3ds_dsp_emulation", "DSP emulation; Null|HLE|LLE" },
    { "panda3ds_use_audio", "Enable audio; disabled|enabled" },
    { "panda3ds_use_virtual_sd", "Enable virtual SD card; enabled|disabled" },
    { "panda3ds_write_protect_virtual_sd", "Write protect virtual SD card; disabled|enabled" },
    { "panda3ds_battery_level", "Battery percentage; 5|10|20|30|50|70|90|100" },
    { "panda3ds_use_charger", "Charger plugged; enabled|disabled" },
    { nullptr, nullptr }
  };

  environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)values);
}

static void ConfigUpdate() {
  EmulatorConfig& config = emulator->getConfig();

  config.rendererType      = RendererType::OpenGL;
  config.vsyncEnabled      = FetchVariableBool("panda3ds_use_vsync", true);
  config.shaderJitEnabled  = FetchVariableBool("panda3ds_use_shader_jit", true);
  config.chargerPlugged    = FetchVariableBool("panda3ds_use_charger", true);
  config.batteryPercentage = std::clamp(std::stoi(FetchVariable("panda3ds_battery_level", "5")), 0, 100);
  config.dspType           = Audio::DSPCore::typeFromString(FetchVariable("panda3ds_dsp_emulation", "null"));
  config.audioEnabled      = FetchVariableBool("panda3ds_use_audio", false);
  config.sdCardInserted    = FetchVariableBool("panda3ds_use_virtual_sd", true);
  config.sdWriteProtected  = FetchVariableBool("panda3ds_write_protect_virtual_sd", false);
  config.discordRpcEnabled = false;
}

void retro_get_system_info(retro_system_info* info) {
  info->need_fullpath    = true;
  info->valid_extensions = "3ds|3dsx|elf|axf|cci|cxi|app";
  info->library_version  = "0.8";
  info->library_name     = "Panda3DS";
  info->block_extract    = true;
}

void retro_get_system_av_info(retro_system_av_info* info) {
  info->geometry.base_width   = emulator->width;
  info->geometry.base_height  = emulator->height;

  info->geometry.max_width    = info->geometry.base_width;
  info->geometry.max_height   = info->geometry.base_height;

  info->geometry.aspect_ratio = 5.0 / 6.0;
  info->timing.fps            = 60.0;
  info->timing.sample_rate    = 32000;
}

void retro_set_environment(retro_environment_t cb) {
  environ_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
  video_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
  audio_batch_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
}

void retro_set_input_poll(retro_input_poll_t cb) {
  input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb) {
  input_state_cb = cb;
}

void retro_init(void) {
  enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;
  environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);

  emulator = std::make_unique<Emulator>();
}

void retro_deinit(void) {
  emulator = nullptr;
}

bool retro_load_game(const struct retro_game_info* game) {
  ConfigInit();
  ConfigUpdate();

  if (emulator->getRendererType() != RendererType::OpenGL) {
    throw std::runtime_error("Libretro: Renderer is not OpenGL");
  }

  renderer = static_cast<RendererGL*>(emulator->getRenderer());
  emulator->setOutputSize(emulator->width, emulator->height);

  InputInit();
  VideoInit();

  return emulator->loadROM(game->path);
}

bool retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t num) {
  return false;
}

void retro_unload_game(void) {
  renderer->setFBO(0);
  renderer = nullptr;
}

void retro_reset(void) {
  emulator->reset(Emulator::ReloadOption::Reload);
}

void retro_run(void) {
  renderer->setFBO(hw_render.get_current_framebuffer());
  renderer->resetStateManager();

  input_poll_cb();

  HIDService& hid = emulator->getServiceManager().getHID();

  hid.setKey(HID::Keys::A,      GetButtonState(RETRO_DEVICE_ID_JOYPAD_A));
  hid.setKey(HID::Keys::B,      GetButtonState(RETRO_DEVICE_ID_JOYPAD_B));
  hid.setKey(HID::Keys::X,      GetButtonState(RETRO_DEVICE_ID_JOYPAD_X));
  hid.setKey(HID::Keys::Y,      GetButtonState(RETRO_DEVICE_ID_JOYPAD_Y));
  hid.setKey(HID::Keys::L,      GetButtonState(RETRO_DEVICE_ID_JOYPAD_L));
  hid.setKey(HID::Keys::R,      GetButtonState(RETRO_DEVICE_ID_JOYPAD_R));
  hid.setKey(HID::Keys::Start,  GetButtonState(RETRO_DEVICE_ID_JOYPAD_START));
  hid.setKey(HID::Keys::Select, GetButtonState(RETRO_DEVICE_ID_JOYPAD_SELECT));
  hid.setKey(HID::Keys::Up,     GetButtonState(RETRO_DEVICE_ID_JOYPAD_UP));
  hid.setKey(HID::Keys::Down,   GetButtonState(RETRO_DEVICE_ID_JOYPAD_DOWN));
  hid.setKey(HID::Keys::Left,   GetButtonState(RETRO_DEVICE_ID_JOYPAD_LEFT));
  hid.setKey(HID::Keys::Right,  GetButtonState(RETRO_DEVICE_ID_JOYPAD_RIGHT));

  float x_left = GetAxisState(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
  float y_left = GetAxisState(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

  hid.setCirclepadX(x_left == 0 ? 0 : x_left < 0 ? -0x9C : 0x9C);
  hid.setCirclepadY(y_left == 0 ? 0 : y_left > 0 ? -0x9C : 0x9C);

  bool touch = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
  auto pos_x = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
  auto pos_y = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);

  auto new_x = static_cast<int>((pos_x + 0x7fff) / (float)(0x7fff * 2) * emulator->width);
  auto new_y = static_cast<int>((pos_y + 0x7fff) / (float)(0x7fff * 2) * emulator->height);

  auto off_x = 40;
  auto off_y = emulator->height / 2;

  bool scr_x = new_x >= off_x && new_x < emulator->width - off_x;
  bool scr_y = new_y >= off_y && new_y <= emulator->height;

  if (touch && scr_y && scr_x) {
    u16 x = static_cast<u16>(new_x - off_x);
    u16 y = static_cast<u16>(new_y - off_y);

    hid.setTouchScreenPress(x, y);
  } else {
    hid.releaseTouchScreen();
  }

  hid.updateInputs(emulator->getTicks());

  emulator->runFrame();
  video_cb(RETRO_HW_FRAME_BUFFER_VALID, emulator->width, emulator->height, 0);
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
}

size_t retro_serialize_size(void) {
  size_t size = 0;
  return size;
}

bool retro_serialize(void* data, size_t size) {
  return false;
}

bool retro_unserialize(const void* data, size_t size) {
  return false;
}

unsigned retro_get_region(void) {
  return RETRO_REGION_NTSC;
}

unsigned retro_api_version() {
  return RETRO_API_VERSION;
}

size_t retro_get_memory_size(unsigned id) {
  if (id == RETRO_MEMORY_SYSTEM_RAM) {
    return 0;
  }
  return 0;
}

void* retro_get_memory_data(unsigned id) {
  if (id == RETRO_MEMORY_SYSTEM_RAM) {
    return 0;
  }
  return NULL;
}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {
}

void retro_cheat_reset(void) {
}
