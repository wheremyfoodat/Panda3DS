#include "services/service_manager.hpp"

#include <map>

#include "ipc.hpp"
#include "kernel.hpp"

ServiceManager::ServiceManager(
	std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel, const EmulatorConfig& config, LuaManager& lua
)
	: regs(regs), mem(mem), kernel(kernel), lua(lua), ac(mem), am(mem), boss(mem), act(mem), apt(mem, kernel), cam(mem, kernel), cecd(mem, kernel),
	  cfg(mem, config), csnd(mem, kernel), dlp_srvr(mem), dsp(mem, kernel, config), hid(mem, kernel), http(mem), ir_user(mem, hid, config, kernel),
	  frd(mem), fs(mem, kernel, config), gsp_gpu(mem, gpu, kernel, currentPID), gsp_lcd(mem), ldr(mem, kernel), mcu_hwc(mem, config),
	  mic(mem, kernel), nfc(mem, kernel), nim(mem), ndm(mem), news_u(mem), ns(mem), nwm_uds(mem, kernel), ptm(mem, config), soc(mem), ssl(mem),
	  y2r(mem, kernel) {}

static constexpr int MAX_NOTIFICATION_COUNT = 16;

// Reset every single service
void ServiceManager::reset() {
	ac.reset();
	act.reset();
	am.reset();
	apt.reset();
	boss.reset();
	cam.reset();
	cecd.reset();
	cfg.reset();
	csnd.reset();
	dlp_srvr.reset();
	dsp.reset();
	hid.reset();
	http.reset();
	ir_user.reset();
	frd.reset();
	fs.reset();
	gsp_gpu.reset();
	gsp_lcd.reset();
	ldr.reset();
	mcu_hwc.reset();
	mic.reset();
	ndm.reset();
	news_u.reset();
	nfc.reset();
	nim.reset();
	ns.reset();
	ptm.reset();
	soc.reset();
	ssl.reset();
	y2r.reset();

	notificationSemaphore = std::nullopt;
}

// Match IPC messages to a "srv:" command based on their header
namespace Commands {
	enum : u32 {
		RegisterClient = 0x00010002,
		EnableNotification = 0x00020000,
		RegisterService = 0x00030100,
		UnregisterService = 0x000400C0,
		GetServiceHandle = 0x00050100,
		RegisterPort = 0x000600C2,
		UnregisterPort = 0x000700C0,
		GetPort = 0x00080100,
		Subscribe = 0x00090040,
		Unsubscribe = 0x000A0040,
		ReceiveNotification = 0x000B0000,
		PublishToSubscriber = 0x000C0080,
		PublishAndGetSubscriber = 0x000D0040,
		IsServiceRegistered = 0x000E00C0
	};
}

// Handle an IPC message issued using the SendSyncRequest SVC
// The parameters are stored in thread-local storage in this format: https://www.3dbrew.org/wiki/IPC#Message_Structure
// messagePointer: The base pointer for the IPC message
void ServiceManager::handleSyncRequest(u32 messagePointer) {
	const u32 header = mem.read32(messagePointer);

	switch (header) {
		case Commands::EnableNotification: enableNotification(messagePointer); break;
		case Commands::ReceiveNotification: receiveNotification(messagePointer); break;
		case Commands::RegisterClient: registerClient(messagePointer); break;
		case Commands::GetServiceHandle: getServiceHandle(messagePointer); break;
		case Commands::Subscribe: subscribe(messagePointer); break;
		case Commands::Unsubscribe: unsubscribe(messagePointer); break;
		case Commands::PublishToSubscriber: publishToSubscriber(messagePointer); break;
		default: Helpers::panic("Unknown \"srv:\" command: %08X", header); break;
	}
}

// https://www.3dbrew.org/wiki/SRV:RegisterClient
void ServiceManager::registerClient(u32 messagePointer) {
	log("srv::registerClient (Stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// clang-format off
static std::map<std::string, HorizonHandle> serviceMap = {
	{ "ac:u", KernelHandles::AC },
	{ "ac:i", KernelHandles::AC },
	{ "act:a", KernelHandles::ACT },
	{ "act:u", KernelHandles::ACT },
	{ "am:app", KernelHandles::AM },
	{ "am:sys", KernelHandles::AM },
	{ "APT:S", KernelHandles::APT }, // TODO: APT:A, APT:S and APT:U are slightly different
	{ "APT:A", KernelHandles::APT },
	{ "APT:U", KernelHandles::APT },
	{ "boss:U", KernelHandles::BOSS },
	{ "boss:P", KernelHandles::BOSS },
	{ "cam:u", KernelHandles::CAM },
	{ "cecd:u", KernelHandles::CECD },
	{ "cfg:u", KernelHandles::CFG_U },
	{ "cfg:i", KernelHandles::CFG_I },
	{ "cfg:s", KernelHandles::CFG_S },
	{ "cfg:nor", KernelHandles::CFG_NOR },
	{ "csnd:SND", KernelHandles::CSND },
	{ "dlp:SRVR", KernelHandles::DLP_SRVR },
	{ "dsp::DSP", KernelHandles::DSP },
	{ "hid:USER", KernelHandles::HID },
	{ "hid:SPVR", KernelHandles::HID },
	{ "http:C", KernelHandles::HTTP },
	{ "ir:USER", KernelHandles::IR_USER },
	{ "frd:a", KernelHandles::FRD_A },
	{ "frd:u", KernelHandles::FRD_U },
	{ "fs:USER", KernelHandles::FS },
	{ "gsp::Gpu", KernelHandles::GPU },
	{ "gsp::Lcd", KernelHandles::LCD },
	{ "ldr:ro", KernelHandles::LDR_RO },
	{ "mcu::HWC", KernelHandles::MCU_HWC },
	{ "mic:u", KernelHandles::MIC },
	{ "ndm:u", KernelHandles::NDM },
	{ "news:u", KernelHandles::NEWS_U },
	{ "nfc:u", KernelHandles::NFC },
	{ "ns:s", KernelHandles::NS_S },
	{ "nwm::EXT", KernelHandles::NWM_EXT },
	{ "nwm::UDS", KernelHandles::NWM_UDS },
	{ "nim:aoc", KernelHandles::NIM },
	{ "ptm:u", KernelHandles::PTM_U }, // TODO: ptm:u and ptm:sysm have very different command sets
	{ "ptm:sysm", KernelHandles::PTM_SYSM },
	{ "ptm:play", KernelHandles::PTM_PLAY },
	{ "ptm:gets", KernelHandles::PTM_GETS },
	{ "soc:U", KernelHandles::SOC },
	{ "ssl:C", KernelHandles::SSL },
	{ "y2r:u", KernelHandles::Y2R },
};
// clang-format on

// https://www.3dbrew.org/wiki/SRV:GetServiceHandle
void ServiceManager::getServiceHandle(u32 messagePointer) {
	u32 nameLength = mem.read32(messagePointer + 12);
	u32 flags = mem.read32(messagePointer + 16);
	u32 handle = 0;

	std::string service = mem.readString(messagePointer + 4, 8);
	log("srv::getServiceHandle (Service: %s, nameLength: %d, flags: %d)\n", service.c_str(), nameLength, flags);

	// Look up service handle in map, panic if it does not exist
	if (auto search = serviceMap.find(service); search != serviceMap.end())
		handle = search->second;
	else
		Helpers::panic("srv: GetServiceHandle with unknown service %s", service.c_str());

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 12, handle);
}

void ServiceManager::enableNotification(u32 messagePointer) {
	log("srv::EnableNotification()\n");

	// Make a semaphore for notifications if none exists currently
	if (!notificationSemaphore.has_value() || kernel.getObject(notificationSemaphore.value(), KernelObjectType::Semaphore) == nullptr) {
		notificationSemaphore = kernel.makeSemaphore(0, MAX_NOTIFICATION_COUNT);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);  // Result code
	mem.write32(messagePointer + 8, 0);                // Translation descriptor
	// Handle to semaphore signaled on process notification
	mem.write32(messagePointer + 12, notificationSemaphore.value());
}

void ServiceManager::receiveNotification(u32 messagePointer) {
	log("srv::ReceiveNotification() (STUBBED)\n");

	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);  // Result code
	mem.write32(messagePointer + 8, 0);                // Notification ID
}

void ServiceManager::subscribe(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	log("srv::Subscribe (id = %d) (stubbed)\n", id);

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::unsubscribe(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	log("srv::Unsubscribe (id = %d) (stubbed)\n", id);

	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::publishToSubscriber(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	log("srv::PublishToSubscriber (Notification ID = %d) (stubbed)\n", id);

	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::sendCommandToService(u32 messagePointer, Handle handle) {
	if (haveServiceIntercepts) [[unlikely]] {
		if (checkForIntercept(messagePointer, handle)) [[unlikely]] {
			return;
		}
	}

	switch (handle) {
		// Breaking alphabetical order a bit to place the ones I think are most common at the top
		case KernelHandles::GPU: [[likely]] gsp_gpu.handleSyncRequest(messagePointer); break;
		case KernelHandles::FS: [[likely]] fs.handleSyncRequest(messagePointer); break;
		case KernelHandles::APT: [[likely]] apt.handleSyncRequest(messagePointer); break;
		case KernelHandles::DSP: [[likely]] dsp.handleSyncRequest(messagePointer); break;

		case KernelHandles::AC: ac.handleSyncRequest(messagePointer); break;
		case KernelHandles::ACT: act.handleSyncRequest(messagePointer); break;
		case KernelHandles::AM: am.handleSyncRequest(messagePointer); break;
		case KernelHandles::BOSS: boss.handleSyncRequest(messagePointer); break;
		case KernelHandles::CAM: cam.handleSyncRequest(messagePointer); break;
		case KernelHandles::CECD: cecd.handleSyncRequest(messagePointer); break;
		case KernelHandles::CFG_U: cfg.handleSyncRequest(messagePointer, CFGService::Type::U); break;
		case KernelHandles::CFG_I: cfg.handleSyncRequest(messagePointer, CFGService::Type::I); break;
		case KernelHandles::CFG_S: cfg.handleSyncRequest(messagePointer, CFGService::Type::S); break;
		case KernelHandles::CFG_NOR: cfg.handleSyncRequest(messagePointer, CFGService::Type::NOR); break;
		case KernelHandles::CSND: csnd.handleSyncRequest(messagePointer); break;
		case KernelHandles::DLP_SRVR: dlp_srvr.handleSyncRequest(messagePointer); break;
		case KernelHandles::HID: hid.handleSyncRequest(messagePointer); break;
		case KernelHandles::HTTP: http.handleSyncRequest(messagePointer); break;
		case KernelHandles::IR_USER: ir_user.handleSyncRequest(messagePointer); break;
		case KernelHandles::FRD_A: frd.handleSyncRequest(messagePointer, FRDService::Type::A); break;
		case KernelHandles::FRD_U: frd.handleSyncRequest(messagePointer, FRDService::Type::U); break;
		case KernelHandles::LCD: gsp_lcd.handleSyncRequest(messagePointer); break;
		case KernelHandles::LDR_RO: ldr.handleSyncRequest(messagePointer); break;
		case KernelHandles::MCU_HWC: mcu_hwc.handleSyncRequest(messagePointer); break;
		case KernelHandles::MIC: mic.handleSyncRequest(messagePointer); break;
		case KernelHandles::NFC: nfc.handleSyncRequest(messagePointer); break;
		case KernelHandles::NIM: nim.handleSyncRequest(messagePointer); break;
		case KernelHandles::NDM: ndm.handleSyncRequest(messagePointer); break;
		case KernelHandles::NEWS_U: news_u.handleSyncRequest(messagePointer); break;
		case KernelHandles::NS_S: ns.handleSyncRequest(messagePointer, NSService::Type::S); break;
		case KernelHandles::NWM_UDS: nwm_uds.handleSyncRequest(messagePointer); break;
		case KernelHandles::PTM_PLAY: ptm.handleSyncRequest(messagePointer, PTMService::Type::PLAY); break;
		case KernelHandles::PTM_SYSM: ptm.handleSyncRequest(messagePointer, PTMService::Type::SYSM); break;
		case KernelHandles::PTM_U: ptm.handleSyncRequest(messagePointer, PTMService::Type::U); break;
		case KernelHandles::PTM_GETS: ptm.handleSyncRequest(messagePointer, PTMService::Type::GETS); break;
		case KernelHandles::SOC: soc.handleSyncRequest(messagePointer); break;
		case KernelHandles::SSL: ssl.handleSyncRequest(messagePointer); break;
		case KernelHandles::Y2R: y2r.handleSyncRequest(messagePointer); break;
		default: Helpers::panic("Sent IPC message to unknown service %08X\n Command: %08X", handle, mem.read32(messagePointer));
	}
}

bool ServiceManager::checkForIntercept(u32 messagePointer, Handle handle) {
	// Check if there's a Lua handler for this function and call it
	const u32 function = mem.read32(messagePointer);

	for (const auto& [serviceName, serviceHandle] : serviceMap) {
		if (serviceHandle == handle) {
			auto intercept = InterceptedService(std::string(serviceName), function);
			if (auto intercept_it = interceptedServices.find(intercept); intercept_it != interceptedServices.end()) {
				// If the Lua handler returns true, it means the service is handled entirely
				// From Lua, and we shouldn't do anything else here.
				return lua.signalInterceptedService(intercept_it->second, intercept.serviceName, function, messagePointer);
			}

			break;
		}
	}

	// Lua did not intercept the service, so emulate it normally
	return false;
}