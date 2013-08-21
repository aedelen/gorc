#pragma once

#include "model_face.h"
#include <memory>
#include <vector>

namespace Gorc {
namespace Content {
namespace Assets {

class ModelMesh {
public:
	unsigned int Index;
	std::string Name;
	float Radius;
	Flags::GeometryMode Geo;
	Flags::LightMode Light;
	Flags::TextureMode Tex;

	std::vector<Math::Vector<3>> Vertices;
	std::vector<Math::Vector<3>> VertexNormals;
	std::vector<Math::Vector<2>> TextureVertices;

	std::vector<ModelFace> Faces;

	std::vector<int> MeshIndexBuffer;
};

}
}
}
