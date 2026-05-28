#pragma once
#include "../Common/modules/ntshengn_graphics_module_interface.h"

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Default Graphics Module") {}

		void init();
		void update(float dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		MeshID load(const Mesh& mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		ImageID load(const Image& image);
		// Loads the font described in the font parameter in the internal format and returns a unique identifier
		FontID load(const Font& font);

		// Sets the background color
		void setBackgroundColor(const Math::vec4& backgroundColor);

		// Plays an animation for an entity, indexed in the entity's model animation list, looping or not
		void playAnimation(Entity entity, uint32_t animationIndex, bool looping);
		// Resumes the animation played by an entity
		void resumeAnimation(Entity entity);
		// Pauses an animation played by an entity
		void pauseAnimation(Entity entity);
		// Stops an animation played by an entity
		void stopAnimation(Entity entity);

		// Returns the index of the playing animation
		uint32_t getPlayingAnimation(Entity entity);
		// Returns true if the entity is currently playing the animation with index animationIndex, else, returns false
		bool isAnimationPlaying(Entity entity, uint32_t animationIndex);

		// Sets the current playing time of an animation played by an entity
		void setAnimationCurrentTime(Entity entity, float newTime);
		// Returns the current playing time of an animation played by an entity
		float getAnimationCurrentTime(Entity entity);

		// Sets the speed of an animation played by an entity
		void setAnimationSpeed(Entity entity, float newSpeed);
		// Returns the speed of an animation played by an entity
		float getAnimationSpeed(Entity entity);

		// Emits particles described by particleEmitter
		void emitParticles(const ParticleEmitter& particleEmitter);
		// Destroys all particles
		void destroyParticles();

		// Draws a text on the UI with the font in the fontID parameter according to its position, rotation, scale and color
		void drawUIText(FontID fontID, const std::wstring& text, AnchorPoint anchorPoint, CoordinateType coordinateType, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color);
		// Draws a line on the UI according to its start and end points and its color
		void drawUILine(CoordinateType coordinateType, const Math::vec2& start, const Math::vec2& end, const Math::vec4& color);
		// Draws a rectangle on the UI according to its position, its size (width and height) and its color
		void drawUIRectangle(CoordinateType coordinateType, const Math::vec2& position, const Math::vec2& size, const Math::vec4& color);
		// Draws an image on the UI according to its sampler filter, position, rotation, scale and color to multiply the image with
		void drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, AnchorPoint anchorPoint, CoordinateType coordinateType, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color);
	};

}