#pragma once
#ifndef __ROCK_TEXTURE_H__
#define __ROCK_TEXTURE_H__
#include <random>
#include <vector>

namespace rock_tex
{
	//[0, 1]
	struct color_t
	{
		float r;
		float g;
		float b;
	};

	class RockTexture
	{
	public:
		unsigned iterations_{ 2 };
		//for noise
		float perlin_freq_{ 0.08f };
		float cell_freq_{ 0.1f };
		unsigned octaves_{ 8 };
		float threshold_{ 0.2f };
		float falloff_{ 2.0f };
		//texture data
		color_t ** data_{ nullptr };
		//1st color is base color
		std::vector<color_t> palette;

		RockTexture();
		~RockTexture();
		RockTexture(const RockTexture& other) = delete;
		RockTexture(RockTexture&& other) noexcept = delete;
		RockTexture& operator=(const RockTexture& other) = delete;
		RockTexture& operator=(RockTexture&& other) noexcept = delete;
		void generate(unsigned size);

	private:
		std::random_device rd;
		std::mt19937 gen;
		unsigned size_{ 256 };

		void generateLayer(float ** Layer, unsigned sz);
		void generateBaseLayer();
		void generatePerlin(float ** Layer, unsigned sz);
		void generateCell(float ** Layer, unsigned sz);
	};

	template <typename T>
	void RockTexture_release(T ** data, unsigned sz)
	{
		if (data != nullptr)
		{
			for (unsigned int i = 0; i < sz; ++i)
			{
				delete[] data[i];
			}
			delete[] data;
		}
	}

	template <typename T>
	T ** RockTexture_alloc(unsigned sz)
	{
		T ** data = new T *[sz];
		for (unsigned i = 0; i < sz; ++i)
		{
			data[i] = new T[sz];
		}
		return data;
	}
}/*namespace rock_tex*/

#endif		/*__ROCK_TEXTURE_H__*/
