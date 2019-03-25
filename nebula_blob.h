#pragma once
#include <Urho3D/Scene/Node.h>

namespace Urho3D
{
	template <typename T>
	void release2Darr(T ** data, unsigned width, unsigned height)
	{
		if (data != nullptr)
		{
			for (unsigned int i = 0; i < width; ++i)
			{
				delete[] data[i];
			}
			delete[] data;
		}
	}

	template <typename T>
	T ** alloc2Darr(unsigned width, unsigned height)
	{
		T ** data = new T *[width];
		for (unsigned i = 0; i < width; ++i)
		{
			data[i] = new T[height];
		}
		return data;
	}

	//normalize to [0, 1]
	template <typename T>
	void normalize2Darr(T ** data, unsigned width, unsigned height)
	{
		if (data != nullptr)
		{
			T max = data[0][0];
			T min = data[0][0];
			for (unsigned x = 0; x < width; ++x)
			{
				for (unsigned y = 0; y < height; ++y)
				{
					if (data[x][y] > max)
						max = data[x][y];
					else if (data[x][y] < min)
						min = data[x][y];
				}
			}

			for (unsigned x = 0; x < width; ++x)
			{
				for (unsigned y = 0; y < height; ++y)
				{
					data[x][y] = (data[x][y] - min) / (max - min);
				}
			}
		}
	}

	void CreateNebulaBlob(Context* ctx, Node * node, const PODVector<Color> &colors, unsigned int TextureSize);
}
