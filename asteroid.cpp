#include "uv_mapper.hpp"
#include "FastNoise.h"
#include "asteroid.h"
#include <Urho3D/Urho3DAll.h>

namespace Urho3D
{
	typedef struct asteroid_vertex_s_
	{
		Vector3 position;
		Vector3 normal;
		Vector4 tangent;
		Vector2 uv;
	}asteroid_vertex_data_;

	static BoundingBox calculateBB(asteroid_vertex_data_ * vd, unsigned numVertices)
	{
		BoundingBox ret;
		for (unsigned ii = 0; ii < numVertices; ++ii)
			ret.Merge(vd[ii].position);

		return ret;
	}

	static unsigned CreatePlane(asteroid_vertex_data_ * vd, unsigned short * id, unsigned data_off, const Vector3 &start, const Vector3 &dir1, const Vector3 &dir2, float len1, float len2,
		unsigned division1, unsigned division2, const Vector3 &normal/*, const Vector2 &uvStart, const Vector2 &uvEnd*/)
	{
		unsigned idx = data_off;
		for (unsigned d1 = 0; d1 < division1; ++d1)
		{
			for (unsigned d2 = 0; d2 < division2; ++d2)
			{
				Vector3 corners[4] = {  
					Vector3(start + (dir1*d1*len1/ division1) + (dir2*d2*len2 / division2)),
					Vector3(start + (dir1*(d1+1)*len1 / division1) + (dir2*d2*len2 / division2)),
					Vector3(start + (dir1*d1*len1 / division1) + (dir2*(d2+1)*len2 / division2)),
					Vector3(start + (dir1*(d1+1)*len1 / division1) + (dir2*(d2+1)*len2 / division2))
				};
#if 0
				Vector2 uvs[4] = {
					Vector2(uvStart + Vector2(d1*(uvEnd- uvStart).x_/ division1, d2*(uvEnd - uvStart).y_ / division2)),
					Vector2(uvStart + Vector2((d1+1)*(uvEnd - uvStart).x_ / division1, d2*(uvEnd - uvStart).y_ / division2)),
					Vector2(uvStart + Vector2(d1*(uvEnd - uvStart).x_ / division1, (d2+1)*(uvEnd - uvStart).y_ / division2)),
					Vector2(uvStart + Vector2((d1+1)*(uvEnd - uvStart).x_ / division1, (d2+1)*(uvEnd - uvStart).y_ / division2))
				};
#endif
				const unsigned short triangleIdxList[6] = {  
					0, 1, 3, 0, 3, 2
				};

				for (unsigned ii = 0; ii < 6; ++ii)
				{
					vd[idx + ii].position = corners[triangleIdxList[ii]];
					vd[idx + ii].normal = normal;
					//vd[idx + ii].uv = uvs[triangleIdxList[ii]];
					id[idx + ii] = idx + ii;
				}
				idx += 6;
			}
		}

		return idx - data_off;
	}

	static void cutByPlane(asteroid_vertex_data_ * vd, unsigned numVertices, const Plane &p)
	{
		for (unsigned ii = 0; ii < numVertices; ++ii)
		{
			if (p.Distance(vd[ii].position) < 0.0f)
			{
				vd[ii].position = p.Project(vd[ii].position);
			}
		}
	}

	static void calculateNormal(asteroid_vertex_data_ * vd, unsigned numVertices)
	{
		for (unsigned ii = 0; ii < numVertices; ii += 3)
		{
			Vector3 n((vd[ii + 1].position - vd[ii].position).CrossProduct((vd[ii + 2].position - vd[ii].position)).Normalized());
			for (unsigned jj = 0; jj < 3; ++jj)
				vd[ii + jj].normal = n;
		}
	}

	static unsigned numCornersBehindPlane(const BoundingBox &bb, const Plane &p)
	{
		Vector3 bbCorners[8] = {
			bb.min_,
			bb.max_,
			Vector3(bb.min_.x_, bb.min_.y_, bb.max_.z_),
			Vector3(bb.min_.x_, bb.max_.y_,bb.min_.z_),
			Vector3(bb.max_.x_, bb.min_.y_, bb.min_.z_),
			Vector3(bb.max_.x_, bb.max_.y_, bb.min_.z_),
			Vector3(bb.max_.x_, bb.min_.y_, bb.max_.z_),
			Vector3(bb.min_.x_, bb.max_.y_, bb.max_.z_)
		};
		unsigned cnt = 0;

		for (unsigned ii = 0; ii < 8; ++ii)
		{
			if (p.Distance(bbCorners[ii]) < 0.0f)
			{
				cnt += 1;
			}
		}
		return cnt;
	}

	static Model * CreateMesh(Context* ctx, unsigned edge_division)
	{
		/*create a box from (-0.5, -0.5) ~ (0.5, 0.5)*/
		const Vector3 start[6] = {
			Vector3(-0.5f, -0.5f, 0.5f),			/*+Z*/
			Vector3(-0.5f, -0.5f, -0.5f),		/*-Z*/
			Vector3(0.5f, -0.5f, -0.5f),			/*+X*/
			Vector3(-0.5f, -0.5f, -0.5f),		/*-X*/
			Vector3(-0.5f, 0.5f, -0.5f),			/*+Y*/
			Vector3(-0.5f, -0.5f, -0.5f)			/*-Y*/
		};

		const Vector3 dir1[6] = {
			Vector3::RIGHT,
			Vector3::UP,
			Vector3::UP,
			Vector3::FORWARD,
			Vector3::FORWARD,
			Vector3::RIGHT
		};

		const Vector3 dir2[6] = {
			Vector3::UP,
			Vector3::RIGHT,
			Vector3::FORWARD,
			Vector3::UP,
			Vector3::RIGHT,
			Vector3::FORWARD
		};

		const Vector3 normals[6] = {
			Vector3::FORWARD,
			Vector3::BACK,
			Vector3::RIGHT,
			Vector3::LEFT,
			Vector3::UP,
			Vector3::DOWN
		};
		const unsigned numTriangles = 6 * edge_division * edge_division * 2;
		const unsigned numVertices = numTriangles * 3;

		asteroid_vertex_data_ * vd = new asteroid_vertex_data_[numVertices];
		unsigned short * id = new unsigned short[numVertices];
		unsigned idx = 0;
		for (unsigned face = 0; face < 6; ++face)
		{
			idx += CreatePlane(vd, id, idx, start[face], dir1[face], dir2[face], 1.0f, 1.0f, edge_division, edge_division, normals[face]);
		}

		/*random scale*/
		Vector3 scale(Random(0.5f, 1.5f), Random(0.5f, 1.5f), Random(0.5f, 1.5f));
		for (unsigned ii = 0; ii < numVertices; ++ii)
		{
			vd[ii].position *= scale;
		}

		/*random cut with plane*/
		const unsigned numCutPlane = 8;
		BoundingBox BB = calculateBB(vd, numVertices);
		const float plane_points_x_start[numCutPlane] = { 0, BB.min_.x_ , 0, BB.min_.x_, 0, BB.min_.x_ ,0, BB.min_.x_ };
		const float plane_points_x_end[numCutPlane] = { BB.max_.x_, 0, BB.max_.x_ , 0, BB.max_.x_, 0, BB.max_.x_ , 0 };
		const float plane_points_y_start[numCutPlane] = { 0, 0, 0, 0, BB.min_.y_ ,BB.min_.y_ ,BB.min_.y_ ,BB.min_.y_ };
		const float plane_points_y_end[numCutPlane] = { BB.max_.y_, BB.max_.y_, BB.max_.y_, BB.max_.y_, 0, 0, 0, 0 };
		const float plane_points_z_start[numCutPlane] = { 0, 0, BB.min_.z_, BB.min_.z_, 0, 0, BB.min_.z_, BB.min_.z_ };
		const float plane_points_z_end[numCutPlane] = { BB.max_.z_, BB.max_.z_, 0, 0, BB.max_.z_, BB.max_.z_, 0, 0 };
#if 0
		Vector3 plane_points[8] = {
			Vector3(Random(0.0f, BB.max_.x_), Random(0.0f, BB.max_.y_), Random(0.0f, BB.max_.z_)),
			Vector3(Random(BB.min_.x_, 0.0f), Random(0.0f, BB.max_.y_), Random(0.0f, BB.max_.z_)),
			Vector3(Random(0.0f, BB.max_.x_), Random(0.0f, BB.max_.y_), Random(BB.min_.z_, 0.0f)),
			Vector3(Random(BB.min_.x_, 0.0f), Random(0.0f, BB.max_.y_), Random(BB.min_.z_, 0.0f)),
			Vector3(Random(0.0f, BB.max_.x_), Random(BB.min_.y_, 0.0f), Random(0.0f, BB.max_.z_)),
			Vector3(Random(BB.min_.x_, 0.0f), Random(BB.min_.y_, 0.0f), Random(0.0f, BB.max_.z_)),
			Vector3(Random(0.0f, BB.max_.x_), Random(BB.min_.y_, 0.0f), Random(BB.min_.z_, 0.0f)),
			Vector3(Random(BB.min_.x_, 0.0f), Random(BB.min_.y_, 0.0f), Random(BB.min_.z_, 0.0f))
		};
#endif
		for (unsigned ii = 0; ii < numCutPlane; ++ii)
		{
			while (true)
			{
				Quaternion q(Random(-20.0f, 20.0f), Random(-20.0f, 20.0f), Random(-20.0f, 20.0f));
				Vector3 plane_point(Random(plane_points_x_start[ii], plane_points_x_end[ii]), Random(plane_points_y_start[ii], plane_points_y_end[ii]),
					Random(plane_points_z_start[ii], plane_points_z_end[ii]));
				Plane plane(-(q * plane_point), plane_point);
				if (numCornersBehindPlane(BB, plane) == 1)
				{
					cutByPlane(vd, numVertices, plane);
					break;
				}
			}
		}
		calculateNormal(vd, numVertices);

		/*displace with noise*/
		FastNoise *cellular = new FastNoise(Random(0, M_MAX_UNSIGNED));
		cellular->SetFrequency(0.02f);
		cellular->SetCellularReturnType(FastNoise::Distance);
		const Vector3 noiseScale(200.0f, 200.0f, 200.0f);
		for (unsigned ii = 0; ii < numVertices; ++ii)
		{
			Vector3 p(vd[ii].position * noiseScale);
			float displace = cellular->GetCellular(p.x_, p.y_, p.z_) / 4.0f;
			vd[ii].position = vd[ii].position + displace * vd[ii].position;
		}
		delete cellular;
		calculateNormal(vd, numVertices);

		VertexBuffer * vb(new VertexBuffer(ctx));
		IndexBuffer * ib(new IndexBuffer(ctx));
		Geometry * geom(new Geometry(ctx));
		vb->SetShadowed(true);
		PODVector<VertexElement> elements;
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
		elements.Push(VertexElement(TYPE_VECTOR4, SEM_TANGENT));
		elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
		vb->SetSize(numVertices, elements);
		vb->SetData(vd);

		ib->SetShadowed(true);
		ib->SetSize(numVertices, false);
		ib->SetData(id);

		geom->SetVertexBuffer(0, vb);
		geom->SetIndexBuffer(ib);
		geom->SetDrawRange(TRIANGLE_LIST, 0, numVertices);

		BB = calculateBB(vd, numVertices);

		Model * fromScratchModel(new Model(ctx));
		fromScratchModel->SetNumGeometries(1);
		fromScratchModel->SetGeometry(0, 0, geom);
		fromScratchModel->SetBoundingBox(BB);

		delete[] vd;
		delete[] id;
		return fromScratchModel;
	}

	void CreateAsteroidBlob(Context* ctx, Node * node, const String &srcModel)
	{
		StaticModelGroup * s = node->CreateComponent<StaticModelGroup>();
		Model * model = CreateMesh(ctx, 20);
		if (model != nullptr)
			s->SetModel(model);
	}
}
