#pragma once
#include <Urho3D/Scene/Node.h>

namespace Urho3D
{
	void CreateAsteroidBlob(Context* ctx, Node * node, unsigned textureSize, unsigned subdivision, const Vector<String> &diffusePaths);
}		/*namespace Urho3D*/

