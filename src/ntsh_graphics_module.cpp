#include "ntsh_graphics_module.h"
#include "../external/Module/ntsh_module_defines.h"
#include "../external/Module/ntsh_dynamic_library.h"
#include "../external/Common/ntsh_engine_enums.h"

void NutshellGraphicsModule::init() {
    NTSH_MODULE_ERROR("init() function not implemented.", NTSH_RESULT_MODULE_FUNCTION_NOT_IMPLEMENTED);
}

void NutshellGraphicsModule::update(double dt) {
    NTSH_UNUSED(dt);
    NTSH_MODULE_ERROR("update() function not implemented.", NTSH_RESULT_MODULE_FUNCTION_NOT_IMPLEMENTED);
}

void NutshellGraphicsModule::destroy() {
    NTSH_MODULE_ERROR("destroy() function not implemented.", NTSH_RESULT_MODULE_FUNCTION_NOT_IMPLEMENTED);
}

extern "C" NTSH_MODULE_API NutshellGraphicsModuleInterface * createModule() {
    return new NutshellGraphicsModule;
}

extern "C" NTSH_MODULE_API void destroyModule(NutshellGraphicsModuleInterface * m) {
    delete m;
}