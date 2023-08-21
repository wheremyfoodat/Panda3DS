#include "services/service_manager.hpp"

#include <map>

#include "ipc.hpp"
#include "kernel.hpp"

ServiceManager::ServiceManager(std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel, const EmulatorConfig& config)
	: regs(regs), mem(mem), kernel(kernel), ac(mem), am(mem), boss(mem), act(mem), apt(mem, kernel), cam(mem, kernel), cecd(mem, kernel), cfg(mem),
	  dlp_srvr(mem), dsp(mem, kernel), hid(mem, kernel), http(mem), ir_user(mem, kernel), frd(mem), fs(mem, kernel),
	  gsp_gpu(mem, gpu, kernel, currentPID), gsp_lcd(mem), ldr(mem), mcu_hwc(mem, config), mic(mem), nfc(mem, kernel), nim(mem), ndm(mem),
	  news_u(mem), ptm(mem, config), soc(mem), ssl(mem), y2r(mem, kernel) {}

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
		default: Helpers::panic("Unknown \"srv:\" command: %08X", header);
	}
}

// https://www.3dbrew.org/wiki/SRV:RegisterClient
void ServiceManager::registerClient(u32 messagePointer) {
	log("srv::registerClient (Stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// clang-format off
static std::map<std::string, Handle> serviceMap = {
	{ "ac:u", KernelHandles::AC },
	{ "act:u", KernelHandles::ACT },
	{ "am:app", KernelHandles::AM },
	{ "APT:S", KernelHandles::APT }, // TODO: APT:A, APT:S and APT:U are slightly different
	{ "APT:A", KernelHandles::APT },
	{ "APT:U", KernelHandles::APT },
	{ "boss:U", KernelHandles::BOSS },
	{ "cam:u", KernelHandles::CAM },
	{ "cecd:u", KernelHandles::CECD },
	{ "cfg:u", KernelHandles::CFG_U },
	{ "cfg:i", KernelHandles::CFG_I },
	{ "dlp:SRVR", KernelHandles::DLP_SRVR },
	{ "dsp::DSP", KernelHandles::DSP },
	{ "hid:USER", KernelHandles::HID },
	{ "http:C", KernelHandles::HTTP },
	{ "ir:USER", KernelHandles::IR_USER },
	{ "frd:u", KernelHandles::FRD },
	{ "fs:USER", KernelHandles::FS },
	{ "gsp::Gpu", KernelHandles::GPU },
	{ "gsp::Lcd", KernelHandles::LCD },
	{ "ldr:ro", KernelHandles::LDR_RO },
	{ "mcu::HWC", KernelHandles::MCU_HWC },
	{ "mic:u", KernelHandles::MIC },
	{ "ndm:u", KernelHandles::NDM },
	{ "news:u", KernelHandles::NEWS_U },
	{ "nfc:u", KernelHandles::NFC },
	{ "nim:aoc", KernelHandles::NIM },
	{ "ptm:u", KernelHandles::PTM }, // TODO: ptm:u and ptm:sysm have very different command sets
	{ "ptm:sysm", KernelHandles::PTM },
	{ "soc:U", KernelHandles::SOC },
	{ "ssl:C", KernelHandles::SSL },
	{ "y2r:u", KernelHandles::Y2R }
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
	mem.write32(messagePointer + 4, Result::Success); // Result code
	mem.write32(messagePointer + 8, 0); // Translation descriptor
	// Handle to semaphore signaled on process notification
	mem.write32(messagePointer + 12, notificationSemaphore.value());
}

void ServiceManager::receiveNotification(u32 messagePointer) {
	log("srv::ReceiveNotification() (STUBBED)\n");

	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success); // Result code
	mem.write32(messagePointer + 8, 0); // Notification ID
}

void ServiceManager::subscribe(u32 messagePointer) {
	u32 id = mem.read32(messagePointer + 4);
	log("srv::Subscribe (id = %d) (stubbed)\n", id);

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void ServiceManager::sendCommandToService(u32 messagePointer, Handle handle) {
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
		case KernelHandles::CFG_U: cfg.handleSyncRequest(messagePointer); break;
		case KernelHandles::DLP_SRVR: dlp_srvr.handleSyncRequest(messagePointer); break;
		case KernelHandles::HID: hid.handleSyncRequest(messagePointer); break;
		case KernelHandles::HTTP: http.handleSyncRequest(messagePointer); break;
		case KernelHandles::IR_USER: ir_user.handleSyncRequest(messagePointer); break;
		case KernelHandles::FRD: frd.handleSyncRequest(messagePointer); break;
		case KernelHandles::LCD: gsp_lcd.handleSyncRequest(messagePointer); break;
		case KernelHandles::LDR_RO: ldr.handleSyncRequest(messagePointer); break;
		case KernelHandles::MCU_HWC: mcu_hwc.handleSyncRequest(messagePointer); break;
		case KernelHandles::MIC: mic.handleSyncRequest(messagePointer); break;
		case KernelHandles::NFC: nfc.handleSyncRequest(messagePointer); break;
		case KernelHandles::NIM: nim.handleSyncRequest(messagePointer); break;
		case KernelHandles::NDM: ndm.handleSyncRequest(messagePointer); break;
		case KernelHandles::NEWS_U: news_u.handleSyncRequest(messagePointer); break;
		case KernelHandles::PTM: ptm.handleSyncRequest(messagePointer); break;
		case KernelHandles::SOC: soc.handleSyncRequest(messagePointer); break;
		case KernelHandles::SSL: ssl.handleSyncRequest(messagePointer); break;
		case KernelHandles::Y2R: y2r.handleSyncRequest(messagePointer); break;
		default: Helpers::panic("Sent IPC message to unknown service %08X\n Command: %08X", handle, mem.read32(messagePointer));
	}
}