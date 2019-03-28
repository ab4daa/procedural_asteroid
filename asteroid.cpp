#include <vector>
#include "uv_mapper.hpp"
#include "FastNoise.h"
#include "rock_texture.h"
#include "asteroid.h"
#include <Urho3D/Urho3DAll.h>

//uncomment this if u want to subdivide more
//#define DETAIL_ASTEROID_MODEL		1
namespace Urho3D
{
	struct asteroid_vertex_data_
	{
		Vector3 position;
		Vector3 normal;
		Vector4 tangent;
		Vector2 uv;
	};

	#ifdef DETAIL_ASTEROID_MODEL
	typedef unsigned IBtype;
	#else
	typedef unsigned short IBtype;
	#endif


	static BoundingBox calculateBB(const PODVector<asteroid_vertex_data_> &vd)
	{
		BoundingBox ret;
		for (unsigned ii = 0; ii < vd.Size(); ++ii)
			ret.Merge(vd[ii].position);

		return ret;
	}

	class XZplaneIterator{
	public:
		virtual IBtype iter(unsigned i) const = 0;
		virtual ~XZplaneIterator() = default;
	};

	class TopBottomPlane : public XZplaneIterator
	{
	public:
		TopBottomPlane(IBtype start_offset, const IntVector3 &Segment)
			: XZplaneIterator(), off(start_offset), seg(Segment)
		{}
		IBtype iter(unsigned i) const override
		{
			if(i < seg.z_)
			{
				return off + i;
			}
			else if(i < seg.z_ + seg.x_)
			{
				unsigned xx = i - seg.z_;
				return off + seg.z_ + xx * (seg.z_ + 1);
			}
			else if(i < seg.z_ + seg.x_ + seg.z_)
			{
				unsigned zz = seg.z_ + seg.x_ + seg.z_ - i;
				return off + seg.x_ * (seg.z_ + 1) + zz;
			}
			else if(i < 2*seg.x_ + 2*seg.z_)
			{
				unsigned xx = 2*seg.x_ + 2*seg.z_ - i;
				return off + xx * (seg.z_ + 1);
			}
			else if(i == 2*seg.x_ + 2*seg.z_)
			{
				return off;
			}
			else
			{
				URHO3D_LOGERROR("TopBottomPlane: exceed boundary");
				return off;
			}
		}
	private:
		const IBtype off;
		const IntVector3 seg;
	};

	class MiddlePlane : public XZplaneIterator
	{
	public:
		MiddlePlane(IBtype start_offset, const IntVector3 &Segment)
			: XZplaneIterator(), off(start_offset), seg(Segment)
		{}
		IBtype iter(unsigned i) const override
		{
			if(i < 2*seg.x_ + 2*seg.z_)
			{
				return off + i;
			}
			else if(i == 2*seg.x_ + 2*seg.z_)
			{
				return off;
			}
			else
			{
				URHO3D_LOGERROR("MiddlePlane: exceed boundary");
				return off;
			}
		}
	private:
		const IBtype off;
		const IntVector3 seg;
	};

	/*
	quad:
	i0			i1

	i2			i3
	*/
	static void buildQuadIndex(PODVector<IBtype> &id, IBtype i0, IBtype i1, IBtype i2, IBtype i3, const bool bottom)
	{
		if (!bottom)
		{		//CW
			id.Push(i0); id.Push(i1); id.Push(i3);
			id.Push(i0); id.Push(i3); id.Push(i2);
		}
		else
		{		//CCW
			id.Push(i0); id.Push(i3); id.Push(i1);
			id.Push(i0); id.Push(i2); id.Push(i3);
		}
	}

	static IBtype CreateCubeTopBottomPlane(PODVector<asteroid_vertex_data_> &vd, PODVector<IBtype> &id, const Vector3 &Size, const IntVector3 &Segment, 
		const bool bottom)
	{
		const unsigned vdStart = vd.Size();
		const Vector3 half(Size / 2.0f);

		for (unsigned xx = 0; xx < Segment.x_ + 1; ++xx)
		{
			for (unsigned zz = 0; zz < Segment.z_ + 1; ++zz)
			{
				asteroid_vertex_data_ data;
				data.position = Vector3(-half.x_ + xx * Size.x_ / Segment.x_, bottom ? -half.y_ : half.y_, -half.z_ + zz * Size.z_ / Segment.z_);
				vd.Push(data);
			}
		}

		for (unsigned xx = 0; xx < Segment.x_; ++xx)
		{
			for (unsigned zz = 0; zz < Segment.z_; ++zz)
			{
				IBtype i0 = vdStart + xx * (Segment.z_ + 1) + 1 + zz;
				IBtype i1 = vdStart + (xx + 1) * (Segment.z_ + 1) + 1 + zz;
				IBtype i2 = vdStart + xx * (Segment.z_ + 1) + zz;
				IBtype i3 = vdStart + (xx + 1) * (Segment.z_ + 1) + zz;
				buildQuadIndex(id, i0, i1, i2, i3, bottom);
			}
		}

		return vdStart;
	}
	
	static IBtype CreateMiddleXZVertices(PODVector<asteroid_vertex_data_> &vd, const Vector3 &Size, const IntVector3 &Segment, float y)
	{
		const unsigned vdStart = vd.Size();
		const Vector3 half(Size / 2.0f);
		for(unsigned zz = 0; zz < Segment.z_ + 1; ++zz)
		{
			asteroid_vertex_data_ data;
			data.position = Vector3(-half.x_, y, -half.z_ + zz * (Size.z_ / Segment.z_));
			vd.Push(data);
		}
		for(unsigned xx = 1; xx < Segment.x_; ++xx)
		{
			asteroid_vertex_data_ data;
			data.position = Vector3(-half.x_ + xx * (Size.x_ / Segment.x_), y, half.z_);
			vd.Push(data);
		}
		for(unsigned zz = 0; zz < Segment.z_ + 1; ++zz)
		{
			asteroid_vertex_data_ data;
			data.position = Vector3(half.x_, y, half.z_ - zz * (Size.z_ / Segment.z_));
			vd.Push(data);
		}
		for(unsigned xx = 1; xx < Segment.x_; ++xx)
		{
			asteroid_vertex_data_ data;
			data.position = Vector3(half.x_ - xx * (Size.x_ / Segment.x_), y, -half.z_);
			vd.Push(data);
		}
		return vdStart;
	}

	/*create cube without duplicated vertices*/
	static void CreateCube(PODVector<asteroid_vertex_data_> &vd, PODVector<IBtype> &id, const Vector3 &Size, const IntVector3 &Segment)
	{
		if (Segment.x_ <= 0 || Segment.y_ <= 0 || Segment.z_ <= 0 || Size.x_ <= 0.0f || Size.y_ <= 0.0f || Size.z_ <= 0.0f)
		{
			URHO3D_LOGERROR("CreateCube: size or segment cannot <= 0");
			return;
		}

		const unsigned numVertices = (Segment.x_ + 1) * (Segment.y_ + 1) * 2 + (Segment.y_ + 1)*(Segment.z_ + 1) * 2 + (Segment.x_ + 1)*(Segment.z_ + 1) * 2
			- 4 * (Segment.x_ - 1) - 4 * (Segment.y_ - 1) - 4 * (Segment.z_ - 1) - 8 * 2;
		const unsigned numIndices = Segment.x_*Segment.y_ * 2 * 3 * 2 + Segment.y_*Segment.z_ * 2 * 3 * 2 + Segment.x_*Segment.z_ * 2 * 3 * 2;

		#ifndef DETAIL_ASTEROID_MODEL
		if (numVertices > 65535)
		{
			URHO3D_LOGERROR("CreateCube: index buffer need larger type; plz define DETAIL_ASTEROID_MODEL");
			return;
		}
		#endif

		vd.Clear();
		vd.Reserve(numVertices);
		id.Clear();
		id.Reserve(numIndices);

		/*bottom xz plane*/
		IBtype bottomOff = CreateCubeTopBottomPlane(vd, id, Size, Segment, true);
		XZplaneIterator * lastPlane = new TopBottomPlane(bottomOff, Segment);
		XZplaneIterator * newPlane = nullptr;
		for(unsigned ii=0; ii<Segment.y_-1; ++ii)
		{
			IBtype middleOff = CreateMiddleXZVertices(vd,Size, Segment, -Size.y_/2.0f + (ii+1)*(Size.y_/Segment.y_));
			newPlane = new MiddlePlane(middleOff, Segment);
			
			for(unsigned jj=0; jj<Segment.x_*2 + Segment.z_*2; ++jj)
			{
				buildQuadIndex(id, newPlane->iter(jj+1), newPlane->iter(jj), lastPlane->iter(jj+1), lastPlane->iter(jj), false);
			}
			delete lastPlane;
			lastPlane = newPlane;
			newPlane = nullptr;
		}
		IBtype topOff = CreateCubeTopBottomPlane(vd, id, Size, Segment, false);
		newPlane = new TopBottomPlane(topOff, Segment);
		for(unsigned jj=0; jj<Segment.x_*2 + Segment.z_*2; ++jj)
		{
			buildQuadIndex(id, newPlane->iter(jj+1), newPlane->iter(jj), lastPlane->iter(jj+1), lastPlane->iter(jj), false);
		}
		delete newPlane;
		delete lastPlane;

		if(vd.Size() != numVertices)
		{
			URHO3D_LOGERROR("numVertices calculation error");
		}

		if(id.Size() != numIndices)
		{
			URHO3D_LOGERROR("numIndices calculation error");
		}
	}

	/*Create sphere without duplicated vertices, github.com/caosdoar/spheres*/
	static void CreateSphere(PODVector<asteroid_vertex_data_> &vd, PODVector<IBtype> &id, const float radius, const unsigned parallels_count, const unsigned meridians_count)
	{
		if (radius <= 0.0f || parallels_count < 3 || meridians_count < 3)
		{
			URHO3D_LOGERROR("CreateSphere: parameter error");
			return;
		}

		const unsigned numVertices = 2 + (parallels_count-1) * meridians_count;
		const unsigned numIndices = 2 * meridians_count * 3 + (parallels_count - 2) * meridians_count * 6;

		#ifndef DETAIL_ASTEROID_MODEL
		if (numVertices > 65535)
		{
			URHO3D_LOGERROR("CreateSphere: index buffer need larger type; plz define DETAIL_ASTEROID_MODEL");
			return;
		}
		#endif

		vd.Clear();
		vd.Reserve(numVertices);
		id.Clear();
		id.Reserve(numIndices);
		{
			asteroid_vertex_data_ north;
			north.position = Vector3(0.0f, radius, 0.0f);
			vd.Push(north);
		}
		for(unsigned j=0; j<parallels_count-1; ++j)
		{
			float const polar = 180.0f * float(j+1) / float(parallels_count);
			float const sp = Sin(polar);
			float const cp = Cos(polar);
			for(unsigned i=0; i<meridians_count; ++i)
			{
				float const azimuth = 360.0f * float(i) / float(meridians_count);
				float const sa = Sin(azimuth);
				float const ca = Cos(azimuth);
				float const x = sp * ca * radius;
				float const y = cp * radius;
				float const z = sp * sa * radius;
				asteroid_vertex_data_ point;
				point.position = Vector3(x, y, z);
				vd.Push(point);
			}
		}
		{
			asteroid_vertex_data_ south;
			south.position = Vector3(0.0f, -radius, 0.0f);
			vd.Push(south);
		}

		for(unsigned i=0; i<meridians_count; ++i)
		{
			IBtype const a = i + 1;
			IBtype const b = (i + 1) % meridians_count + 1;
			id.Push(0);
			id.Push(b);
			id.Push(a);
		}
		for (unsigned  j = 0; j < parallels_count - 2; ++j)
		{
			IBtype aStart = j * meridians_count + 1;
			IBtype bStart = (j + 1) * meridians_count + 1;
			for (unsigned i = 0; i < meridians_count; ++i)
			{
				const IBtype a = aStart + i;
				const IBtype a1 = aStart + (i + 1) % meridians_count;
				const IBtype b = bStart + i;
				const IBtype b1 = bStart + (i + 1) % meridians_count;
				id.Push(a);
				id.Push(a1);
				id.Push(b1);
				id.Push(a);
				id.Push(b1);
				id.Push(b);
			}
		}
		for (unsigned i = 0; i < meridians_count; ++i)
		{
			IBtype const a = i + meridians_count * (parallels_count - 2) + 1;
			IBtype const b = (i + 1) % meridians_count + meridians_count * (parallels_count - 2) + 1;
			id.Push(vd.Size()-1);
			id.Push(a);
			id.Push(b);
		}	

		if(vd.Size() != numVertices)
		{
			URHO3D_LOGERROR("numVertices calculation error");
		}

		if(id.Size() != numIndices)
		{
			URHO3D_LOGERROR("numIndices calculation error");
		}	
	}

	/*cut north pole and a round of triangles*/
	static void SplitSphere(const PODVector<asteroid_vertex_data_> &vd, const PODVector<IBtype> &id, const unsigned parallels_count, const unsigned meridians_count, 
		Vector< PODVector<IBtype> > &parts)
	{
		const unsigned numIndicesOfNorth = meridians_count * 3 + meridians_count * 6;
		parts.Resize(2);
		parts[0].Clear();
		parts[1].Clear();
		parts[0].Reserve(numIndicesOfNorth);
		parts[1].Reserve(id.Size() - numIndicesOfNorth);
		for (unsigned ii = 0; ii < numIndicesOfNorth; ++ii)
		{
			parts[0].Push(id[ii]);
		}
		for (unsigned ii = numIndicesOfNorth; ii < id.Size(); ++ii)
		{
			parts[1].Push(id[ii]);
		}
	}

	/*split cube bottom plane*/
	static void SplitCubeBottom(const PODVector<asteroid_vertex_data_> &vd, const PODVector<IBtype> &id, const IntVector3 &Segment, 
		Vector< PODVector<IBtype> > &parts)
	{
		const unsigned numIndicesOfBottom = Segment.x_ * Segment.z_ * 2 * 3;
		parts.Resize(2);
		parts[0].Clear();
		parts[1].Clear();
		parts[0].Reserve(numIndicesOfBottom);
		parts[1].Reserve(id.Size() - numIndicesOfBottom);
		for (unsigned ii = 0; ii < numIndicesOfBottom; ++ii)
		{
			parts[0].Push(id[ii]);
		}
		for (unsigned ii = numIndicesOfBottom; ii < id.Size(); ++ii)
		{
			parts[1].Push(id[ii]);
		}
	}

	/*split mesh to 2 parts by a plane; output 2 index buffers, they use the same vertex buffer*/
	static void SplitMesh(const PODVector<asteroid_vertex_data_> &vd, const PODVector<IBtype> &id, const Plane &p, 
		Vector< PODVector<IBtype> > &parts)
	{
		if(id.Size() % 3)
		{
			URHO3D_LOGERROR("SplitCube: size of index buffer mod 3 != 0");
			return;
		}
		const unsigned numTriangles = id.Size() / 3;
		parts.Resize(2);
		parts[0].Clear();
		parts[1].Clear();
		
		for(unsigned ii=0; ii<numTriangles; ++ii)
		{
			unsigned behindPlane = 0;
			for(unsigned jj=0; jj<3; ++jj)
			{
				if(p.Distance(vd[ id[ii*3 + jj] ].position) < 0.0f)
				{
					behindPlane += 1;
				}
			}

			if(behindPlane > 2)
			{
				for(unsigned jj=0; jj<3; ++jj)
				{
					parts[0].Push(id[ii*3 + jj]);
				}
			}
			else
			{
				for(unsigned jj=0; jj<3; ++jj)
				{
					parts[1].Push(id[ii*3 + jj]);
				}
			}
		}
	}

	static void cutByPlane(PODVector<asteroid_vertex_data_> &vd, const Plane &p)
	{
		for (unsigned ii = 0; ii < vd.Size(); ++ii)
		{
			if (p.Distance(vd[ii].position) < 0.0f)
			{
				vd[ii].position = p.Project(vd[ii].position);
			}
		}
	}

	static Vector3 calculateCenter(const PODVector<asteroid_vertex_data_> &vd)
	{
		Vector3 ret(Vector3::ZERO);
		const unsigned sz = vd.Size();

		for(unsigned ii=0; ii<sz; ++ii)
		{
			ret += vd[ii].position;
		}

		ret /= sz;
		return ret;
	}

	static void calculateNormal(PODVector<asteroid_vertex_data_> &vd, const PODVector<IBtype> &id)
	{
		if(id.Size() % 3)
		{
			URHO3D_LOGERROR("calculateNormal: size of index buffer mod 3 != 0");
			return;
		}
		const unsigned numTriangles = id.Size() / 3;
		const unsigned numVertices = vd.Size();

		for (unsigned ii = 0; ii < numVertices; ++ii)
		{
			Vector3 n(Vector3::ZERO);
			for (unsigned jj = 0; jj < numTriangles; ++jj)
			{
				bool found = false;
				for(unsigned kk=0; kk<3; ++kk)
				{
					if(id[jj*3 + kk] == ii)
					{
						found = true;
						break;
					}
				}

				if(found)
				{
					Vector3 triNormal((vd[ id[jj*3+1] ].position - vd[ id[jj*3] ].position).CrossProduct(vd[ id[jj*3+2] ].position - vd[ id[jj*3] ].position));
					n += triNormal;
				}
			}
			
			vd[ii].normal = n.Normalized();
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

	static void autoUV(const PODVector<asteroid_vertex_data_> &vd, const PODVector<IBtype> &id,
		PODVector<asteroid_vertex_data_> &outVd, PODVector<IBtype> &outId)
	{
		std::vector<float> vertices, outVertices;
		std::vector<int> indices, outIndices;
		std::vector<float> outUv;

		vertices.reserve(vd.Size() * 3);
		indices.reserve(id.Size());
		for (unsigned ii = 0; ii < vd.Size(); ++ii)
		{
			vertices.push_back(vd[ii].position.x_);
			vertices.push_back(vd[ii].position.y_);
			vertices.push_back(vd[ii].position.z_);
		}
		for (unsigned ii = 0; ii < id.Size(); ++ii)
		{
			indices.push_back(id[ii]);
		}

		uvMap(vertices, indices, outVertices, outIndices, outUv, nullptr);

		for (unsigned ii = 0; ii < outVertices.size(); ii += 3)
		{
			asteroid_vertex_data_ v;
			v.position = Vector3(outVertices[ii], outVertices[ii + 1], outVertices[ii + 2]);
			outVd.Push(v);
		}

		for (unsigned ii = 0; ii < outVd.Size(); ++ii)
		{
			outVd[ii].uv = Vector2(outUv[ii * 2], outUv[ii * 2 + 1]);
		}

		/*recover normal*/
		for (unsigned ii = 0; ii < outVd.Size(); ++ii)
		{
			for (unsigned jj = 0; jj < vd.Size(); ++jj)
			{
				if (outVd[ii].position == vd[jj].position)
				{
					outVd[ii].normal = vd[jj].normal;
					break;
				}
			}
		}

		for (unsigned ii = 0; ii < outIndices.size(); ++ii)
		{
			outId.Push(outIndices[ii]);
		}
	}

	/* Lengyel, Eric. “Computing Tangent Space Basis Vectors for an Arbitrary Mesh”. Terathon Software, 2001. http://terathon.com/code/tangent.html */
	static void CalculateTangentArray(PODVector<asteroid_vertex_data_> &vd, const PODVector<IBtype> &id)
	{
		Vector<Vector3> tan1, tan2;
		tan1.Resize(vd.Size());
		tan2.Resize(vd.Size());

		for(unsigned a=0; a<id.Size(); a+=3)
		{
			IBtype i1 = id[a];
			IBtype i2 = id[a+1];
			IBtype i3 = id[a+2];

			const Vector3 &v1 = vd[i1].position;
			const Vector3 &v2 = vd[i2].position;
			const Vector3 &v3 = vd[i3].position;

			const Vector2 &w1 = vd[i1].uv;
			const Vector2 &w2 = vd[i2].uv;
			const Vector2 &w3 = vd[i3].uv;

			float x1 = v2.x_ - v1.x_;
			float x2 = v3.x_ - v1.x_;
			float y1 = v2.y_ - v1.y_;
			float y2 = v3.y_ - v1.y_;
			float z1 = v2.z_ - v1.z_;
			float z2 = v3.z_ - v1.z_;

			float s1 = w2.x_ - w1.x_;
			float s2 = w3.x_ - w1.x_;
			float t1 = w2.y_ - w1.y_;
			float t2 = w3.y_ - w1.y_;

			float r = 1.0f / (s1 * t2 - s2 * t1);
			Vector3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r);
        	Vector3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
                (s1 * z2 - s2 * z1) * r);

			tan1[i1] += sdir;
			tan1[i2] += sdir;
			tan1[i3] += sdir;
			
			tan2[i1] += tdir;
			tan2[i2] += tdir;
			tan2[i3] += tdir;
		}

		for(unsigned a=0; a<vd.Size(); ++a)
		{
			const Vector3 &n = vd[a].normal;
			const Vector3 &t = tan1[a];

			// Gram-Schmidt orthogonalize
        	Vector3 tan3((t - n * (n.DotProduct(t))).Normalized());

			// Calculate handedness
			float w = (n.CrossProduct(t)).DotProduct(tan2[a]) < 0.0f ? -1.0f : 1.0f;

			vd[a].tangent = Vector4(tan3, w);
		}
	}

	static Model * CreateMesh(Context* ctx, unsigned edge_division)
	{
		bool useSphere = false;
		if (Random(1.0f) < 0.5f)
			useSphere = true;
		PODVector<asteroid_vertex_data_> vd;
		PODVector<IBtype> id;
		BoundingBox BB;
		const IntVector3 segment(edge_division, edge_division, edge_division);
		
		if(useSphere)
			CreateSphere(vd, id, 0.5f, edge_division/2, edge_division);
		else
			CreateCube(vd, id, Vector3::ONE, segment);

		/*random scale*/
		Vector3 scale(Random(0.5f, 1.5f), Random(0.5f, 1.5f), Random(0.5f, 1.5f));
		for (unsigned ii = 0; ii < vd.Size(); ++ii)
		{
			vd[ii].position *= scale;
		}

		/*random cut with plane*/
		const unsigned numCutPlane = 8;
		BB = calculateBB(vd);
		const float plane_points_x_start[numCutPlane] = { 0, BB.min_.x_ , 0, BB.min_.x_, 0, BB.min_.x_ ,0, BB.min_.x_ };
		const float plane_points_x_end[numCutPlane] = { BB.max_.x_, 0, BB.max_.x_ , 0, BB.max_.x_, 0, BB.max_.x_ , 0 };
		const float plane_points_y_start[numCutPlane] = { 0, 0, 0, 0, BB.min_.y_ ,BB.min_.y_ ,BB.min_.y_ ,BB.min_.y_ };
		const float plane_points_y_end[numCutPlane] = { BB.max_.y_, BB.max_.y_, BB.max_.y_, BB.max_.y_, 0, 0, 0, 0 };
		const float plane_points_z_start[numCutPlane] = { 0, 0, BB.min_.z_, BB.min_.z_, 0, 0, BB.min_.z_, BB.min_.z_ };
		const float plane_points_z_end[numCutPlane] = { BB.max_.z_, BB.max_.z_, 0, 0, BB.max_.z_, BB.max_.z_, 0, 0 };
		for (unsigned ii = 0; ii < numCutPlane; ++ii)
		{
			while (true)
			{
				Quaternion q(Random(-30.0f, 30.0f), Random(-30.0f, 30.0f), Random(-30.0f, 30.0f));
				Vector3 plane_point(Random(plane_points_x_start[ii], plane_points_x_end[ii]), Random(plane_points_y_start[ii], plane_points_y_end[ii]),
					Random(plane_points_z_start[ii], plane_points_z_end[ii]));
				Plane plane(-(q * plane_point), plane_point);
				if (numCornersBehindPlane(BB, plane) == 1)
				{
					cutByPlane(vd, plane);
					break;
				}
			}
		}
		calculateNormal(vd, id);

		/*displace with noise*/
		FastNoise *cellular = new FastNoise(Random(0, M_MAX_UNSIGNED));
		cellular->SetFrequency(0.02f);
		cellular->SetCellularReturnType(FastNoise::Distance);
		const Vector3 noiseScale(200.0f, 200.0f, 200.0f);
		for (unsigned ii = 0; ii < vd.Size(); ++ii)
		{
			Vector3 p(vd[ii].position * noiseScale);
			float displace = cellular->GetCellular(p.x_, p.y_, p.z_) / 4.0f;
			vd[ii].position = vd[ii].position + displace * vd[ii].normal;
		}
		delete cellular;
		BB = calculateBB(vd);
		calculateNormal(vd, id);
		Vector3 center = calculateCenter(vd);

		Vector< PODVector<IBtype> > parts;
#if 0
		SplitCubeBottom(vd, id, segment, parts);
#else
		{
			Plane split(Vector3::UP, center);
			SplitMesh(vd, id, split, parts);
		}
#endif
		
		Vector< PODVector<asteroid_vertex_data_> > new_parts_vd(parts.Size());
		Vector< PODVector<IBtype> > new_parts_id(parts.Size());
		for (unsigned ii = 0; ii < parts.Size(); ++ii)
		{
			autoUV(vd, parts[ii], new_parts_vd[ii], new_parts_id[ii]);
		}

		PODVector<Geometry*> geometries;
		for (unsigned ii = 0; ii < parts.Size(); ++ii)
		{
			VertexBuffer * vb(new VertexBuffer(ctx));
			IndexBuffer * ib(new IndexBuffer(ctx));
			Geometry * geom(new Geometry(ctx));
			vb->SetShadowed(true);
			PODVector<VertexElement> elements;
			elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
			elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
			elements.Push(VertexElement(TYPE_VECTOR4, SEM_TANGENT));
			elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
			vb->SetSize(new_parts_vd[ii].Size(), elements);
			vb->SetData(new_parts_vd[ii].Buffer());

			ib->SetShadowed(true);
			#ifdef DETAIL_ASTEROID_MODEL
			bool largeIndices = true;
			#else
			bool largeIndices = false;
			#endif
			ib->SetSize(new_parts_id[ii].Size(), largeIndices);
			ib->SetData(new_parts_id[ii].Buffer());

			geom->SetVertexBuffer(0, vb);
			geom->SetIndexBuffer(ib);
			geom->SetDrawRange(TRIANGLE_LIST, 0, new_parts_id[ii].Size());
			geometries.Push(geom);
		}
		
		Model * fromScratchModel(new Model(ctx));
		fromScratchModel->SetNumGeometries(geometries.Size());
		for (unsigned ii = 0; ii < geometries.Size(); ++ii)
		{
			fromScratchModel->SetGeometry(ii, 0, geometries[ii]);
		}
		fromScratchModel->SetBoundingBox(BB);
		

		return fromScratchModel;
	}

	static float sampleHeightMap(int x, int y, const Image * heightMap)
	{
		const int w = heightMap->GetWidth();
		const int h = heightMap->GetHeight();

		if(x < 0)
		{
			x = -x;
		}
		else if(x >= w)
		{
			x = w - 1 - (x-w+1);
		}

		if(y < 0)
		{
			y = -y;
		}
		else if(y >= h)
		{
			y = h - 1 - (y-h+1);
		}		

		if(x >= 0 && x < w && y >= 0 && y < h)
			return heightMap->GetPixel(x, y).r_;
		else
			return 0.0f;
	}

	// stackoverflow.com/questions/5281261/generating-a-normal-map-from-a-height-map
	static Image * CalculateBumpMap(Context* ctx, const Image * diffuse)
	{
		const int w = diffuse->GetWidth();
		const int h = diffuse->GetHeight();
		/*create height map from L of HSL color space*/
		SharedPtr<Image> heightMap = MakeShared<Image>(ctx);
		if(heightMap->SetSize(w, h, 1) == false)
		{
			URHO3D_LOGERROR("CalculateBumpMap: Image::SetSize fail");
			return nullptr;
		}

		for(int x=0; x<w; ++x)
		{
			for(int y=0; y<h; ++y)
			{
				Color c = diffuse->GetPixel(x, y);
				Vector3 hsl = c.ToHSL();
				heightMap->SetPixel(x, y, Color(hsl.z_, hsl.z_, hsl.z_, hsl.z_));
			}
		}

		Image * ret = new Image(ctx);
		if(ret->SetSize(w, h, 3) == false)
		{
			URHO3D_LOGERROR("CalculateBumpMap: Image::SetSize fail");
			delete ret;
			return nullptr;
		}

		for(int x=0; x<w; ++x)
		{
			for(int y=0; y<h; ++y)
			{
				float s01 = sampleHeightMap(x-1, y, heightMap);
				float s21 = sampleHeightMap(x+1, y, heightMap);
				float s10 = sampleHeightMap(x, y-1, heightMap);
				float s12 = sampleHeightMap(x, y+1, heightMap);
				Vector3 va(2.0f, 0.0f, s21-s01);
				Vector3 vb(0.0f, 2.0f, s12-s10);
				Vector3 bump(va.CrossProduct(vb));
				ret->SetPixel(x, y, Color(bump.x_, bump.y_, bump.z_));
			}
		}

		return ret;
	}

	static Color averageColors(const PODVector<Color> &colors)
	{
		Vector4 sum;

		for(unsigned ii=0; ii<colors.Size(); ++ii)
		{
			Vector4 v = colors[ii].ToVector4();
			sum += v * v;
		}

		sum = sum / colors.Size();
		return Color(Sqrt(sum.x_), Sqrt(sum.y_), Sqrt(sum.z_), Sqrt(sum.w_));
	}

	static Image * CreateDiffuse(Context* ctx, unsigned size, const PODVector<Color> &colors)
	{
		rock_tex::RockTexture tex;
		for (unsigned ii = 0; ii < colors.Size(); ++ii)
		{
			rock_tex::color_t c {colors[ii].r_, colors[ii].g_ , colors[ii].b_ };
			tex.palette.push_back(c);
		}
		tex.generate(size);
		Image * ret = new Image(ctx);
		if (ret->SetSize(size, size, 3) == false)
		{
			URHO3D_LOGERROR("CreateDiffuse: Image::SetSize fail");
			delete ret;
			return nullptr;
		}

		for (unsigned x = 0; x < size; ++x)
		{
			for (unsigned y = 0; y < size; ++y)
			{
				ret->SetPixel(x, y, Color(tex.data_[x][y].r, tex.data_[x][y].g, tex.data_[x][y].b));
			}
		}
		return ret;
	}

	void CreateAsteroidBlob(Context* ctx, Node * node, unsigned textureSize, const PODVector<Color> &palette, unsigned subdivision)
	{
		StaticModelGroup * s = node->CreateComponent<StaticModelGroup>();
		Model * model = CreateMesh(ctx, subdivision);
		if (model != nullptr)
			s->SetModel(model);

		SharedPtr<Image> diffuse(CreateDiffuse(ctx, textureSize, palette));
		if (diffuse == nullptr)
			return;

		SharedPtr <Texture2D> diffTex(MakeShared<Texture2D>(ctx));
		diffTex->SetNumLevels(1);
		if (diffTex->SetSize(textureSize, textureSize, Graphics::GetRGBAFormat(), TEXTURE_DYNAMIC) == false)
		{
			URHO3D_LOGERROR(String("diffTex->SetSize fail"));
			return;
		}
		diffTex->SetData(diffuse, false);

		ResourceCache * cache = ctx->GetSubsystem<ResourceCache>();
		Material * m = new Material(ctx);
		m->SetNumTechniques(1);
		m->SetTechnique(0, cache->GetResource<Technique>("Techniques/Diff.xml"), QUALITY_MAX);
		m->SetTexture(TU_DIFFUSE, diffTex);
		s->SetMaterial(m);
	}
}
