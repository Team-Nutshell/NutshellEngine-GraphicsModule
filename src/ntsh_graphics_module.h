#pragma once
#include "../external/Common/module_interfaces/ntsh_graphics_module_interface.h"

class NutshellGraphicsModule : public NutshellGraphicsModuleInterface {
public:
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Test Module") {}

	void init();
	void update(double dt);
	void destroy();

	// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
	NtshMeshId load(const NtshMesh mesh);
	// Loads the image described in the image parameter in the internal format and returns a unique identifier
	NtshImageId load(const NtshImage image);
};