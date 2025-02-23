#include "ntshengn_graphics_module.h"
#include "../Module/utils/ntshengn_module_defines.h"
#include "../Module/utils/ntshengn_dynamic_library.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"

void NtshEngn::GraphicsModule::init() {
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::update(float dt) {
	NTSHENGN_UNUSED(dt);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::destroy() {
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

NtshEngn::MeshID NtshEngn::GraphicsModule::load(const Mesh& mesh) {
	NTSHENGN_UNUSED(mesh);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_MESH_UNKNOWN;
}

NtshEngn::ImageID NtshEngn::GraphicsModule::load(const Image& image) {
	NTSHENGN_UNUSED(image);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_IMAGE_UNKNOWN;
}

NtshEngn::FontID NtshEngn::GraphicsModule::load(const Font& font) {
	NTSHENGN_UNUSED(font);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_FONT_UNKNOWN;
}

void NtshEngn::GraphicsModule::setBackgroundColor(const Math::vec4& backgroundColor) {
	NTSHENGN_UNUSED(backgroundColor);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::playAnimation(Entity entity, uint32_t animationIndex) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_UNUSED(animationIndex);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::pauseAnimation(Entity entity) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::stopAnimation(Entity entity) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::setAnimationCurrentTime(Entity entity, float time) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_UNUSED(time);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

bool NtshEngn::GraphicsModule::isAnimationPlaying(Entity entity, uint32_t animationIndex) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_UNUSED(animationIndex);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return false;
}

void NtshEngn::GraphicsModule::emitParticles(const ParticleEmitter& particleEmitter) {
	NTSHENGN_UNUSED(particleEmitter);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::destroyParticles() {
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUIText(FontID fontID, const std::wstring& text, const Math::vec2& position, const Math::vec4& color) {
	NTSHENGN_UNUSED(fontID);
	NTSHENGN_UNUSED(text);
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color) {
	NTSHENGN_UNUSED(start);
	NTSHENGN_UNUSED(end);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color) {
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(size);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color) {
	NTSHENGN_UNUSED(imageID);
	NTSHENGN_UNUSED(imageSamplerFilter);
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(rotation);
	NTSHENGN_UNUSED(scale);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

extern "C" NTSHENGN_MODULE_API NtshEngn::GraphicsModuleInterface* createModule() {
	return new NtshEngn::GraphicsModule;
}

extern "C" NTSHENGN_MODULE_API void destroyModule(NtshEngn::GraphicsModuleInterface* m) {
	delete m;
}