#pragma once
#include "../external/Common/module_interfaces/ntsh_graphics_module_interface.h"

class NutshellGraphicsModule : public NutshellGraphicsModuleInterface {
public:
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Test Module") {}

	void init();
	void update(double dt);
	void destroy();
};