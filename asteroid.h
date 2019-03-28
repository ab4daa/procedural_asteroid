#pragma once
#include <Urho3D/Scene/Node.h>

namespace Urho3D
{
	void CreateAsteroidBlob(Context* ctx, Node * node, unsigned textureSize, const PODVector<Color> &palette, unsigned subdivision);
}		/*namespace Urho3D*/

