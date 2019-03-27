#include "uv_mapper.hpp"
#include "FastNoise.h"
#include "asteroid.h"
#include <Urho3D/Urho3DAll.h>

namespace Urho3D
{
	struct asteroid_vertex_data_
	{
		Vector3 position;
		Vector3 normal;
		Vector4 tangent;
		Vector2 uv;
	};

	static BoundingBox calculateBB(const PODVector<asteroid_vertex_data_> &vd)
	{
		BoundingBox ret;
		for (unsigned ii = 0; ii < vd.Size(); ++ii)
			ret.Merge(vd[ii].position);

		return ret;
	}

	class XZplaneIterator{
	public:
		virtual unsigned short iter(unsigned i) const = 0;
		virtual ~XZplaneIterator() = default;
	};

	class TopBottomPlane : public XZplaneIterator
	{
	public:
		TopBottomPlane(unsigned short start_offset, const IntVector3 &Segment)
			: XZplaneIterator(), off(start_offset), seg(Segment)
		{}
		unsigned short iter(unsigned i) const override
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
		const unsigned short off;
		const IntVector3 seg;
	};

	class MiddlePlane : public XZplaneIterator
	{
	public:
		MiddlePlane(unsigned short start_offset, const IntVector3 &Segment)
			: XZplaneIterator(), off(start_offset), seg(Segment)
		{}
		unsigned short iter(unsigned i) const override
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
		const unsigned short off;
		const IntVector3 seg;
	};

	/*
	quad:
	i0			i1

	i2			i3
	*/
	static void buildQuadIndex(PODVector<unsigned short> &id, unsigned short i0, unsigned short i1, unsigned short i2, unsigned short i3, const bool bottom)
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

	static unsigned short CreateCubeTopBottomPlane(PODVector<asteroid_vertex_data_> &vd, PODVector<unsigned short> &id, const Vector3 &Size, const IntVector3 &Segment, 
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
				unsigned short i0 = vdStart + xx * (Segment.z_ + 1) + 1 + zz;
				unsigned short i1 = vdStart + (xx + 1) * (Segment.z_ + 1) + 1 + zz;
				unsigned short i2 = vdStart + xx * (Segment.z_ + 1) + zz;
				unsigned short i3 = vdStart + (xx + 1) * (Segment.z_ + 1) + zz;
				buildQuadIndex(id, i0, i1, i2, i3, bottom);
			}
		}

		return vdStart;
	}
	
	static unsigned short CreateMiddleXZVertices(PODVector<asteroid_vertex_data_> &vd, const Vector3 &Size, const IntVector3 &Segment, float y)
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
	static void CreateCube(PODVector<asteroid_vertex_data_> &vd, PODVector<unsigned short> &id, const Vector3 &Size, const IntVector3 &Segment)
	{
		if (Segment.x_ <= 0 || Segment.y_ <= 0 || Segment.z_ <= 0 || Size.x_ <= 0.0f || Size.y_ <= 0.0f || Size.z_ <= 0.0f)
		{
			URHO3D_LOGERROR("CreateCube: size or segment cannot <= 0");
			return;
		}

		const unsigned numVertices = (Segment.x_ + 1) * (Segment.y_ + 1) * 2 + (Segment.y_ + 1)*(Segment.z_ + 1) * 2 + (Segment.x_ + 1)*(Segment.z_ + 1) * 2
			- 4 * (Segment.x_ - 1) - 4 * (Segment.y_ - 1) - 4 * (Segment.z_ - 1) - 8 * 2;
		const unsigned numIndices = Segment.x_*Segment.y_ * 2 * 3 * 2 + Segment.y_*Segment.z_ * 2 * 3 * 2 + Segment.x_*Segment.z_ * 2 * 3 * 2;

		vd.Clear();
		vd.Reserve(numVertices);
		id.Clear();
		id.Reserve(numIndices);

		/*bottom xz plane*/
		unsigned short bottomOff = CreateCubeTopBottomPlane(vd, id, Size, Segment, true);
		XZplaneIterator * lastPlane = new TopBottomPlane(bottomOff, Segment);
		XZplaneIterator * newPlane = nullptr;
		for(unsigned ii=0; ii<Segment.y_-1; ++ii)
		{
			unsigned short middleOff = CreateMiddleXZVertices(vd,Size, Segment, -Size.y_/2.0f + (ii+1)*(Size.y_/Segment.y_));
			newPlane = new MiddlePlane(middleOff, Segment);
			
			for(unsigned jj=0; jj<Segment.x_*2 + Segment.z_*2; ++jj)
			{
				buildQuadIndex(id, newPlane->iter(jj+1), newPlane->iter(jj), lastPlane->iter(jj+1), lastPlane->iter(jj), false);
			}
			delete lastPlane;
			lastPlane = newPlane;
			newPlane = nullptr;
		}
		unsigned short topOff = CreateCubeTopBottomPlane(vd, id, Size, Segment, false);
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
	static void CreateSphere(PODVector<asteroid_vertex_data_> &vd, PODVector<unsigned short> &id, const float radius, const unsigned parallels_count, const unsigned meridians_count)
	{
		if (radius <= 0.0f || parallels_count < 1 || meridians_count < 3)
		{
			URHO3D_LOGERROR("CreateSphere: parameter error");
			return;
		}

		vd.Clear();
		id.Clear();
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
			unsigned short const a = i + 1;
			unsigned short const b = (i + 1) % meridians_count + 1;
			id.Push(0);
			id.Push(b);
			id.Push(a);
		}
		for (unsigned  j = 0; j < parallels_count - 2; ++j)
		{
			unsigned short aStart = j * meridians_count + 1;
			unsigned short bStart = (j + 1) * meridians_count + 1;
			for (unsigned i = 0; i < meridians_count; ++i)
			{
				const unsigned short a = aStart + i;
				const unsigned short a1 = aStart + (i + 1) % meridians_count;
				const unsigned short b = bStart + i;
				const unsigned short b1 = bStart + (i + 1) % meridians_count;
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
			unsigned short const a = i + meridians_count * (parallels_count - 2) + 1;
			unsigned short const b = (i + 1) % meridians_count + meridians_count * (parallels_count - 2) + 1;
			id.Push(vd.Size()-1);
			id.Push(a);
			id.Push(b);
		}		
	}

	/*split mesh to 2 parts by a plane; output 2 index buffers, they use the same vertex buffer*/
	static void SplitMesh(const PODVector<asteroid_vertex_data_> &vd, const PODVector<unsigned short> &id, const Plane &p, 
		Vector< PODVector<unsigned short> > &parts)
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

	static void calculateNormal(PODVector<asteroid_vertex_data_> &vd, const PODVector<unsigned short> &id)
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
		/*create a box from (-0.5, -0.5) ~ (0.5, 0.5)*/
		PODVector<asteroid_vertex_data_> vd;
		PODVector<unsigned short> id;
		BoundingBox BB;
		CreateCube(vd, id, Vector3::ONE, IntVector3(edge_division, edge_division, edge_division));
		//CreateSphere(vd, id, 0.5f, 10, 20);

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

		VertexBuffer * vb(new VertexBuffer(ctx));
		IndexBuffer * ib(new IndexBuffer(ctx));
		Geometry * geom(new Geometry(ctx));
		vb->SetShadowed(true);
		PODVector<VertexElement> elements;
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
		elements.Push(VertexElement(TYPE_VECTOR4, SEM_TANGENT));
		elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
		vb->SetSize(vd.Size(), elements);
		vb->SetData(vd.Buffer());

		ib->SetShadowed(true);
		ib->SetSize(id.Size(), false);
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

	void CreateAsteroidBlob(Context* ctx, Node * node, const String &srcModel)
	{
		StaticModelGroup * s = node->CreateComponent<StaticModelGroup>();
		Model * model = CreateMesh(ctx, 20);
		if (model != nullptr)
			s->SetModel(model);
	}
}
