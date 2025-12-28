#include "aquarium_controller.h"

// Define the global variable
uint16_t aquariumMinTransitionTime = DEFAULT_MIN_TRANSITION;
uint8_t  aquariumMaxBrightness = DEFAULT_MAX_BRIGHTNESS;

// Static member definitions
const char AquariumControllerUsermod::_name[] PROGMEM = "AquariumController";
// const char AquariumControllerUsermod::_enabled[] PROGMEM = "enabled"; // removed

// Register the usermod
static AquariumControllerUsermod aquariumController;
REGISTER_USERMOD(aquariumController);
