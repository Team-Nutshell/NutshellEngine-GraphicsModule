#pragma once
#include "../external/Common/module_interfaces/ntshengn_graphics_module_interface.h"

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Graphics Test Module") {}

		void init();
		void update(double dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		NtshEngn::MeshId load(const NtshEngn::Mesh mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		NtshEngn::ImageId load(const NtshEngn::Image image);
	};

}