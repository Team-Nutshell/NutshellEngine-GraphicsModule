#include "ntsh_graphics_module.h"
#include "../external/Module/utils/ntsh_module_defines.h"
#include "../external/Module/utils/ntsh_dynamic_library.h"
#include "../external/Common/utils/ntsh_engine_defines.h"
#include "../external/Common/utils/ntsh_engine_enums.h"

void NutshellGraphicsModule::init() {
	NTSH_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NutshellGraphicsModule::update(double dt) {
	NTSH_UNUSED(dt);
	NTSH_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NutshellGraphicsModule::destroy() {
	NTSH_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

NtshMeshId NutshellGraphicsModule::load(const NtshMesh mesh) {
	NTSH_MODULE_FUNCTION_NOT_IMPLEMENTED();
	return 0;
}

NtshImageId NutshellGraphicsModule::load(const NtshImage mesh) {
	NTSH_MODULE_FUNCTION_NOT_IMPLEMENTED();
	return 0;
}

extern "C" NTSH_MODULE_API NutshellGraphicsModuleInterface* createModule() {
	return new NutshellGraphicsModule;
}

extern "C" NTSH_MODULE_API void destroyModule(NutshellGraphicsModuleInterface* m) {
	delete m;
}