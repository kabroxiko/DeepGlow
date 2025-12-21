#include "aquarium_controller.h"

// Static member definitions
const char AquariumControllerUsermod::_name[] PROGMEM = "AquariumController";
// const char AquariumControllerUsermod::_enabled[] PROGMEM = "enabled"; // removed

// Register the usermod
static AquariumControllerUsermod aquariumController;
REGISTER_USERMOD(aquariumController);
