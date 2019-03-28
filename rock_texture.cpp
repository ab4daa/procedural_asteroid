#include "rock_texture.h"
#include "FastNoise.h"
#include <cmath>

namespace rock_tex
{
	static color_t mix_color(const color_t &a, const color_t &b, float alpha)
	{
		color_t ret;
		ret.r = a.r * alpha + b.r*(1.0f - alpha);
		ret.g = a.g * alpha + b.g*(1.0f - alpha);
		ret.b = a.b * alpha + b.b*(1.0f - alpha);
		return ret;
	}

	RockTexture::RockTexture()
		: gen(rd()) {}

	RockTexture::~RockTexture()
	{
		RockTexture_release<color_t>(data_, size_);
	}

	void RockTexture::generatePerlin(float ** Layer, unsigned sz)
	{
		std::uniform_int_distribution<unsigned int> dis(0, 0xffffffff);
		FastNoise perlin(dis(gen));
		perlin.SetFractalOctaves(octaves_);
		perlin.SetFrequency(perlin_freq_);

		float max = perlin.GetPerlinFractal(0, 0);
		float min = max;
		for (unsigned x = 0; x < sz; ++x)
		{
			for (unsigned y = 0; y < sz; ++y)
			{
				Layer[x][y] = perlin.GetPerlinFractal(x, y);
				if (Layer[x][y] > max)
					max = Layer[x][y];
				if (Layer[x][y] < min)
					min = Layer[x][y];
			}
		}

		for (unsigned x = 0; x < sz; ++x)
		{
			for (unsigned y = 0; y < sz; ++y)
			{
				Layer[x][y] = (Layer[x][y] - min) / (max - min);
			}
		}
	}

	void RockTexture::generateCell(float ** Layer, unsigned sz)
	{
		std::uniform_int_distribution<unsigned int> dis(0, 0xffffffff);
		FastNoise cell(dis(gen));
		cell.SetFrequency(cell_freq_);
		cell.SetCellularDistanceFunction(FastNoise::Euclidean);
		cell.SetCellularReturnType(FastNoise::Distance2Sub);

		float max = cell.GetCellular(0, 0); 
		float min = max;
		for (unsigned x = 0; x < sz; ++x)
		{
			for (unsigned y = 0; y < sz; ++y)
			{
				Layer[x][y] = cell.GetCellular(x, y);
				if (Layer[x][y] > max)
					max = Layer[x][y];
				if (Layer[x][y] < min)
					min = Layer[x][y];
			}
		}

		for (unsigned x = 0; x < sz; ++x)
		{
			for (unsigned y = 0; y < sz; ++y)
			{
				Layer[x][y] = (Layer[x][y] - min) / (max - min);
			}
		}
	}

	void RockTexture::generateBaseLayer()
	{
		float ** tmp = RockTexture_alloc<float>(size_);
		generatePerlin(tmp, size_);

		for (unsigned x = 0; x < size_; ++x)
		{
			for (unsigned y = 0; y < size_; ++y)
			{
				data_[x][y] = mix_color(palette[0], palette[1], tmp[x][y]);
			}
		}

		RockTexture_release<float>(tmp, size_);
	}

	void RockTexture::generateLayer(float ** Layer, unsigned sz)
	{
		float ** tmp = RockTexture_alloc<float>(size_);
		generateCell(tmp, size_);

		for (unsigned x = 0; x < sz; ++x)
		{
			for (unsigned y = 0; y < sz; ++y)
			{
				Layer[x][y] = pow(tmp[x][y], falloff_);
			}
		}
		RockTexture_release<float>(tmp, size_);
	}

	void RockTexture::generate(unsigned size)
	{
		RockTexture_release<color_t>(data_, size_);
		size_ = size;
		data_ = RockTexture_alloc<color_t>(size_);
		generateBaseLayer();
		
		std::uniform_real_distribution<float> fdis(-1.0f, 1.0f);
		float ** layer = RockTexture_alloc<float>(size_);
		for (unsigned j = 0; j < iterations_; ++j)
		{
			for (unsigned i = 0; i < palette.size(); ++i)
			{
				generateLayer(layer, size_);
				for (unsigned x = 0; x < size_; ++x)
				{
					for (unsigned y = 0; y < size_; ++y)
					{
						if (layer[x][y] > threshold_)
						{
							float a = layer[x][y];
							data_[x][y] = mix_color(palette[i], data_[x][y], a);
						}
					}
				}
			}
		}
		RockTexture_release<float>(layer, size_);
	}
}		/*namespace rock_tex*/
