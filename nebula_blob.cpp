#include "nebula_blob.h"
#include "FastNoise.h"
#include <Urho3D/Urho3DAll.h>

namespace Urho3D
{
	typedef struct nebula_vertex_s_
	{
		Vector3 position;
		Vector3 normal;
		Vector2 uv;
	}nebula_vertex_data_;

	static Model * CreateNebulaModel(Context* ctx, unsigned numMaterial)
	{
		const unsigned numPlane = 5;
		const unsigned vertexPerPlane = 4;
		const unsigned indexPerPlane = 6;
		const Quaternion q[numPlane] =
		{
			Quaternion::IDENTITY,
			Quaternion(90.0f, Vector3::RIGHT),
			Quaternion(90.0f, Vector3::FORWARD),
			Quaternion(90.0f, Vector3(1.0f, 0.0f, -1.0f)),
			Quaternion(90.0f, Vector3(1.0f, 0.0f, 1.0f)),
		};
		const Vector3 vertices[vertexPerPlane] = {
			Vector3(-0.5f, 0.0f, 0.5f),
			Vector3(0.5f, 0.0f, 0.5f),
			Vector3(0.5f, 0.0f, -0.5f),
			Vector3(-0.5f, 0.0f, -0.5f)
		};
		const Vector3 normal(Vector3::UP);
		const Vector2 uv[vertexPerPlane] = {
			Vector2(0.0f, 0.0f),
			Vector2(1.0f, 0.0f),
			Vector2(1.0f, 1.0f),
			Vector2(0.0f, 1.0f)
		};
		const unsigned short indices[indexPerPlane] = {
			0, 1, 3,
			1, 2, 3
		};

		if (numMaterial > numPlane)
			numMaterial = numPlane;

		unsigned GeometryNum = numMaterial;
		unsigned remainder = numPlane % numMaterial;
		unsigned PlanePerGeometry = numPlane / numMaterial + (remainder > 0 ? 1 : 0);
		remainder -= (remainder > 0 ? 1 : 0);
		nebula_vertex_data_ * vd = new nebula_vertex_data_[vertexPerPlane * PlanePerGeometry];
		unsigned short * id = new unsigned short[indexPerPlane * PlanePerGeometry];
		unsigned PlaneCnt = 0;
		BoundingBox BB;
		PODVector<Geometry*> geometries;
		for (unsigned ii = 0; ii < GeometryNum; ++ii)
		{
			for (unsigned kk = 0; kk < PlanePerGeometry; ++kk, ++PlaneCnt)
			{
				for (unsigned jj = 0; jj < vertexPerPlane; ++jj)
				{
					unsigned idx = kk * vertexPerPlane + jj;
					assert(idx < vertexPerPlane * PlanePerGeometry);
					vd[idx].position = q[PlaneCnt] * vertices[jj];
					vd[idx].normal = q[PlaneCnt] * normal;
					vd[idx].uv = uv[jj];
					BB.Merge(vd[idx].position);
				}

				for (unsigned jj = 0; jj < indexPerPlane; ++jj)
				{
					unsigned idx = kk * indexPerPlane + jj;
					assert(idx < indexPerPlane * PlanePerGeometry);
					id[idx] = indices[jj] + kk * vertexPerPlane;
				}
			}

			/*create a geometry contain some planes*/
			VertexBuffer * vb(new VertexBuffer(ctx));
			IndexBuffer * ib(new IndexBuffer(ctx));
			Geometry * geom(new Geometry(ctx));
			vb->SetShadowed(true);
			PODVector<VertexElement> elements;
			elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
			elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
			elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
			vb->SetSize(PlanePerGeometry * vertexPerPlane, elements);
			vb->SetData(vd);

			ib->SetShadowed(true);
			ib->SetSize(PlanePerGeometry * indexPerPlane, false);
			ib->SetData(id);

			geom->SetVertexBuffer(0, vb);
			geom->SetIndexBuffer(ib);
			geom->SetDrawRange(TRIANGLE_LIST, 0, PlanePerGeometry * indexPerPlane);
			geometries.Push(geom);

			/*# of planes for next geometry*/
			PlanePerGeometry = numPlane / numMaterial + (remainder > 0 ? 1 : 0);
			remainder -= (remainder > 0 ? 1 : 0);
		}

		Model * fromScratchModel(new Model(ctx));
		fromScratchModel->SetNumGeometries(geometries.Size());
		for (unsigned ii = 0; ii < geometries.Size(); ++ii)
			fromScratchModel->SetGeometry(ii, 0, geometries[ii]);
		fromScratchModel->SetBoundingBox(BB);

		delete[] vd;
		delete[] id;

		return fromScratchModel;
	}

	static Material * CreateNebulaMaterial(Context* ctx, unsigned int TextureSize, const Color &color)
	{
		FastNoise perlin(Random(0, M_MAX_UNSIGNED));
		perlin.SetFractalOctaves(8);
		perlin.SetFrequency(0.04f);
		float ** noise = alloc2Darr<float>(TextureSize, TextureSize);
		for (int xx = 0; xx < TextureSize; ++xx)
		{
			for (int yy = 0; yy < TextureSize; ++yy)
			{
				noise[xx][yy] = perlin.GetPerlinFractal(xx, yy);
			}
		}
		normalize2Darr<float>(noise, TextureSize, TextureSize);
		SharedPtr <Texture2D> perlin2D(MakeShared<Texture2D>(ctx));
		perlin2D->SetNumLevels(1);
		if (perlin2D->SetSize(TextureSize, TextureSize, Graphics::GetRGBAFormat(), TEXTURE_DYNAMIC) == false)
		{
			URHO3D_LOGERROR(String("perlin2D->SetSize fail"));
		}
		SharedPtr<Image> pic(MakeShared<Image>(ctx));
		pic->SetSize(TextureSize, TextureSize, 4);
		for (int xx = 0; xx < TextureSize; ++xx)
		{
			for (int yy = 0; yy < TextureSize; ++yy)
			{
				float dist = (Vector2(xx, yy) - Vector2(TextureSize / 2, TextureSize / 2)).Length();
				Vector2 n(Vector2(xx, yy).Normalized() * 1000.0f);
				float a = Pow(1.0f - dist / TextureSize, 6.0f);

				Color c(color);
				c.a_ = Pow(noise[xx][yy], 4.0f) * a;
				pic->SetPixel(xx, yy, c);
			}
		}
		perlin2D->SetData(pic, true);
		release2Darr<float>(noise, TextureSize, TextureSize);

		ResourceCache * cache = ctx->GetSubsystem<ResourceCache>();
		Material * ret = new Material(ctx);
		ret->SetNumTechniques(1);
		ret->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffAlphaNebula.xml"), QUALITY_MAX);
		ret->SetTexture(TU_DIFFUSE, perlin2D);
		ret->SetCullMode(CULL_NONE);
		return ret;
	}

	void CreateNebulaBlob(Context* ctx, Node * node, const PODVector<Color> &colors, unsigned int TextureSize)
	{
		StaticModelGroup * s = node->CreateComponent<StaticModelGroup>();
		s->SetModel(CreateNebulaModel(ctx, colors.Size()));

		for (unsigned ii = 0; ii < colors.Size(); ++ii)
		{
			s->SetMaterial(ii, CreateNebulaMaterial(ctx, TextureSize, colors[ii]));
		}
	}
}
