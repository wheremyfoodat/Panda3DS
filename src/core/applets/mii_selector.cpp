#include "applets/mii_selector.hpp"

#include <boost/crc.hpp>
#include <limits>

#include "kernel/handles.hpp"

using namespace Applets;

void MiiSelectorApplet::reset() {}
Result::HorizonResult MiiSelectorApplet::start(const MemoryBlock* sharedMem, const std::vector<u8>& parameters, u32 appID) {
	// Get mii configuration from the application
	std::memcpy(&config, &parameters[0], sizeof(config));

	Applets::Parameter param = Applets::Parameter{
		.senderID = appID,
		.destID = AppletIDs::Application,
		.signal = static_cast<u32>(APTSignal::WakeupByExit),
		.object = 0,
	};

	// Thanks to Citra devs as always for the default mii data and other applet help
	output = getDefaultMii();
	output.returnCode = 0;  // Success
	// output.selectedMiiData = miiData;
	output.selectedGuestMiiIndex = std::numeric_limits<u32>::max();
	output.miiChecksum = boost::crc<16, 0x1021, 0, 0, false, false>(&output.selectedMiiData, sizeof(MiiData) + sizeof(output.unknown1));

	// Copy output into the response parameter
	param.data.resize(sizeof(output));
	std::memcpy(&param.data[0], &output, sizeof(output));

	nextParameter = param;
	return Result::Success;
}

Result::HorizonResult MiiSelectorApplet::receiveParameter(const Applets::Parameter& parameter) {
	Applets::Parameter param = Applets::Parameter{
		.senderID = parameter.destID,
		.destID = AppletIDs::Application,
		.signal = static_cast<u32>(APTSignal::Response),
		.object = KernelHandles::APTCaptureSharedMemHandle,
		.data = {},
	};

	nextParameter = param;
	return Result::Success;
}

MiiResult MiiSelectorApplet::getDefaultMii() {
	// This data was obtained from Citra
	MiiData miiData;
	miiData.version = 0x03;
	miiData.miiOptions = 0x00;
	miiData.miiPos = 0x10;
	miiData.consoleID = 0x30;
	miiData.systemID = 0xD285B6B300C8850A;
	miiData.miiID = 0x98391EE4;
	miiData.creatorMAC = {0x40, 0xF4, 0x07, 0xB7, 0x37, 0x10};
	miiData.padding = 0x0000;
	miiData.miiDetails = 0xA600;
	miiData.miiName = {'P', 'a', 'n', 'd', 'a', '3', 'D', 'S', 0x0, 0x0};
	miiData.height = 0x40;
	miiData.width = 0x40;
	miiData.faceStyle = 0x00;
	miiData.faceDetails = 0x00;
	miiData.hairStyle = 0x21;
	miiData.hairDetails = 0x01;
	miiData.eyeDetails = 0x02684418;
	miiData.eyebrowDetails = 0x26344614;
	miiData.noseDetails = 0x8112;
	miiData.mouthDetails = 0x1768;
	miiData.moustacheDetails = 0x0D00;
	miiData.beardDetails = 0x0029;
	miiData.glassesDetails = 0x0052;
	miiData.moleDetails = 0x4850;
	miiData.authorName = {u'B', u'O', u'N', u'K', u'E', u'R'};

	MiiResult result;
	result.returnCode = 0x0;
	result.isGuestMiiSelected = 0x0;
	result.selectedGuestMiiIndex = std::numeric_limits<u32>::max();
	result.selectedMiiData = miiData;
	result.guestMiiName.fill(0x0);

	return result;
}