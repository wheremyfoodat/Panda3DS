#include "services/amiibo_device.hpp"

void AmiiboDevice::reset() {
	encrypted = false;
	loaded = false;
}

// Load amiibo information from our raw 540 byte array
void AmiiboDevice::loadFromRaw() {

}