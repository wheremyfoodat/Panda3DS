#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>

#include <cstddef>
#include <vector>

#include "services/nwm/nwm_types.hpp"
#include "services/nwm/nwm_uds.hpp"

using namespace NWM;

std::vector<u8> NetworkInfoTag::getBytes(const NetworkInfo& networkInfo) {
	applicationDataSize = networkInfo.applicationDataSize;
	header = BeaconTagHeader(u8(TagID::VendorSpecific), sizeof(NetworkInfoTag) - sizeof(BeaconTagHeader) + applicationDataSize);

	// The network info tag stores network info data starting from the OUI
	std::memcpy(networkInfoData.data(), &networkInfo.oui, networkInfoData.size());

	// Calculate the SHA1 hash of the tag + application data.
	// The hash is calculated assuming the tag's original hash value is 0
	sha.fill(0);
	std::vector<u8> ret(sizeof(NetworkInfoTag) + applicationDataSize);
	std::memcpy(&ret[0], this, sizeof(*this));
	std::memcpy(&ret[sizeof(NetworkInfoTag)], &networkInfo.applicationData[0], applicationDataSize);

	CryptoPP::SHA1 shaEngine;
	// Skip the tag header when hashing
	shaEngine.CalculateDigest(sha.data(), ret.data() + sizeof(BeaconTagHeader), ret.size() - sizeof(BeaconTagHeader));

	// Copy the SHA1 into the output buffer
	std::copy(sha.begin(), sha.end(), ret.begin() + offsetof(NetworkInfoTag, sha));
	return ret;
}
