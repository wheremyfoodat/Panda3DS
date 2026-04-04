#ifdef IMGUI_FRONTEND

#include "panda_sdl/panda_fsui.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "IconsFontAwesome5.h"
#include "IconsPromptFont.h"
#include "helpers.hpp"
#include "config.hpp"
#include "emulator.hpp"
#include "frontend_settings.hpp"
#include "fsui/imgui_fullscreen.hpp"
#include "fsui/platform_sdl2.hpp"
#include "io_file.hpp"
#include "services/hid.hpp"
#include "services/region_codes.hpp"
#include "version.hpp"

namespace
{
	const std::array<const char*, 6> s_window_icon_names = {
		"Rpog",
		"Rsyn",
		"Rnap",
		"Rcow",
		"SkyEmu",
		"Runpog",
	};

	const std::array<FrontendSettings::WindowIcon, 6> s_window_icon_values = {
		FrontendSettings::WindowIcon::Rpog,
		FrontendSettings::WindowIcon::Rsyn,
		FrontendSettings::WindowIcon::Rnap,
		FrontendSettings::WindowIcon::Rcow,
		FrontendSettings::WindowIcon::SkyEmu,
		FrontendSettings::WindowIcon::Runpog,
	};

	const std::array<const char*, 4> s_screen_layout_names = {
		"Default",
		"Default Flipped",
		"Side By Side",
		"Side By Side Flipped",
	};

	const std::array<ScreenLayout::Layout, 4> s_screen_layout_values = {
		ScreenLayout::Layout::Default,
		ScreenLayout::Layout::DefaultFlipped,
		ScreenLayout::Layout::SideBySide,
		ScreenLayout::Layout::SideBySideFlipped,
	};

	const std::array<const char*, 3> s_dsp_names = {
		"Null",
		"Teakra",
		"HLE",
	};

	const std::array<Audio::DSPCore::Type, 3> s_dsp_values = {
		Audio::DSPCore::Type::Null,
		Audio::DSPCore::Type::Teakra,
		Audio::DSPCore::Type::HLE,
	};

	const std::array<const char*, 2> s_volume_curve_names = {
		"Cubic",
		"Linear",
	};

	const std::array<AudioDeviceConfig::VolumeCurve, 2> s_volume_curve_values = {
		AudioDeviceConfig::VolumeCurve::Cubic,
		AudioDeviceConfig::VolumeCurve::Linear,
	};

	template <typename T, size_t N>
	int findArrayIndex(const std::array<T, N>& values, const T& value, int fallback = 0)
	{
		for (int i = 0; i < static_cast<int>(N); i++) {
			if (values[static_cast<size_t>(i)] == value) {
				return i;
			}
		}
		return fallback;
	}

	template <size_t N>
	int findStringIndex(const std::array<const char*, N>& values, std::string_view value, int fallback = 0)
	{
		for (int i = 0; i < static_cast<int>(N); i++) {
			if (value == values[static_cast<size_t>(i)]) {
				return i;
			}
		}
		return fallback;
	}

	std::string toLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value;
	}

	std::string percentEncodeUrl(std::string_view value)
	{
		std::string encoded;
		encoded.reserve(value.size());
		for (unsigned char c : value) {
			const bool unreserved =
				(c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':';
			if (unreserved) {
				encoded.push_back(static_cast<char>(c));
			} else {
				static constexpr char hex[] = "0123456789ABCDEF";
				encoded.push_back('%');
				encoded.push_back(hex[c >> 4]);
				encoded.push_back(hex[c & 0x0F]);
			}
		}
		return encoded;
	}

	std::string fileUrlForPath(const std::filesystem::path& path)
	{
		std::error_code ec;
		const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
		const std::string generic = (ec || absolute.empty()) ? path.generic_string() : absolute.generic_string();
		std::string url = "file://";
		#ifdef _WIN32
		url.push_back('/');
		#endif
		url += percentEncodeUrl(generic);
		return url;
	}

	bool openUrl(std::string_view url)
	{
		const std::string url_string(url);
		if (SDL_OpenURL(url_string.c_str()) != 0) {
			Helpers::warn("Failed to open URL %s: %s", url_string.c_str(), SDL_GetError());
			return false;
		}
		return true;
	}

	bool openPathInBrowser(const std::filesystem::path& path)
	{
		return openUrl(fileUrlForPath(path));
	}

	const char* windowIconFilename(FrontendSettings::WindowIcon icon)
	{
		switch (icon) {
			case FrontendSettings::WindowIcon::Rsyn: return "rsyn_icon.png";
			case FrontendSettings::WindowIcon::Rnap: return "rnap_icon.png";
			case FrontendSettings::WindowIcon::Rcow: return "rcow_icon.png";
			case FrontendSettings::WindowIcon::SkyEmu: return "skyemu_icon.png";
			case FrontendSettings::WindowIcon::Runpog: return "runpog_icon.png";
			case FrontendSettings::WindowIcon::Rpog:
			default: return "rpog_icon.png";
		}
	}

	std::filesystem::path fsuiIconDirectory()
	{
		#ifdef PANDA3DS_FSUI_ICON_DIR
		return std::filesystem::path(PANDA3DS_FSUI_ICON_DIR);
		#else
		return {};
		#endif
	}

	std::string resolveFsuiAppIconPath(FrontendSettings::WindowIcon icon)
	{
		const std::string filename = windowIconFilename(icon);
		std::vector<std::filesystem::path> candidates;

		char* base_path = SDL_GetBasePath();
		if (base_path != nullptr) {
			const std::filesystem::path root(base_path);
			SDL_free(base_path);
			candidates.push_back(root / filename);
			candidates.push_back(root / "docs" / "img" / filename);
			candidates.push_back(root.parent_path() / "Resources" / filename);
			candidates.push_back(root.parent_path() / "Resources" / "docs" / "img" / filename);
		}

		const std::filesystem::path icon_dir = fsuiIconDirectory();
		if (!icon_dir.empty()) {
			candidates.push_back(icon_dir / filename);
		}

		for (const auto& candidate : candidates) {
			std::error_code ec;
			if (!candidate.empty() && std::filesystem::exists(candidate, ec)) {
				return candidate.string();
			}
		}

		return icon_dir.empty() ? filename : (icon_dir / filename).string();
	}

	void customizeInterfaceRows(std::vector<fsui::SettingsRowDescriptor>& rows)
	{
		for (auto& row : rows) {
			if (row.id == "fsui_prompt_icons") {
				row.summary = "Chooses which controller prompt glyph set the fullscreen UI should use.";
			} else if (row.id == "fsui_background_image") {
				row.summary = "Sets a fullscreen background image for the application UI.";
			}
		}
	}

	std::time_t fileTimeToTimeT(const std::filesystem::file_time_type& file_time)
	{
		using namespace std::chrono;
		const auto system_now = system_clock::now();
		const auto file_now = std::filesystem::file_time_type::clock::now();
		const auto adjusted = time_point_cast<system_clock::duration>(file_time - file_now + system_now);
		return system_clock::to_time_t(adjusted);
	}

	std::string regionToString(Regions region)
	{
		switch (region) {
			case Regions::Japan: return "Japan";
			case Regions::USA: return "North America";
			case Regions::Europe: return "Europe";
			case Regions::Australia: return "Australia";
			case Regions::China: return "China";
			case Regions::Korea: return "Korea";
			case Regions::Taiwan: return "Taiwan";
			default: return "Other";
		}
	}

	const char* regionToFlagAsset(Regions region)
	{
		switch (region) {
			case Regions::Japan: return "icons/flags/jp.png";
			case Regions::USA: return "icons/flags/us.png";
			case Regions::Europe: return "icons/flags/eu.png";
			case Regions::Australia: return "icons/flags/au.png";
			case Regions::China: return "icons/flags/cn.png";
			case Regions::Korea: return "icons/flags/kr.png";
			case Regions::Taiwan: return "icons/flags/tw.png";
			default: return "icons/flags/Other.png";
		}
	}

	bool readBytes(IOFile& file, std::uint64_t offset, void* out, size_t size)
	{
		if (!file.seek(static_cast<std::int64_t>(offset), SEEK_SET)) {
			return false;
		}
		auto [ok, read] = file.readBytes(out, size);
		return ok && read == size;
	}

	std::string readSmdhString(const std::vector<u8>& data, size_t offset, size_t size)
	{
		std::string result;
		result.reserve(size / 2);
		for (size_t i = offset; i + 1 < offset + size && i + 1 < data.size(); i += 2) {
			const std::uint16_t code = static_cast<std::uint16_t>(data[i] | (data[i + 1] << 8));
			if (code == 0) {
				break;
			}
			if (code < 0x80) {
				result.push_back(static_cast<char>(code));
			} else if (code < 0x800) {
				result.push_back(static_cast<char>(0xC0 | (code >> 6)));
				result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			} else {
				result.push_back(static_cast<char>(0xE0 | (code >> 12)));
				result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
				result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			}
		}
		return result;
	}
} // namespace

struct PandaFsuiAdapter::ParsedMetadata
{
	std::string title;
	std::string english_title;
	Regions region = Regions::USA;
	bool has_region = false;
	std::string title_id;
};

struct PandaFsuiAdapter::LanguageOption
{
	const char* label;
	const char* code;
};

PandaFsuiAdapter::PandaFsuiAdapter(SDL_Window* window_, Emulator& emu_) : window(window_), emu(emu_) {}

void PandaFsuiAdapter::setPauseCallback(std::function<void(bool)> callback)
{
	onPauseChange = std::move(callback);
}

void PandaFsuiAdapter::setVsyncCallback(std::function<void(bool)> callback)
{
	onVsyncChange = std::move(callback);
}

void PandaFsuiAdapter::setExitToSelectorCallback(std::function<void()> callback)
{
	onExitToSelector = std::move(callback);
}

void PandaFsuiAdapter::syncUiStateFromConfig()
{
	const EmulatorConfig& cfg = emu.getConfig();
	uiState.theme = cfg.fsuiTheme;
	uiState.prompt_icon_pack = fsui::PromptIconPackFromString(cfg.fsuiPromptIconPack);
	uiState.background_image_path = cfg.fsuiBackgroundImagePath;
	uiState.default_game_view = cfg.fsuiDefaultGameView;
	uiState.game_sort = cfg.fsuiGameSort;
	uiState.game_sort_reverse = cfg.fsuiGameSortReverse;
	uiState.game_list_paths = cfg.fsuiGameListPaths;
	uiState.game_list_recursive_paths = cfg.fsuiGameListRecursivePaths;
	uiState.covers_path = cfg.fsuiCoversPath.empty() ? (emu.getAppDataRoot() / "covers") : cfg.fsuiCoversPath;
	uiState.show_inputs_overlay = cfg.fsuiShowInputsOverlay;
	uiState.show_settings_overlay = cfg.fsuiShowSettingsOverlay;
	uiState.show_performance_overlay = cfg.fsuiShowPerformanceOverlay;
	fsuiContext.app_icon_path = resolveFsuiAppIconPath(cfg.frontendSettings.icon);
}

void PandaFsuiAdapter::persistUiState(bool reload)
{
	EmulatorConfig& cfg = emu.getConfig();
	cfg.fsuiTheme = uiState.theme;
	cfg.fsuiPromptIconPack = std::string(fsui::PromptIconPackToString(uiState.prompt_icon_pack));
	cfg.fsuiBackgroundImagePath = uiState.background_image_path;
	cfg.fsuiDefaultGameView = uiState.default_game_view;
	cfg.fsuiGameSort = uiState.game_sort;
	cfg.fsuiGameSortReverse = uiState.game_sort_reverse;
	cfg.fsuiGameListPaths = uiState.game_list_paths;
	cfg.fsuiGameListRecursivePaths = uiState.game_list_recursive_paths;
	cfg.fsuiCoversPath = uiState.covers_path;
	cfg.fsuiShowInputsOverlay = uiState.show_inputs_overlay;
	cfg.fsuiShowSettingsOverlay = uiState.show_settings_overlay;
	cfg.fsuiShowPerformanceOverlay = uiState.show_performance_overlay;
	cfg.save();
	if (reload) {
		emu.reloadSettings();
	}
}

std::vector<std::filesystem::path> PandaFsuiAdapter::currentScanRoots() const
{
	std::vector<std::filesystem::path> paths;
	paths.emplace_back("E:/");
	paths.emplace_back(std::filesystem::path("E:/") / "PANDA3DS");

	std::filesystem::path root_path;
	#ifdef __WINRT__
	char* sdl_path = SDL_GetPrefPath(nullptr, nullptr);
	#else
	char* sdl_path = SDL_GetBasePath();
	#endif
	if (sdl_path) {
		root_path = std::filesystem::path(sdl_path);
		SDL_free(sdl_path);
	}

	if (!root_path.empty()) {
		paths.push_back(root_path);
		paths.push_back(root_path / "PANDA3DS");
	}

	std::vector<std::filesystem::path> unique_paths;
	for (const auto& path : paths) {
		if (std::find(unique_paths.begin(), unique_paths.end(), path) == unique_paths.end()) {
			unique_paths.push_back(path);
		}
	}
	return unique_paths;
}

void PandaFsuiAdapter::seedGameListPathsIfNeeded()
{
	if (!uiState.game_list_paths.empty() || !uiState.game_list_recursive_paths.empty()) {
		return;
	}

	for (const auto& path : currentScanRoots()) {
		uiState.game_list_paths.push_back(path);
	}
}

bool PandaFsuiAdapter::hasSupportedExtension(const std::filesystem::path& path) const
{
	static const std::set<std::string> extensions = {
		".cci", ".3ds", ".cxi", ".app", ".ncch", ".elf", ".axf", ".3dsx",
	};
	return extensions.contains(toLower(path.extension().string()));
}

std::string PandaFsuiAdapter::formatPercent(float value) const
{
	char buffer[32] = {};
	std::snprintf(buffer, sizeof(buffer), "%.0f%%", value * 100.0f);
	return buffer;
}

std::string PandaFsuiAdapter::formatInteger(int value) const
{
	return std::to_string(value);
}

std::string PandaFsuiAdapter::formatTitleId(std::uint64_t program_id) const
{
	if (program_id == 0) {
		return {};
	}
	char buffer[17] = {};
	std::snprintf(buffer, sizeof(buffer), "%016" PRIX64, program_id);
	return buffer;
}

std::filesystem::path PandaFsuiAdapter::defaultCoverDirectory() const
{
	return uiState.covers_path.empty() ? (emu.getAppDataRoot() / "covers") : uiState.covers_path;
}

std::filesystem::path PandaFsuiAdapter::findCoverPath(const std::filesystem::path& rom_path, const std::string& title_id) const
{
	const std::filesystem::path covers_dir = defaultCoverDirectory();
	static const std::array<const char*, 4> cover_extensions = {".png", ".jpg", ".jpeg", ".bmp"};

	auto try_base = [&](const std::string& base) -> std::filesystem::path {
		if (base.empty()) {
			return {};
		}
		for (const char* ext : cover_extensions) {
			const std::filesystem::path candidate = covers_dir / (base + ext);
			if (std::filesystem::exists(candidate)) {
				return candidate;
			}
		}
		return {};
	};

	if (auto by_title_id = try_base(title_id); !by_title_id.empty()) {
		return by_title_id;
	}
	return try_base(rom_path.stem().string());
}

std::optional<PandaFsuiAdapter::ParsedMetadata> PandaFsuiAdapter::readMetadataForPath(const std::filesystem::path& path) const
{
	auto parseSmdh = [](const std::vector<u8>& smdh) -> ParsedMetadata {
		ParsedMetadata metadata;
		if (smdh.size() < 0x36C0) {
			return metadata;
		}
		if (smdh[0] != 'S' || smdh[1] != 'M' || smdh[2] != 'D' || smdh[3] != 'H') {
			return metadata;
		}

		const u32 region_masks = *reinterpret_cast<const u32*>(&smdh[0x2018]);
		const bool japan = (region_masks & 0x1) != 0;
		const bool north_america = (region_masks & 0x2) != 0;
		const bool europe = (region_masks & 0x4) != 0;
		const bool australia = (region_masks & 0x8) != 0;
		const bool china = (region_masks & 0x10) != 0;
		const bool korea = (region_masks & 0x20) != 0;
		const bool taiwan = (region_masks & 0x40) != 0;

		int meta_language = 1;
		if (north_america) {
			metadata.region = Regions::USA;
			metadata.has_region = true;
		} else if (europe) {
			metadata.region = Regions::Europe;
			metadata.has_region = true;
		} else if (australia) {
			metadata.region = Regions::Australia;
			metadata.has_region = true;
		} else if (japan) {
			metadata.region = Regions::Japan;
			metadata.has_region = true;
			meta_language = 0;
		} else if (korea) {
			metadata.region = Regions::Korea;
			metadata.has_region = true;
			meta_language = 7;
		} else if (china) {
			metadata.region = Regions::China;
			metadata.has_region = true;
			meta_language = 6;
		} else if (taiwan) {
			metadata.region = Regions::Taiwan;
			metadata.has_region = true;
			meta_language = 6;
		}

		metadata.english_title = readSmdhString(smdh, 0x8 + (512 * 1) + 0x80, 0x100);
		metadata.title = readSmdhString(smdh, 0x8 + (512 * meta_language) + 0x80, 0x100);
		if (metadata.title.empty()) {
			metadata.title = metadata.english_title;
		}
		return metadata;
	};

	auto readNcchMetadata = [&](IOFile& file, std::uint64_t base_offset) -> std::optional<ParsedMetadata> {
		std::array<u8, 0x200> header {};
		if (!readBytes(file, base_offset, header.data(), header.size())) {
			return std::nullopt;
		}
		if (header[0x100] != 'N' || header[0x101] != 'C' || header[0x102] != 'C' || header[0x103] != 'H') {
			return std::nullopt;
		}

		ParsedMetadata metadata;
		metadata.title_id = formatTitleId(*reinterpret_cast<u64*>(header.data() + 0x118));
		const std::uint64_t exefs_offset = base_offset + static_cast<std::uint64_t>(*reinterpret_cast<u32*>(header.data() + 0x1A0)) * 0x200ull;
		const std::uint64_t exefs_size = static_cast<std::uint64_t>(*reinterpret_cast<u32*>(header.data() + 0x1A4)) * 0x200ull;
		if (exefs_offset == base_offset || exefs_size < 0x200) {
			return metadata;
		}

		std::array<u8, 0x200> exefs_header {};
		if (!readBytes(file, exefs_offset, exefs_header.data(), exefs_header.size())) {
			return metadata;
		}

		for (int i = 0; i < 10; i++) {
			const u8* entry = exefs_header.data() + (i * 16);
			char name[9] = {};
			std::memcpy(name, entry, 8);
			const u32 file_offset = *reinterpret_cast<const u32*>(entry + 8);
			const u32 file_size = *reinterpret_cast<const u32*>(entry + 12);
			if (file_size == 0) {
				continue;
			}
			if (std::string_view(name) == "icon") {
				std::vector<u8> smdh(file_size);
				if (readBytes(file, exefs_offset + 0x200 + file_offset, smdh.data(), smdh.size())) {
					ParsedMetadata smdh_metadata = parseSmdh(smdh);
					if (!smdh_metadata.title.empty()) {
						metadata.title = std::move(smdh_metadata.title);
					}
					if (!smdh_metadata.english_title.empty()) {
						metadata.english_title = std::move(smdh_metadata.english_title);
					}
					if (smdh_metadata.has_region) {
						metadata.region = smdh_metadata.region;
						metadata.has_region = true;
					}
				}
				break;
			}
		}

		return metadata;
	};

	IOFile file;
	if (!file.open(path, "rb")) {
		return std::nullopt;
	}

	const std::string ext = toLower(path.extension().string());
	if (ext == ".cxi" || ext == ".app" || ext == ".ncch") {
		return readNcchMetadata(file, 0);
	}

	if (ext == ".3ds" || ext == ".cci") {
		std::array<u8, 0x200> header {};
		if (!readBytes(file, 0, header.data(), header.size())) {
			return std::nullopt;
		}
		if (header[0x100] != 'N' || header[0x101] != 'C' || header[0x102] != 'S' || header[0x103] != 'D') {
			return std::nullopt;
		}
		for (int i = 0; i < 8; i++) {
			const size_t entry_offset = 0x120 + i * 8;
			const std::uint64_t partition_offset = static_cast<std::uint64_t>(*reinterpret_cast<u32*>(header.data() + entry_offset)) * 0x200ull;
			const std::uint64_t partition_size = static_cast<std::uint64_t>(*reinterpret_cast<u32*>(header.data() + entry_offset + 4)) * 0x200ull;
			if (partition_offset != 0 && partition_size != 0) {
				return readNcchMetadata(file, partition_offset);
			}
		}
	}

	return std::nullopt;
}

fsui::GameEntry PandaFsuiAdapter::buildGameListEntry(const std::filesystem::directory_entry& entry) const
{
	fsui::GameEntry item;
	item.path = entry.path();
	item.file_size = entry.file_size();
	item.modified_time = fileTimeToTimeT(entry.last_write_time());
	item.title = entry.path().stem().string();
	item.english_title = item.title;
	item.title_sort = toLower(item.title);

	const std::string extension = toLower(entry.path().extension().string());
	if (extension == ".elf" || extension == ".axf" || extension == ".3dsx") {
		item.type = fsui::GameEntryType::Homebrew;
	} else {
		item.type = fsui::GameEntryType::Cartridge;
	}

	if (auto metadata = readMetadataForPath(entry.path()); metadata.has_value()) {
		if (!metadata->title.empty()) {
			item.title = metadata->title;
			item.title_sort = toLower(metadata->title);
		}
		if (!metadata->english_title.empty()) {
			item.english_title = metadata->english_title;
		}
		item.title_id = metadata->title_id;
		if (metadata->has_region) {
			item.region = regionToString(metadata->region);
			item.region_flag_path = regionToFlagAsset(metadata->region);
		}
	}

	item.cover_path = findCoverPath(item.path, item.title_id);
	return item;
}

std::vector<fsui::GameEntry> PandaFsuiAdapter::buildGameList()
{
	seedGameListPathsIfNeeded();
	std::vector<fsui::GameEntry> items;
	std::set<std::filesystem::path> seen;

	auto scan_directory = [&](const std::filesystem::path& root, bool recursive) {
		std::error_code ec;
		if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
			return;
		}

		if (recursive) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
				if (ec || !entry.is_regular_file()) {
					continue;
				}
				if (!hasSupportedExtension(entry.path()) || !seen.insert(entry.path()).second) {
					continue;
				}
				items.push_back(buildGameListEntry(entry));
			}
		} else {
			for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
				if (ec || !entry.is_regular_file()) {
					continue;
				}
				if (!hasSupportedExtension(entry.path()) || !seen.insert(entry.path()).second) {
					continue;
				}
				items.push_back(buildGameListEntry(entry));
			}
		}
	};

	for (const auto& path : uiState.game_list_paths) {
		scan_directory(path, false);
	}
	for (const auto& path : uiState.game_list_recursive_paths) {
		scan_directory(path, true);
	}
	return items;
}

fsui::CurrentGameInfo PandaFsuiAdapter::buildCurrentGameInfo()
{
	fsui::CurrentGameInfo info;
	const auto& rom_path = emu.getROMPath();
	if (!rom_path.has_value()) {
		info.title = "No Game Loaded";
		info.subtitle = "No title is currently running.";
		return info;
	}

	info.has_game = true;
	info.path = *rom_path;
	info.title = rom_path->stem().string();
	info.subtitle = rom_path->filename().string();
	auto it = std::find_if(cachedGameList.begin(), cachedGameList.end(), [&](const fsui::GameEntry& entry) { return entry.path == *rom_path; });
	if (it != cachedGameList.end()) {
		info.title = it->title;
		info.title_id = it->title_id;
	}
	return info;
}

std::string PandaFsuiAdapter::currentGameTitle() const
{
	const auto& rom_path = emu.getROMPath();
	return rom_path.has_value() ? rom_path->stem().string() : "No Game Loaded";
}

std::string PandaFsuiAdapter::currentGameSubtitle() const
{
	const auto& rom_path = emu.getROMPath();
	return rom_path.has_value() ? rom_path->filename().string() : "No title is currently running.";
}

std::vector<fsui::OverlayTextLine> PandaFsuiAdapter::buildPerformanceOverlayLines() const
{
	char fps[64] = {};
	std::snprintf(fps, sizeof(fps), "FPS: %.1f", ImGui::GetIO().Framerate);
	return {
		fsui::OverlayTextLine{.text = fps},
		fsui::OverlayTextLine{.text = "Renderer: OpenGL 4.1"},
		fsui::OverlayTextLine{.text = std::string("Version: ") + PANDA3DS_VERSION},
	};
}

std::vector<fsui::OverlayTextLine> PandaFsuiAdapter::buildSettingsOverlayLines() const
{
	const EmulatorConfig& cfg = emu.getConfig();
	return {
		fsui::OverlayTextLine{.text = std::string("VSync: ") + (cfg.vsyncEnabled ? "On" : "Off")},
		fsui::OverlayTextLine{.text = std::string("Layout: ") + s_screen_layout_names[static_cast<size_t>(findArrayIndex(s_screen_layout_values, cfg.screenLayout))]},
		fsui::OverlayTextLine{.text = std::string("Shader JIT: ") + (cfg.shaderJitEnabled ? "On" : "Off")},
		fsui::OverlayTextLine{.text = std::string("Ubershaders: ") + (cfg.useUbershaders ? "On" : "Off")},
	};
}

std::vector<fsui::InputOverlayDeviceState> PandaFsuiAdapter::buildInputOverlayDevices() const
{
	HIDService& hid = emu.getServiceManager().getHID();
	const u32 buttons = hid.getOldButtons();
	auto button_value = [buttons](u32 mask) { return (buttons & mask) ? 1.0f : 0.0f; };

	fsui::InputOverlayDeviceState device;
	device.title = "3DS";
	device.bindings = {
		{.label = "Up", .glyph = ICON_PF_DPAD_UP, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::Up)},
		{.label = "Down", .glyph = ICON_PF_DPAD_DOWN, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::Down)},
		{.label = "Left", .glyph = ICON_PF_DPAD_LEFT, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::Left)},
		{.label = "Right", .glyph = ICON_PF_DPAD_RIGHT, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::Right)},
		{.label = "A", .glyph = ICON_PF_BUTTON_A, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::A)},
		{.label = "B", .glyph = ICON_PF_BUTTON_B, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::B)},
		{.label = "X", .glyph = ICON_PF_BUTTON_X, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::X)},
		{.label = "Y", .glyph = ICON_PF_BUTTON_Y, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::Y)},
		{.label = "L", .glyph = ICON_PF_LEFT_SHOULDER_L1, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::L)},
		{.label = "R", .glyph = ICON_PF_RIGHT_SHOULDER_R1, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::R)},
		{.label = "ZL", .glyph = ICON_PF_LEFT_TRIGGER_ZL, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::ZL)},
		{.label = "ZR", .glyph = ICON_PF_RIGHT_TRIGGER_ZR, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::ZR)},
		{.label = "Start", .glyph = ICON_PF_START, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::Start)},
		{.label = "Select", .glyph = ICON_PF_SELECT_SHARE, .kind = fsui::InputOverlayBindingKind::Button, .value = button_value(HID::Keys::Select)},
		{.label = "CP X", .kind = fsui::InputOverlayBindingKind::Axis, .value = static_cast<float>(hid.getCirclepadX()) / 156.0f},
		{.label = "CP Y", .kind = fsui::InputOverlayBindingKind::Axis, .value = static_cast<float>(hid.getCirclepadY()) / 156.0f},
		{.label = "C X", .kind = fsui::InputOverlayBindingKind::Axis, .value = static_cast<float>(hid.getCStickX()) / 156.0f},
		{.label = "C Y", .kind = fsui::InputOverlayBindingKind::Axis, .value = static_cast<float>(hid.getCStickY()) / 156.0f},
	};
	return {device};
}

void PandaFsuiAdapter::requestLaunchPath(const std::filesystem::path& path)
{
	pendingLaunchPath = path;
	closeSelectorRequested = true;
}

void PandaFsuiAdapter::requestClassicUi(bool open_settings)
{
	emu.getConfig().frontendSettings.enableFullscreenUI = false;
	emu.getConfig().save();
	pendingClassicUiRequest = open_settings ? ClassicUiRequest::OpenSettings : ClassicUiRequest::Return;
}

void PandaFsuiAdapter::openUnsupportedPrompt(std::string title, std::string message) const
{
	ImGuiFullscreen::OpenInfoMessageDialog(std::move(title), std::move(message));
}

void PandaFsuiAdapter::openFileAndLaunch()
{
	ImGuiFullscreen::OpenFileSelector(
		"Start File",
		false,
		[this](const std::string& path) {
			if (!path.empty()) {
				requestLaunchPath(path);
				closeSelectorRequested = true;
			}
			ImGuiFullscreen::CloseFileSelector();
		},
		{".cci", ".3ds", ".cxi", ".app", ".ncch", ".elf", ".axf", ".3dsx"}
	);
}

std::vector<fsui::MenuItemDescriptor> PandaFsuiAdapter::buildLandingItems()
{
	return {
		{
			.id = "game_list",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::GameList),
			.title = "Game List",
			.summary = "Launch a game from images scanned from your game directories.",
			.on_activate = []() { fsui::ShowGameListWindow(); },
		},
		{
			.id = "start_game",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::StartGame),
			.title = "Start Game",
			.summary = "Launch a game by selecting a file image.",
			.on_activate = []() { fsui::ShowStartGameWindow(); },
		},
		{
			.id = "settings",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::Settings),
			.title = "Settings",
			.summary = "Changes settings for the application.",
			.on_activate = []() { fsui::SwitchToSettings(); },
		},
		{
			.id = "exit",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::Exit),
			.title = "Exit",
			.summary = "Exit the application.",
			.on_activate = []() { fsui::ShowExitWindow(); },
		},
	};
}

std::vector<fsui::MenuItemDescriptor> PandaFsuiAdapter::buildStartItems()
{
	return {
		{
			.id = "start_file",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::StartFile),
			.title = "Start File",
			.summary = "Launch a game by selecting a file image.",
			.on_activate = [this]() { openFileAndLaunch(); },
		},
		{
			.id = "back",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::Back),
			.title = "Back",
			.summary = "Return to the previous menu.",
			.on_activate = []() { fsui::ShowLandingWindow(); },
		},
	};
}

std::vector<fsui::MenuItemDescriptor> PandaFsuiAdapter::buildExitItems()
{
	return {
		{
			.id = "back",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::Back),
			.title = "Back",
			.summary = "Return to the previous menu.",
			.on_activate = []() { fsui::ShowLandingWindow(); },
		},
		{
			.id = "exit_app",
			.icon_path = fsui::ResolveStartupIconPath(fsuiContext, fsui::BuiltInStartupIcon::Exit),
			.title = "Exit Alber",
			.summary = "Completely exits the application.",
			.on_activate = []() {
				SDL_Event quit {};
				quit.type = SDL_QUIT;
				SDL_PushEvent(&quit);
			},
		},
		{
			.id = "desktop_mode",
			.icon_path = "fullscreenui/desktop-mode.png",
			.title = "Desktop Mode",
			.summary = "Return to a desktop-style interface.",
			.on_activate = [this]() { requestClassicUi(false); },
		},
	};
}

std::vector<fsui::MenuItemDescriptor> PandaFsuiAdapter::buildPauseItems()
{
	return {
		{
			.id = "resume",
			.title = ICON_FA_PLAY " Resume Game",
			.summary = "Return to the running game.",
			.on_activate = []() { fsui::ReturnToMainWindow(); },
		},
		{
			.id = "settings",
			.title = ICON_FA_SLIDERS_H " Settings",
			.summary = "Open per-game fullscreen settings.",
			.on_activate = []() { fsui::SwitchToSettings(); },
		},
		{
			.id = "close_game",
			.title = ICON_FA_POWER_OFF " Close Game",
			.summary = "Reset or close the running game.",
			.children = {
				{
					.id = "reset_system",
					.title = ICON_FA_SYNC " Reset System",
					.summary = "Reset the currently running title.",
					.on_activate = [this]() {
						emu.reset(Emulator::ReloadOption::Reload);
						fsui::ReturnToMainWindow();
					},
				},
				{
					.id = "exit_without_saving",
					.title = ICON_FA_POWER_OFF " Exit Without Saving",
					.summary = "Close the game immediately.",
					.on_activate = [this]() {
						if (onExitToSelector) {
							onExitToSelector();
						} else {
							SDL_Event quit {};
							quit.type = SDL_QUIT;
							SDL_PushEvent(&quit);
						}
					},
				},
			},
		},
	};
}

std::vector<fsui::MenuItemDescriptor> PandaFsuiAdapter::buildGameLaunchOptions(const fsui::GameEntry& entry)
{
	return {
		{
			.id = "resume_game",
			.title = "Resume Game",
			.summary = "Launch the selected title.",
			.on_activate = [this, path = entry.path]() { requestLaunchPath(path); },
		},
		{
			.id = "default_boot",
			.title = "Default Boot",
			.summary = "Launch the selected title with default boot behavior.",
			.on_activate = [this, path = entry.path]() { requestLaunchPath(path); },
		},
	};
}

std::vector<fsui::SettingsPageDescriptor> PandaFsuiAdapter::buildSettingsPages(fsui::SettingsScope scope)
{
	const std::array<LanguageOption, 12> language_options = {{
		{"English (en)", "en"},
		{"Japanese (ja)", "ja"},
		{"French (fr)", "fr"},
		{"German (de)", "de"},
		{"Italian (it)", "it"},
		{"Spanish (es)", "es"},
		{"Chinese (zh)", "zh"},
		{"Korean (ko)", "ko"},
		{"Dutch (nl)", "nl"},
		{"Portuguese (pt)", "pt"},
		{"Russian (ru)", "ru"},
		{"Taiwanese (tw)", "tw"},
	}};

	std::vector<fsui::SettingsPageDescriptor> pages;
	if (scope == fsui::SettingsScope::PerGame) {
		pages.push_back({
			.id = "summary",
			.scope = scope,
			.built_in_page = fsui::BuiltInSettingsPage::Summary,
			.build_rows = [this]() {
				std::vector<fsui::SettingsRowDescriptor> rows;
				rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Current Game"});
				const fsui::CurrentGameInfo info = buildCurrentGameInfo();
				rows.push_back({.kind = fsui::SettingsRowKind::Value, .title = ICON_FA_INFO " Title", .summary = "The title currently loaded in Panda3DS.", .value = info.title, .enabled = false});
				rows.push_back({.kind = fsui::SettingsRowKind::Value, .title = ICON_FA_INFO_CIRCLE " Subtitle", .summary = "The current ROM filename.", .value = info.subtitle, .enabled = false});
				if (!info.title_id.empty()) {
					rows.push_back({.kind = fsui::SettingsRowKind::Value, .title = ICON_FA_INFO_CIRCLE " Title ID", .summary = "The detected title ID for the current game.", .value = info.title_id, .enabled = false});
				}
				if (!info.path.empty()) {
					rows.push_back({.kind = fsui::SettingsRowKind::Value, .title = ICON_FA_FOLDER_OPEN " Path", .summary = "The full filesystem path to the current game.", .value = info.path.string(), .enabled = false});
				}
				rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Related"});
				rows.push_back({
					.kind = fsui::SettingsRowKind::Action,
					.title = ICON_FA_LIST_ALT " Open Game List Settings",
					.summary = "Configure directories, sort, and cover settings.",
					.on_activate = []() { fsui::ShowGameListSettingsWindow(); },
				});
				return rows;
			},
		});
	}

	pages.push_back({
		.id = "interface",
		.scope = fsui::SettingsScope::Global,
		.built_in_page = fsui::BuiltInSettingsPage::Interface,
		.build_rows = [this, language_options]() {
			EmulatorConfig& cfg = emu.getConfig();
			std::vector<fsui::SettingsRowDescriptor> rows = fsui::BuildStandardInterfaceRows();
			customizeInterfaceRows(rows);

			const int icon_index = findArrayIndex(s_window_icon_values, cfg.frontendSettings.icon);
			std::vector<fsui::SettingsChoiceOption> icon_choices;
			for (int i = 0; i < static_cast<int>(s_window_icon_names.size()); i++) {
				icon_choices.push_back({s_window_icon_names[static_cast<size_t>(i)], i == icon_index});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_FA_INFO_CIRCLE " Window Icon",
				.summary = "Selects the application icon used by the desktop window.",
				.value = s_window_icon_names[static_cast<size_t>(icon_index)],
				.dialog_title = ICON_FA_INFO_CIRCLE " Window Icon",
				.choices = icon_choices,
				.on_choice = [this](int index) {
					emu.getConfig().frontendSettings.icon = s_window_icon_values[static_cast<size_t>(index)];
					emu.getConfig().save();
				},
			});

			rows.push_back({
				.kind = fsui::SettingsRowKind::Toggle,
				.title = ICON_FA_LIST_ALT " Enable Fullscreen UI",
				.summary = "Uses the fullscreen interface instead of the classic centered ImGui UI.",
				.toggle_value = cfg.frontendSettings.enableFullscreenUI,
				.on_toggle = [this](bool value) {
					emu.getConfig().frontendSettings.enableFullscreenUI = value;
					emu.getConfig().save();
					if (!value) {
						requestClassicUi(true);
					}
				},
			});
			rows.push_back({
				.kind = fsui::SettingsRowKind::Toggle,
				.title = ICON_FA_INFO_CIRCLE " Stretch Output To Window",
				.summary = "Stretches the emulator output to match the current window size.",
				.toggle_value = cfg.frontendSettings.stretchImGuiOutputToWindow,
				.on_toggle = [this](bool value) {
					emu.getConfig().frontendSettings.stretchImGuiOutputToWindow = value;
					emu.getConfig().save();
				},
			});
			rows.push_back({
				.kind = fsui::SettingsRowKind::Toggle,
				.title = ICON_FA_INFO_CIRCLE " Show ImGui Debug Panel",
				.summary = "Displays the classic debugging and diagnostics panel.",
				.toggle_value = cfg.frontendSettings.showImGuiDebugPanel,
				.on_toggle = [this](bool value) {
					emu.getConfig().frontendSettings.showImGuiDebugPanel = value;
					emu.getConfig().save();
				},
			});
			rows.push_back({
				.kind = fsui::SettingsRowKind::Toggle,
				.title = ICON_FA_INFO_CIRCLE " Show App Version On Window",
				.summary = "Adds the Panda3DS version string to the desktop window title.",
				.toggle_value = cfg.windowSettings.showAppVersion,
				.on_toggle = [this](bool value) {
					emu.getConfig().windowSettings.showAppVersion = value;
					emu.getConfig().save();
				},
			});
			rows.push_back({
				.kind = fsui::SettingsRowKind::Toggle,
				.title = ICON_FA_INFO_CIRCLE " Remember Window Position",
				.summary = "Restores the previous desktop window size and position on launch.",
				.toggle_value = cfg.windowSettings.rememberPosition,
				.on_toggle = [this](bool value) {
					emu.getConfig().windowSettings.rememberPosition = value;
					emu.getConfig().save();
				},
			});

			rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Behaviour"});
			const char* current_language = EmulatorConfig::languageCodeToString(cfg.systemLanguage);
			int language_index = 0;
			for (int i = 0; i < static_cast<int>(language_options.size()); i++) {
				if (std::strcmp(language_options[static_cast<size_t>(i)].code, current_language) == 0) {
					language_index = i;
					break;
				}
			}
			std::vector<fsui::SettingsChoiceOption> language_choices;
			for (int i = 0; i < static_cast<int>(language_options.size()); i++) {
				language_choices.push_back({language_options[static_cast<size_t>(i)].label, i == language_index});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_PF_KEYBOARD_ALT " System Language",
				.summary = "Selects the emulated system language used by software.",
				.value = language_options[static_cast<size_t>(language_index)].label,
				.dialog_title = ICON_PF_KEYBOARD_ALT " System Language",
				.choices = language_choices,
				.on_choice = [this, language_options](int index) {
					emu.getConfig().systemLanguage = EmulatorConfig::languageCodeFromString(language_options[static_cast<size_t>(index)].code);
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			rows.push_back({
				.kind = fsui::SettingsRowKind::Toggle,
				.title = ICON_FA_INFO_CIRCLE " Print App Version",
				.summary = "Prints the application version information on startup.",
				.toggle_value = cfg.printAppVersion,
				.on_toggle = [this](bool value) {
					emu.getConfig().printAppVersion = value;
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			rows.push_back({
				.kind = fsui::SettingsRowKind::Toggle,
				.title = ICON_FA_INFO_CIRCLE " Enable Discord RPC",
				.summary = "Enables Discord rich presence support when available.",
				.toggle_value = cfg.discordRpcEnabled,
				.on_toggle = [this](bool value) {
					emu.getConfig().discordRpcEnabled = value;
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			return rows;
		},
	});

	pages.push_back({
		.id = "emulation",
		.scope = scope,
		.built_in_page = fsui::BuiltInSettingsPage::Emulation,
		.build_rows = [this]() {
			EmulatorConfig& cfg = emu.getConfig();
			std::vector<fsui::SettingsRowDescriptor> rows;
			rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Core"});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_GAMEPAD_ALT " Enable Circle Pad Pro", .summary = "Enables Circle Pad Pro emulation for games that support it.", .toggle_value = cfg.circlePadProEnabled, .on_toggle = [this](bool value) { emu.getConfig().circlePadProEnabled = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_MICROCHIP " Enable Fastmem", .summary = "Uses host fast memory mappings for improved performance where supported.", .toggle_value = cfg.fastmemEnabled, .on_toggle = [this](bool value) { emu.getConfig().fastmemEnabled = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_ARCHIVE " Use Portable Build", .summary = "Stores emulator data relative to the application instead of the user profile directory.", .toggle_value = cfg.usePortableBuild, .on_toggle = [this](bool value) { emu.getConfig().usePortableBuild = value; emu.getConfig().save(); emu.reloadSettings(); }});

			rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Power"});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_POWER_OFF " Charger Plugged", .summary = "Controls the emulated charging state reported to software.", .toggle_value = cfg.chargerPlugged, .on_toggle = [this](bool value) { emu.getConfig().chargerPlugged = value; emu.getConfig().save(); emu.reloadSettings(); }});
			std::vector<fsui::SettingsChoiceOption> battery_choices;
			for (int value = 0; value <= 100; value += 5) {
				battery_choices.push_back({std::to_string(value), value == cfg.batteryPercentage});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_FA_INFO_CIRCLE " Battery Percentage",
				.summary = "Sets the emulated battery level reported to software.",
				.value = formatInteger(cfg.batteryPercentage),
				.dialog_title = ICON_FA_INFO_CIRCLE " Battery Percentage",
				.choices = battery_choices,
				.on_choice = [this](int index) {
					emu.getConfig().batteryPercentage = index * 5;
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});

			rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Storage"});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_SAVE " Use Virtual SD", .summary = "Controls whether the emulated SD card is inserted.", .toggle_value = cfg.sdCardInserted, .on_toggle = [this](bool value) { emu.getConfig().sdCardInserted = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_ARCHIVE " Write Protect Virtual SD", .summary = "Marks the emulated SD card as write-protected.", .toggle_value = cfg.sdWriteProtected, .on_toggle = [this](bool value) { emu.getConfig().sdWriteProtected = value; emu.getConfig().save(); emu.reloadSettings(); }});
			return rows;
		},
	});

	pages.push_back({
		.id = "graphics",
		.scope = scope,
		.built_in_page = fsui::BuiltInSettingsPage::Graphics,
		.build_rows = [this]() {
			EmulatorConfig& cfg = emu.getConfig();
			std::vector<fsui::SettingsRowDescriptor> rows;
			rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Renderer"});
			rows.push_back({.kind = fsui::SettingsRowKind::Value, .title = ICON_PF_PICTURE " Renderer", .summary = "The active renderer backend for the SDL frontend.", .value = "OpenGL 4.1", .enabled = false});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_INFO_CIRCLE " Enable VSync", .summary = "Synchronizes presentation to the display refresh rate.", .toggle_value = cfg.vsyncEnabled, .on_toggle = [this](bool value) { emu.getConfig().vsyncEnabled = value; emu.getConfig().save(); if (onVsyncChange) onVsyncChange(value); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_MICROCHIP " Enable Shader JIT", .summary = "Uses the shader JIT to improve GPU performance.", .toggle_value = cfg.shaderJitEnabled, .on_toggle = [this](bool value) { emu.getConfig().shaderJitEnabled = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_PICTURE " Use Ubershaders", .summary = "Uses ubershaders for improved compatibility at a performance cost.", .toggle_value = cfg.useUbershaders, .on_toggle = [this](bool value) { emu.getConfig().useUbershaders = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_PICTURE " Accurate Shader Multiplication", .summary = "Uses a slower but more accurate multiplication mode in shaders.", .toggle_value = cfg.accurateShaderMul, .on_toggle = [this](bool value) { emu.getConfig().accurateShaderMul = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_PICTURE " Accelerate Shaders", .summary = "Uses accelerated shader compilation paths where available.", .toggle_value = cfg.accelerateShaders, .on_toggle = [this](bool value) { emu.getConfig().accelerateShaders = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_EXCLAMATION_TRIANGLE " Force Shadergen For Lighting", .summary = "Forces the shader generator path when enough lights are active.", .toggle_value = cfg.forceShadergenForLights, .on_toggle = [this](bool value) { emu.getConfig().forceShadergenForLights = value; emu.getConfig().save(); emu.reloadSettings(); }});

			std::vector<fsui::SettingsChoiceOption> threshold_choices;
			for (int value = 0; value <= 8; value++) {
				threshold_choices.push_back({std::to_string(value), value == cfg.lightShadergenThreshold});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_FA_INFO_CIRCLE " Lighting Threshold",
				.summary = "Number of active lights before shadergen is forced.",
				.value = formatInteger(cfg.lightShadergenThreshold),
				.dialog_title = ICON_FA_INFO_CIRCLE " Lighting Threshold",
				.choices = threshold_choices,
				.on_choice = [this](int index) {
					emu.getConfig().lightShadergenThreshold = index;
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_ARCHIVE " Hash Textures", .summary = "Hashes textures to improve texture replacement matching.", .toggle_value = cfg.hashTextures, .on_toggle = [this](bool value) { emu.getConfig().hashTextures = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_EXCLAMATION_TRIANGLE " Enable RenderDoc", .summary = "Enables RenderDoc capture support for graphics debugging.", .toggle_value = cfg.enableRenderdoc, .on_toggle = [this](bool value) { emu.getConfig().enableRenderdoc = value; emu.getConfig().save(); emu.reloadSettings(); }});

			rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Layout"});
			const int layout_index = findArrayIndex(s_screen_layout_values, cfg.screenLayout);
			std::vector<fsui::SettingsChoiceOption> layout_choices;
			for (int i = 0; i < static_cast<int>(s_screen_layout_names.size()); i++) {
				layout_choices.push_back({s_screen_layout_names[static_cast<size_t>(i)], i == layout_index});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_FA_TH " Screen Layout",
				.summary = "Chooses how the 3DS screens are arranged in the SDL window.",
				.value = s_screen_layout_names[static_cast<size_t>(layout_index)],
				.dialog_title = ICON_FA_TH " Screen Layout",
				.choices = layout_choices,
				.on_choice = [this](int index) {
					emu.getConfig().screenLayout = s_screen_layout_values[static_cast<size_t>(index)];
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			std::vector<fsui::SettingsChoiceOption> top_screen_choices;
			for (int value = 0; value <= 100; value += 5) {
				top_screen_choices.push_back({std::to_string(value) + "%", value == static_cast<int>(std::lround(cfg.topScreenSize * 100.0f))});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_FA_INFO_CIRCLE " Top Screen Size",
				.summary = "Adjusts the size ratio of the top screen in split layouts.",
				.value = formatPercent(cfg.topScreenSize),
				.dialog_title = ICON_FA_INFO_CIRCLE " Top Screen Size",
				.choices = top_screen_choices,
				.on_choice = [this](int index) {
					emu.getConfig().topScreenSize = static_cast<float>(index) / 20.0f;
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			return rows;
		},
	});

	pages.push_back({
		.id = "audio",
		.scope = scope,
		.built_in_page = fsui::BuiltInSettingsPage::Audio,
		.build_rows = [this]() {
			EmulatorConfig& cfg = emu.getConfig();
			std::vector<fsui::SettingsRowDescriptor> rows;
			rows.push_back({.kind = fsui::SettingsRowKind::Heading, .title = "Output"});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_SOUND " Enable Audio", .summary = "Enables or disables audio output entirely.", .toggle_value = cfg.audioEnabled, .on_toggle = [this](bool value) { emu.getConfig().audioEnabled = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_VOLUME_MUTE " Mute Audio", .summary = "Mutes audio output without changing the configured volume.", .toggle_value = cfg.audioDeviceConfig.muteAudio, .on_toggle = [this](bool value) { emu.getConfig().audioDeviceConfig.muteAudio = value; emu.getConfig().save(); emu.reloadSettings(); }});

			std::vector<fsui::SettingsChoiceOption> volume_choices;
			for (int value = 0; value <= 200; value += 10) {
				volume_choices.push_back({std::to_string(value) + "%", value == static_cast<int>(std::lround(cfg.audioDeviceConfig.volumeRaw * 100.0f))});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_FA_VOLUME_UP " Audio Volume",
				.summary = "Sets the master audio volume for the SDL frontend.",
				.value = formatPercent(cfg.audioDeviceConfig.volumeRaw * 0.5f),
				.dialog_title = ICON_FA_VOLUME_UP " Audio Volume",
				.choices = volume_choices,
				.on_choice = [this](int index) {
					emu.getConfig().audioDeviceConfig.volumeRaw = static_cast<float>(index) / 10.0f;
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_PF_SOUND " Enable AAC Audio", .summary = "Enables AAC decoding for titles which use AAC streams.", .toggle_value = cfg.aacEnabled, .on_toggle = [this](bool value) { emu.getConfig().aacEnabled = value; emu.getConfig().save(); emu.reloadSettings(); }});
			rows.push_back({.kind = fsui::SettingsRowKind::Toggle, .title = ICON_FA_INFO_CIRCLE " Print DSP Firmware", .summary = "Prints the DSP firmware information during initialization.", .toggle_value = cfg.printDSPFirmware, .on_toggle = [this](bool value) { emu.getConfig().printDSPFirmware = value; emu.getConfig().save(); emu.reloadSettings(); }});

			const int dsp_index = findArrayIndex(s_dsp_values, cfg.dspType);
			std::vector<fsui::SettingsChoiceOption> dsp_choices;
			for (int i = 0; i < static_cast<int>(s_dsp_names.size()); i++) {
				dsp_choices.push_back({s_dsp_names[static_cast<size_t>(i)], i == dsp_index});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_PF_MICROCHIP " DSP Emulation",
				.summary = "Selects the DSP emulation backend used by Panda3DS.",
				.value = s_dsp_names[static_cast<size_t>(dsp_index)],
				.dialog_title = ICON_PF_MICROCHIP " DSP Emulation",
				.choices = dsp_choices,
				.on_choice = [this](int index) {
					emu.getConfig().dspType = s_dsp_values[static_cast<size_t>(index)];
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});

			const int curve_index = findArrayIndex(s_volume_curve_values, cfg.audioDeviceConfig.volumeCurve);
			std::vector<fsui::SettingsChoiceOption> curve_choices;
			for (int i = 0; i < static_cast<int>(s_volume_curve_names.size()); i++) {
				curve_choices.push_back({s_volume_curve_names[static_cast<size_t>(i)], i == curve_index});
			}
			rows.push_back({
				.kind = fsui::SettingsRowKind::Choice,
				.title = ICON_FA_INFO_CIRCLE " Volume Curve",
				.summary = "Chooses how the user-facing volume value maps to audio gain.",
				.value = s_volume_curve_names[static_cast<size_t>(curve_index)],
				.dialog_title = ICON_FA_INFO_CIRCLE " Volume Curve",
				.choices = curve_choices,
				.on_choice = [this](int index) {
					emu.getConfig().audioDeviceConfig.volumeCurve = s_volume_curve_values[static_cast<size_t>(index)];
					emu.getConfig().save();
					emu.reloadSettings();
				},
			});
			return rows;
		},
	});

	pages.push_back({
		.id = "folders",
		.scope = fsui::SettingsScope::Global,
		.built_in_page = fsui::BuiltInSettingsPage::Folders,
		.build_rows = [this]() {
			return std::vector<fsui::SettingsRowDescriptor>{
				{.kind = fsui::SettingsRowKind::Heading, .title = "Game List"},
				{.kind = fsui::SettingsRowKind::Value, .title = ICON_FA_FOLDER_OPEN " Covers Directory", .summary = "Folder used to store external cover art for the game list.", .value = defaultCoverDirectory().string(), .enabled = false},
				{.kind = fsui::SettingsRowKind::Action, .title = ICON_FA_FOLDER_OPEN " Open Game List Settings", .summary = "Configure search directories, sort order, and covers.", .on_activate = []() { fsui::ShowGameListSettingsWindow(); }},
			};
		},
	});

	pages.erase(
		std::remove_if(pages.begin(), pages.end(), [scope](const fsui::SettingsPageDescriptor& page) { return page.scope != scope; }),
		pages.end()
	);
	return pages;
}

bool PandaFsuiAdapter::initialize(const fsui::FontStack& fonts)
{
	syncUiStateFromConfig();
	cachedGameList = buildGameList();
	closeSelectorRequested = false;
	pendingClassicUiRequest = ClassicUiRequest::None;
	pendingLaunchPath.reset();

	fsuiContext = {};
	fsuiContext.window = window;
	fsuiContext.fonts = fonts;
	fsuiContext.app_title = "Alber";
	fsuiContext.app_icon_path = resolveFsuiAppIconPath(emu.getConfig().frontendSettings.icon);
	fsuiContext.renderer_backend = fsui::RendererBackend::OpenGL;
	fsuiContext.appearance_settings.expose_theme_selector = true;
	fsuiContext.appearance_settings.expose_prompt_icon_selector = true;
	fsuiContext.appearance_settings.expose_background_image_selector = true;
	fsuiContext.host.ui_state = &uiState;
	fsuiContext.host.has_running_game = [this]() { return emu.romType != ROMType::None; };
	fsuiContext.host.get_current_game = [this]() { return buildCurrentGameInfo(); };
	fsuiContext.host.get_game_list = [this]() { return cachedGameList; };
	fsuiContext.host.refresh_game_list = [this](bool) { cachedGameList = buildGameList(); };
	fsuiContext.host.persist_ui_state = [this](bool reload) { persistUiState(reload); };
	fsuiContext.host.set_paused = [this](bool paused) {
		if (onPauseChange) {
			onPauseChange(paused);
		}
	};
	fsuiContext.host.resume_game = [this]() {
		if (onPauseChange) {
			onPauseChange(false);
		}
	};
	fsuiContext.host.detect_prompt_icon_pack = []() { return fsui::DetectPromptIconPackFromSDL(); };
	fsuiContext.host.detect_swap_north_west_gamepad_buttons = []() { return false; };
	fsuiContext.host.open_file_browser = [this](const std::filesystem::path& path) { openPathInBrowser(path); };
	fsuiContext.host.open_url = [](std::string_view url) { openUrl(url); };
	fsuiContext.host.runtime_overlay_options = fsui::RuntimeOverlayOptions{
		.show_inputs = true,
		.show_settings = true,
		.show_performance = true,
	};
	fsuiContext.host.get_input_overlay_devices = [this]() { return buildInputOverlayDevices(); };
	fsuiContext.host.get_settings_overlay_lines = [this]() { return buildSettingsOverlayLines(); };
	fsuiContext.host.get_performance_overlay_lines = [this]() { return buildPerformanceOverlayLines(); };
	fsuiContext.host.request_classic_ui = [this]() { requestClassicUi(true); };
	fsuiContext.host.launch_path = [this](const std::filesystem::path& path) { requestLaunchPath(path); };
	fsuiContext.host.close_selector = [this]() { closeSelectorRequested = true; };
	fsuiContext.host.get_landing_items = [this]() { return buildLandingItems(); };
	fsuiContext.host.get_start_items = [this]() { return buildStartItems(); };
	fsuiContext.host.get_exit_items = [this]() { return buildExitItems(); };
	fsuiContext.host.get_pause_items = [this]() { return buildPauseItems(); };
	fsuiContext.host.get_game_launch_options = [this](const fsui::GameEntry& entry) { return buildGameLaunchOptions(entry); };
	fsuiContext.host.get_settings_pages = [this](fsui::SettingsScope scope) { return buildSettingsPages(scope); };
	return fsui::Initialize(fsuiContext);
}

void PandaFsuiAdapter::shutdown(bool clear_state)
{
	fsui::Shutdown(clear_state);
}

void PandaFsuiAdapter::render()
{
	syncUiStateFromConfig();
	fsui::Render();
}

void PandaFsuiAdapter::consumeCommands()
{
	for (const fsui::Command& command : fsui::ConsumeCommands()) {
		switch (command.type) {
			case fsui::CommandType::LaunchPath:
				if (!command.path.empty()) {
					pendingLaunchPath = command.path;
					closeSelectorRequested = true;
				}
				break;

			case fsui::CommandType::CloseSelector:
				closeSelectorRequested = true;
				break;

			case fsui::CommandType::Resume:
				if (onPauseChange) {
					onPauseChange(false);
				}
				break;

			case fsui::CommandType::Reset:
				emu.reset(Emulator::ReloadOption::Reload);
				if (onPauseChange) {
					onPauseChange(false);
				}
				break;

			case fsui::CommandType::ExitToLibrary:
				if (onExitToSelector) {
					onExitToSelector();
				} else {
					SDL_Event quit {};
					quit.type = SDL_QUIT;
					SDL_PushEvent(&quit);
				}
				break;

			case fsui::CommandType::RequestQuit:
				{
					SDL_Event quit {};
					quit.type = SDL_QUIT;
					SDL_PushEvent(&quit);
				}
				break;

			case fsui::CommandType::ExitFullscreenUI:
				requestClassicUi(false);
				break;

			case fsui::CommandType::SwitchShell:
				if (command.id == "desktop" || command.id == "classic") {
					requestClassicUi(false);
				}
				break;

			case fsui::CommandType::Custom:
			default:
				break;
		}
	}
}

void PandaFsuiAdapter::setSelectorMode(bool selector_mode)
{
	if (selector_mode) {
		closeSelectorRequested = false;
	}
	fsui::SetSelectorMode(selector_mode);
}

bool PandaFsuiAdapter::isSelectorMode() const
{
	return fsui::IsSelectorMode();
}

bool PandaFsuiAdapter::hasActiveWindow() const
{
	return fsui::HasActiveWindow();
}

void PandaFsuiAdapter::openPauseMenu()
{
	fsui::OpenPauseMenu();
}

void PandaFsuiAdapter::returnToPreviousWindow()
{
	fsui::ReturnToPreviousWindow();
}

void PandaFsuiAdapter::returnToMainWindow()
{
	fsui::ReturnToMainWindow();
}

void PandaFsuiAdapter::switchToSettings()
{
	fsui::SwitchToSettings();
}

std::optional<std::filesystem::path> PandaFsuiAdapter::consumeLaunchPath()
{
	std::optional<std::filesystem::path> value = pendingLaunchPath;
	pendingLaunchPath.reset();
	return value;
}

bool PandaFsuiAdapter::consumeCloseSelector()
{
	const bool value = closeSelectorRequested;
	closeSelectorRequested = false;
	return value;
}

PandaFsuiAdapter::ClassicUiRequest PandaFsuiAdapter::consumeClassicUiRequest()
{
	const ClassicUiRequest value = pendingClassicUiRequest;
	pendingClassicUiRequest = ClassicUiRequest::None;
	return value;
}

#endif
