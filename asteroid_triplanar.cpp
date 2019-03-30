#include <vector>
#include "uv_mapper.hpp"
#include "FastNoise.h"
#include "asteroid_triplanar.h"
#include <Urho3D/Urho3DAll.h>

//uncomment this if u want to subdivide more
//#define DETAIL_ASTEROID_MODEL		1
namespace Urho3D
{
	struct asteroid_triplanar_vertex
	{
		Vector3 position;
		Vector3 normal;
	};

	#ifdef DETAIL_ASTEROID_MODEL
	typedef unsigned IBtype;
	#else
	typedef unsigned short IBtype;
	#endif

	static BoundingBox calculateBB(const PODVector<asteroid_triplanar_vertex> &vd)
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

	static IBtype CreateCubeTopBottomPlane(PODVector<asteroid_triplanar_vertex> &vd, PODVector<IBtype> &id, const Vector3 &Size, const IntVector3 &Segment, 
		const bool bottom)
	{
		const unsigned vdStart = vd.Size();
		const Vector3 half(Size / 2.0f);

		for (unsigned xx = 0; xx < Segment.x_ + 1; ++xx)
		{
			for (unsigned zz = 0; zz < Segment.z_ + 1; ++zz)
			{
				asteroid_triplanar_vertex data;
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
	
	static IBtype CreateMiddleXZVertices(PODVector<asteroid_triplanar_vertex> &vd, const Vector3 &Size, const IntVector3 &Segment, float y)
	{
		const unsigned vdStart = vd.Size();
		const Vector3 half(Size / 2.0f);
		for(unsigned zz = 0; zz < Segment.z_ + 1; ++zz)
		{
			asteroid_triplanar_vertex data;
			data.position = Vector3(-half.x_, y, -half.z_ + zz * (Size.z_ / Segment.z_));
			vd.Push(data);
		}
		for(unsigned xx = 1; xx < Segment.x_; ++xx)
		{
			asteroid_triplanar_vertex data;
			data.position = Vector3(-half.x_ + xx * (Size.x_ / Segment.x_), y, half.z_);
			vd.Push(data);
		}
		for(unsigned zz = 0; zz < Segment.z_ + 1; ++zz)
		{
			asteroid_triplanar_vertex data;
			data.position = Vector3(half.x_, y, half.z_ - zz * (Size.z_ / Segment.z_));
			vd.Push(data);
		}
		for(unsigned xx = 1; xx < Segment.x_; ++xx)
		{
			asteroid_triplanar_vertex data;
			data.position = Vector3(half.x_ - xx * (Size.x_ / Segment.x_), y, -half.z_);
			vd.Push(data);
		}
		return vdStart;
	}

	/*create cube without duplicated vertices*/
	static void CreateCube(PODVector<asteroid_triplanar_vertex> &vd, PODVector<IBtype> &id, const Vector3 &Size, const IntVector3 &Segment)
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
	static void CreateSphere(PODVector<asteroid_triplanar_vertex> &vd, PODVector<IBtype> &id, const float radius, const unsigned parallels_count, const unsigned meridians_count)
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
			asteroid_triplanar_vertex north;
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
				asteroid_triplanar_vertex point;
				point.position = Vector3(x, y, z);
				vd.Push(point);
			}
		}
		{
			asteroid_triplanar_vertex south;
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

	static void cutByPlane(PODVector<asteroid_triplanar_vertex> &vd, const Plane &p)
	{
		for (unsigned ii = 0; ii < vd.Size(); ++ii)
		{
			if (p.Distance(vd[ii].position) < 0.0f)
			{
				vd[ii].position = p.Project(vd[ii].position);
			}
		}
	}

	static Vector3 calculateCenter(const PODVector<asteroid_triplanar_vertex> &vd)
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

	static void calculateNormal(PODVector<asteroid_triplanar_vertex> &vd, const PODVector<IBtype> &id)
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

	static Model * CreateMesh(Context* ctx, unsigned edge_division)
	{
		bool sphereBase = false;
		if (Random(1.0f) < 0.5f)
			sphereBase = true;
		PODVector<asteroid_triplanar_vertex> vd;
		PODVector<IBtype> id;
		BoundingBox BB;
		const IntVector3 segment(edge_division, edge_division, edge_division);
		
		if(sphereBase)
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
		Vector3 center = calculateCenter(vd);

		/*displace with noise*/
		{
			PODVector<float> displacements;
			displacements.Reserve(vd.Size());
			FastNoise *perlin = new FastNoise(Random(0, M_MAX_UNSIGNED));
			perlin->SetFrequency(Random(0.01f, 0.03f));
			const float noiseScale = Random(100.0f, 200.0f);
			const float noiseFactor = Random(0.1f, 0.3f);
			float noiseMax = FLT_MIN, noiseMin = FLT_MAX;
			for (unsigned ii = 0; ii < vd.Size(); ++ii)
			{
				Vector3 p(vd[ii].position * noiseScale);
				float noise = perlin->GetPerlinFractal(p.x_, p.y_, p.z_);
				if (noise > noiseMax)
					noiseMax = noise;
				if (noise < noiseMin)
					noiseMin = noise;
				displacements.Push(noise);
			}
			delete perlin;
			//normalize to [-0.5, 0.5]
			for (unsigned ii = 0; ii < displacements.Size(); ++ii)
			{
				displacements[ii] = (displacements[ii] - noiseMin) / (noiseMax - noiseMin) - 0.5f;
			}
			for (unsigned ii = 0; ii < vd.Size(); ++ii)
			{
				//vd[ii].position = vd[ii].position + displacements[ii] * noiseFactor * (center - vd[ii].position).Normalized();
				vd[ii].position = vd[ii].position + displacements[ii] * noiseFactor * vd[ii].normal;
			}
		}

		BB = calculateBB(vd);
		calculateNormal(vd, id);
		center = calculateCenter(vd);

		VertexBuffer * vb(new VertexBuffer(ctx));
		IndexBuffer * ib(new IndexBuffer(ctx));
		Geometry * geom(new Geometry(ctx));
		vb->SetShadowed(true);
		PODVector<VertexElement> elements;
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
		vb->SetSize(vd.Size(), elements);
		vb->SetData(vd.Buffer());

		ib->SetShadowed(true);
		#ifdef DETAIL_ASTEROID_MODEL
		bool largeIndices = true;
		#else
		bool largeIndices = false;
		#endif
		ib->SetSize(id.Size(), largeIndices);
		ib->SetData(id.Buffer());

		geom->SetVertexBuffer(0, vb);
		geom->SetIndexBuffer(ib);
		geom->SetDrawRange(TRIANGLE_LIST, 0, id.Size());
		
		Model * fromScratchModel(new Model(ctx));
		fromScratchModel->SetNumGeometries(1);
		fromScratchModel->SetGeometry(0, 0, geom);
		fromScratchModel->SetBoundingBox(BB);

		return fromScratchModel;
	}

	// github.com/cpetry/NormalMap-Online
	static Image * CalculateNormalMapFromHeight(Context* ctx, const Image * heightMap)
	{
		const int w = heightMap->GetWidth();
		const int h = heightMap->GetHeight();

		Image * ret = new Image(ctx);
		if (ret->SetSize(w, h, 4) == false)
		{
			URHO3D_LOGERROR("CalculateNormalMapFromHeight: Image::SetSize fail");
			delete ret;
			return nullptr;
		}

		const float Strength = 2.5f;
		const float Level = 7.0f;

		const int type = 0;
		//webgl setting default is [1,1,-1]
		const float invertR = 1.0f;		//-1 or 1
		const float invertG = 1.0f;		//-1 or 1
		const float invertH = -1.0f;		//-1 or 1
		Vector2 step(-1.0f / w, -1.0f / h);
		float dz = (1.0f / Strength) * (1.0f + Pow(2.0f, Level));
		for (int x = 0; x < w; ++x)
		{
			for (int y = 0; y < h; ++y)
			{
				Vector2 vUv((float)x/w, (float)y/h);
				Vector2 tlv = Vector2(vUv.x_ - step.x_, vUv.y_ + step.y_);
				Vector2 lv = Vector2(vUv.x_ - step.x_, vUv.y_);
				Vector2 blv = Vector2(vUv.x_ - step.x_, vUv.y_ - step.y_);
				Vector2 tv = Vector2(vUv.x_, vUv.y_ + step.y_);
				Vector2 bv = Vector2(vUv.x_, vUv.y_ - step.y_);
				Vector2 trv = Vector2(vUv.x_ + step.x_, vUv.y_ + step.y_);
				Vector2 rv = Vector2(vUv.x_ + step.x_, vUv.y_);
				Vector2 brv = Vector2(vUv.x_ + step.x_, vUv.y_ - step.y_);
				tlv = Vector2(tlv.x_ >= 0.0 ? tlv.x_ : (1.0f + tlv.x_), tlv.y_ >= 0.0f ? tlv.y_ : (1.0f + tlv.y_));
				tlv = Vector2(tlv.x_ < 1.0f ? tlv.x_ : (tlv.x_ - 1.0f), tlv.y_ < 1.0f ? tlv.y_ : (tlv.y_ - 1.0f));
				lv = Vector2(lv.x_ >= 0.0f ? lv.x_ : (1.0f + lv.x_), lv.y_ >= 0.0f ? lv.y_ : (1.0f + lv.y_));
				lv = Vector2(lv.x_ < 1.0f ? lv.x_ : (lv.x_ - 1.0f), lv.y_ < 1.0f ? lv.y_ : (lv.y_ - 1.0f));
				blv = Vector2(blv.x_ >= 0.0f ? blv.x_ : (1.0f + blv.x_), blv.y_ >= 0.0f ? blv.y_ : (1.0f + blv.y_));
				blv = Vector2(blv.x_ < 1.0f ? blv.x_ : (blv.x_ - 1.0f), blv.y_ < 1.0f ? blv.y_ : (blv.y_ - 1.0f));
				tv = Vector2(tv.x_ >= 0.0f ? tv.x_ : (1.0f + tv.x_), tv.y_ >= 0.0f ? tv.y_ : (1.0f + tv.y_));
				tv = Vector2(tv.x_ < 1.0f ? tv.x_ : (tv.x_ - 1.0f), tv.y_ < 1.0f ? tv.y_ : (tv.y_ - 1.0f));
				bv = Vector2(bv.x_ >= 0.0f ? bv.x_ : (1.0f + bv.x_), bv.y_ >= 0.0f ? bv.y_ : (1.0f + bv.y_));
				bv = Vector2(bv.x_ < 1.0f ? bv.x_ : (bv.x_ - 1.0f), bv.y_ < 1.0f ? bv.y_ : (bv.y_ - 1.0f));
				trv = Vector2(trv.x_ >= 0.0f ? trv.x_ : (1.0f + trv.x_), trv.y_ >= 0.0f ? trv.y_ : (1.0f + trv.y_));
				trv = Vector2(trv.x_ < 1.0f ? trv.x_ : (trv.x_ - 1.0f), trv.y_ < 1.0f ? trv.y_ : (trv.y_ - 1.0f));
				rv = Vector2(rv.x_ >= 0.0f ? rv.x_ : (1.0f + rv.x_), rv.y_ >= 0.0f ? rv.y_ : (1.0f + rv.y_));
				rv = Vector2(rv.x_ < 1.0f ? rv.x_ : (rv.x_ - 1.0f), rv.y_ < 1.0f ? rv.y_ : (rv.y_ - 1.0f));
				brv = Vector2(brv.x_ >= 0.0f ? brv.x_ : (1.0f + brv.x_), brv.y_ >= 0.0f ? brv.y_ : (1.0f + brv.y_));
				brv = Vector2(brv.x_ < 1.0f ? brv.x_ : (brv.x_ - 1.0f), brv.y_ < 1.0f ? brv.y_ : (brv.y_ - 1.0f));

				float tl = Abs(heightMap->GetPixelBilinear(tlv.x_, tlv.y_).r_);
				float l = Abs(heightMap->GetPixelBilinear(lv.x_, lv.y_).r_);
				float bl = Abs(heightMap->GetPixelBilinear(blv.x_, blv.y_).r_);
				float t = Abs(heightMap->GetPixelBilinear(tv.x_, tv.y_).r_);
				float b = Abs(heightMap->GetPixelBilinear(bv.x_, bv.y_).r_);
				float tr = Abs(heightMap->GetPixelBilinear(trv.x_, trv.y_).r_);
				float r = Abs(heightMap->GetPixelBilinear(rv.x_, rv.y_).r_);
				float br = Abs(heightMap->GetPixelBilinear(brv.x_, brv.y_).r_);
				
				float dx = 0.0f, dy = 0.0f;
				if (type == 0) {	// Sobel
					dx = tl + l * 2.0 + bl - tr - r * 2.0 - br;
					dy = tl + t * 2.0 + tr - bl - b * 2.0 - br;
				}
				else {				// Scharr
					dx = tl * 3.0 + l * 10.0 + bl * 3.0 - tr * 3.0 - r * 10.0 - br * 3.0;
					dy = tl * 3.0 + t * 10.0 + tr * 3.0 - bl * 3.0 - b * 10.0 - br * 3.0;
				}
				Vector4 normal(Vector3(dx * invertR * invertH * 255.0f, dy * invertG * invertH * 255.0f, dz).Normalized(), heightMap->GetPixelBilinear(vUv.x_, vUv.y_).a_);
				Color gl_FragColor(normal.x_ * 0.5f + 0.5f, normal.y_ * 0.5f + 0.5f, normal.z_, normal.w_);
				ret->SetPixel(x, y, gl_FragColor);
			}
		}

		return ret;
	}

	static Image * CreateCraterHeightMap(Context* ctx, int size)
	{
		Image * ret = new Image(ctx);
		if (ret->SetSize(size, size, 1) == false)
		{
			URHO3D_LOGERROR("CreateCraterHeightMap: Image::SetSize fail");
			return nullptr;
		}

		for (unsigned x = 0; x < size; ++x)
		{
			for (unsigned y = 0; y < size; ++y)
			{
				ret->SetPixel(x, y, Color(0.5f, 0.5f, 0.5f, 0.5f));
			}
		}

		const unsigned numCraters = Random(5, 15);
		for (unsigned ii = 0; ii < numCraters; ++ii)
		{
			int centerX = Random(0, size - 1);
			int centerY = Random(0, size - 1);
			float radius = Random(5.0f, 30.0f);

			while (centerX < radius || centerX + radius > size || centerY < radius || centerY + radius > size)
			{
				centerX = Random(0, size - 1);
				centerY = Random(0, size - 1);
				radius = Random(5.0f, 30.0f);
			}

			for (int x = 0; x < size; ++x)
			{
				for (int y = 0; y < size; ++y)
				{
					int sqrX = (x - centerX) * (x - centerX);
					int sqrY = (y - centerY) * (y - centerY);
					const float ring = 4;
					if (sqrX + sqrY <= radius * radius)
					{
						float cosTheta = Sqrt((float)sqrX + sqrY) / radius;
						float sinTheta = Sqrt(1.0f - cosTheta * cosTheta);
						float deepness = sinTheta * 0.5f;		//radius * sinTheta * 0.5f / radius 
						ret->SetPixel(x, y, Color(0.5f - deepness, 0.5f - deepness, 0.5f - deepness, 0.5f - deepness));
					}
				}
			}
		}
		/*add shallow roughness*/
		FastNoise *cell = new FastNoise(Random(0, M_MAX_UNSIGNED));
		float **Layer = new float *[size];
		for (unsigned ii = 0; ii < size; ++ii)
			Layer[ii] = new float[size];

		float max = 0.0f, min = 0.0f;
		for (unsigned x = 0; x < size; ++x)
		{
			for (unsigned y = 0; y < size; ++y)
			{
				Layer[x][y] = cell->GetWhiteNoise(x, y);
				if (Layer[x][y] > max)
					max = Layer[x][y];
				if (Layer[x][y] < min)
					min = Layer[x][y];
			}
		}
		delete cell;
		/*normalize to [0, 1]*/
		for (unsigned x = 0; x < size; ++x)
		{
			for (unsigned y = 0; y < size; ++y)
			{
				Layer[x][y] = (Layer[x][y] - min) / (max - min);
			}
		}

		const float roughnessFactor = 0.1f;
		for (int x = 0; x < size; ++x)
		{
			for (int y = 0; y < size; ++y)
			{
				Color c = ret->GetPixel(x, y);
				float rough = (Layer[x][y] - 0.5f) * roughnessFactor;
				Color r(rough, rough, rough, rough);
				c += r;
				ret->SetPixel(x, y, c);
			}
		}

		for (unsigned ii = 0; ii < size; ++ii)
			delete[] Layer[ii];
		delete[] Layer;
		return  ret;
	}

	void CreateAsteroidBlob_triplanar(Context* ctx, Node * node, unsigned textureSize, unsigned subdivision)
	{
		StaticModelGroup * s = node->CreateComponent<StaticModelGroup>();
		Model * model = CreateMesh(ctx, subdivision);
		if (model != nullptr)
			s->SetModel(model);

		SharedPtr<Image> height(CreateCraterHeightMap(ctx, textureSize));
		if (height == nullptr)
			return;

		SharedPtr<Image> normal(CalculateNormalMapFromHeight(ctx, height));
		if (normal == nullptr)
			return;

		height->SaveBMP("height_triplanar.bmp");
		normal->SavePNG("normal_triplanar.png");

		ResourceCache * cache = ctx->GetSubsystem<ResourceCache>();
		//SharedPtr <Texture2D> diffTex(cache->GetResource<Texture2D>("Textures/1024x1024 Texel Density Texture 1.png"));
		SharedPtr <Texture2D> diffTex(cache->GetResource<Texture2D>("Textures/StoneDiffuse.dds"));
		if (diffTex == nullptr)
		{
			URHO3D_LOGERROR(String("diffTex->LoadFile fail"));
			return;
		}

#if 1
		SharedPtr <Texture2D> normalMap(MakeShared<Texture2D>(ctx));
		normalMap->SetNumLevels(1);
		if (normalMap->SetSize(textureSize, textureSize, Graphics::GetRGBAFormat(), TEXTURE_DYNAMIC) == false)
		{
			URHO3D_LOGERROR(String("normalMap->SetSize fail"));
			return;
		}
		normalMap->SetData(normal, true);
#else
		SharedPtr <Texture2D> normalMap(cache->GetResource<Texture2D>("Textures/NormalMap.png"));
		if (diffTex == nullptr)
		{
			URHO3D_LOGERROR(String("normalMap->LoadFile fail"));
			return;
		}
#endif

		Material * m = new Material(ctx);
		m->SetNumTechniques(2); 
		m->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffNormalTriplanar.xml"), QUALITY_MEDIUM);
		m->SetTechnique(1, cache->GetResource<Technique>("Techniques/DiffTriplanar.xml"), QUALITY_LOW);
		m->SetTexture(TU_DIFFUSE, diffTex);
		m->SetTexture(TU_NORMAL, normalMap);
		m->SetShaderParameter("MatSpecColor", Vector4(0.3f, 0.3f, 0.3f, 16.0f));

		s->SetMaterial(m);
	}
}
