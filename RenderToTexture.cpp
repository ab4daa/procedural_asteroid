//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <random>
#include "FastNoise.h"
#include "nebula_blob.h"
#include "asteroid.h"
#include <Urho3D/Urho3DAll.h>
#include "RenderToTexture.h"


static unsigned int generate_random_seed()
{
	static std::random_device rd;
	static std::default_random_engine gen = std::default_random_engine(rd());
	static std::uniform_int_distribution<unsigned int> dis(0, UINT_MAX);

	return dis(gen);
}

static void addDebugArrow(Urho3D::DebugRenderer * r, const Urho3D::Vector3 &from, const Urho3D::Vector3 &to, const Urho3D::Color &color, const Urho3D::Vector3 &cameraPos)
{
	const float max_arrow_len = 3.0f;
	const float ratio = 0.25f;
	const float degree = cosf(30.0f);

	float arrow_len = max_arrow_len;
	if ((to - from).LengthSquared() * ratio * ratio < arrow_len)
		arrow_len = (to - from).Length() * ratio;

	const Urho3D::Vector3 v1(from - to);
	Urho3D::Vector3 flip_p(to + v1.Normalized() * arrow_len);
	Urho3D::Vector3 v_expand((flip_p - cameraPos).CrossProduct(v1).Normalized());

	r->AddLine(from, to, color, true);
	r->AddLine(to, flip_p + v_expand * degree * arrow_len, color, true);
	r->AddLine(to, flip_p - v_expand * degree * arrow_len, color, true);
}

static const StringHash TEXTURECUBE_SIZE("TEXTURECUBE SIZE");
static const Vector3 default_light_dir(0.5f, -1.0f, 0.5f);
static const Color default_light_color(0.2f, 0.2f, 0.2f);

URHO3D_DEFINE_APPLICATION_MAIN(RenderToTexture)

RenderToTexture::RenderToTexture(Context* context) :
    Sample(context)
{
	SetRandomSeed(generate_random_seed());
}

void RenderToTexture::Setup()
{
	Sample::Setup();
	engineParameters_[EP_LOG_NAME] = "Urho3D.log";
}

void RenderToTexture::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_RELATIVE);
}

void RenderToTexture::CreateScene()
{
    auto* cache = GetSubsystem<ResourceCache>();

    {
        // Create the scene in which we move around
        scene_ = new Scene(context_);
		scene_->CreateComponent<DebugRenderer>();

        // Create octree, use also default volume (-1000, -1000, -1000) to (1000, 1000, 1000)
        scene_->CreateComponent<Octree>();

        // Create a Zone component for ambient lighting & fog control
        Node* zoneNode = scene_->CreateChild("Zone");
        auto* zone = zoneNode->CreateComponent<Zone>();
        zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
        zone->SetAmbientColor(Color(0.1f, 0.1f, 0.1f));
        zone->SetFogStart(500.0f);
        zone->SetFogEnd(800.0f);

		scene_->CreateComponent<DebugRenderer>();
		scene_->GetComponent<DebugRenderer>()->SetLineAntiAlias(true);

        lightNode = scene_->CreateChild("DirectionalLight");
        lightNode->SetDirection(default_light_dir);
        auto* light = lightNode->CreateComponent<Light>();
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetColor(default_light_color);
        light->SetSpecularIntensity(1.0f);
		light->SetCastShadows(true);

		const Color NC(0.8f, 0.2f, 0.4f);
		Color c1(Random(1.0f), Random(1.0f), Random(1.0f));
		Color c2(Random(1.0f), Random(1.0f), Random(1.0f));
		const int TexSize = 256;
		const double frequency = 4.0;
		const double fx = TexSize / frequency;
#if 0
		siv::PerlinNoise perlin(generate_random_seed());
		SharedPtr <Texture2D> perlin2D(MakeShared<Texture2D>(context_));
		perlin2D->SetNumLevels(1);
		if (perlin2D->SetSize(TexSize, TexSize, Graphics::GetRGBAFormat(), TEXTURE_DYNAMIC) == false)
		{
			URHO3D_LOGERROR(String("perlin2D->SetSize fail"));
		}
		SharedPtr<Image> pic(MakeShared<Image>(context_));
		pic->SetSize(TexSize, TexSize, 4);
		for (int xx = 0; xx < TexSize; ++xx)
		{
			for (int yy = 0; yy < TexSize; ++yy)
			{
				double x = xx / fx;
				double y = yy / fx;
				//double p = perlin.octaveNoise0_1((double)xx / fx, (double)yy / fx, 8);
				double p = perlin.octaveNoise0_1(x + 2.0f * perlin.octaveNoise0_1(x, y, 8), y + 2.0f * perlin.octaveNoise0_1(x, y, 8), 8);
				float dist = (Vector2(xx, yy) - Vector2(TexSize / 2, TexSize / 2)).Length();
				float a = Pow(1.0f - dist / TexSize, 8.0f);
				Color c(p, p, p, a);
				pic->SetPixel(xx, yy, c);
			}
		}
		perlin2D->SetData(pic, true);
#endif

#if 1
		const unsigned degree_step = 90;
		const Quaternion q[] =
		{
			Quaternion::IDENTITY,
			Quaternion(90.0f, Vector3::RIGHT),
			Quaternion(90.0f, Vector3::FORWARD),

			Quaternion(90.0f, Vector3(1.0f, 0.0f, -1.0f)),
			Quaternion(90.0f, Vector3(1.0f, 0.0f, 1.0f)),
		};
		for(unsigned ii = 0; ii < 5; ii++)
		{
			if (ii % 2)
			{
				c1 = Color(Random(1.0f), Random(1.0f), Random(1.0f));
				c2 = Color(Random(1.0f), Random(1.0f), Random(1.0f));
			}
#if 1
			FastNoise perlin(generate_random_seed());
			perlin.SetFractalOctaves(8);
			perlin.SetFrequency(0.04f);
			float ** noise = alloc2Darr<float>(TexSize, TexSize);
			for (int xx = 0; xx < TexSize; ++xx)
			{
				for (int yy = 0; yy < TexSize; ++yy)
				{
					noise[xx][yy] = perlin.GetPerlinFractal(xx, yy);
				}
			}
			normalize2Darr<float>(noise, TexSize, TexSize);
			SharedPtr <Texture2D> perlin2D(MakeShared<Texture2D>(context_));
			perlin2D->SetNumLevels(1);
			if (perlin2D->SetSize(TexSize, TexSize, Graphics::GetRGBAFormat(), TEXTURE_DYNAMIC) == false)
			{
				URHO3D_LOGERROR(String("perlin2D->SetSize fail"));
			}
			SharedPtr<Image> pic(MakeShared<Image>(context_));
			pic->SetSize(TexSize, TexSize, 4);
			for (int xx = 0; xx < TexSize; ++xx)
			{
				for (int yy = 0; yy < TexSize; ++yy)
				{
					float dist = (Vector2(xx, yy) - Vector2(TexSize / 2, TexSize / 2)).Length();
					Vector2 n(Vector2(xx,yy).Normalized() * 1000.0f);
					float a = Pow(1.0f - dist / TexSize, 6.0f);
#if 1
					Color c(NC);
					c.a_ = Pow(noise[xx][yy], 4.0f) * a;
#else
					Color c = c1.Lerp(c2, noise[xx][yy]);
					c.a_ = a;
#endif
					pic->SetPixel(xx, yy, c);
				}
			}
			perlin2D->SetData(pic, true);
			release2Darr<float>(noise, TexSize, TexSize);
#endif
			Node * boxNode = scene_->CreateChild("Box");
			boxNode->SetPosition(Vector3(20.5f, 40.0f, 20.5f) /*+ Vector3(Random(-1.0f, 1.0f), Random(-1.0f, 1.0f), Random(-1.0f, 1.0f))*/);
			boxNode->SetScale(Vector3(30.0f, 30.0f, 30.f));
			boxNode->SetRotation(q[ii]);
			auto* boxObject = boxNode->CreateComponent<StaticModel>();
			boxObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
			//auto  mm = cache->GetResource<Material>("Materials/Stone.xml")->Clone();
			SharedPtr<Material> mm = MakeShared<Material>(context_);
			mm->SetNumTechniques(1);
			mm->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffAlphaNebula.xml"), QUALITY_MAX);
			mm->SetTexture(TU_DIFFUSE, perlin2D);
			mm->SetCullMode(CULL_NONE);
			boxObject->SetMaterial(mm);
			//boxObject->SetMaterial(cache->GetResource<Material>("Materials/Stone.xml"));
			//boxObject->GetMaterial()->SetCullMode(CULL_NONE);
			boxObject->SetCastShadows(false);
			planes_.Push(SharedPtr<Node>(boxNode));
		}
#endif
#if 1
		Node * nebulas = scene_->CreateChild("nebulaes");
		Vector<Color> colors;
		for (unsigned ii = 0; ii < 2; ii++)
			colors.Push(Color(Random(1.0f), Random(1.0f), Random(1.0f)));
		CreateNebulaBlob(context_, nebulas, colors, TexSize);
		StaticModelGroup * s = nebulas->GetComponent<StaticModelGroup>();
		Node * nebula = scene_->CreateChild("nebula");
		nebula->SetPosition(Vector3(-20.5f, 40.0f, -20.5f));
		nebula->SetScale(Vector3(30.0f, 30.0f, 30.f));
		s->AddInstanceNode(nebula);
#endif
#if 1
		Node * ast = scene_->CreateChild("asteroids");
		CreateAsteroidBlob(context_, ast, "Models/Sphere.mdl");
		StaticModelGroup * smg = ast->GetComponent<StaticModelGroup>();
		Node * ast1 = scene_->CreateChild("asteroid");
		ast1->SetPosition(Vector3(-20.5f, 40.0f, 20.5f));
		ast1->SetScale(20.0f);
		smg->AddInstanceNode(ast1);
#endif
#if 0
		SharedPtr <Texture3D> perlin3D(MakeShared<Texture3D>(context_));
		perlin3D->SetNumLevels(1);
		if (perlin3D->SetSize(TexSize, TexSize, TexSize, Graphics::GetRGBAFormat(), TEXTURE_DYNAMIC) == false)
		{
			URHO3D_LOGERROR(String("perlin3D->SetSize fail"));
		}
		SharedPtr<Image> pic3D(MakeShared<Image>(context_));
		pic3D->SetSize(TexSize, TexSize, TexSize, 3);
		for (int xx = 0; xx < TexSize; ++xx)
		{
			for (int yy = 0; yy < TexSize; ++yy)
			{
				for (int zz = 0; zz < TexSize; ++zz)
				{
					float p = perlin.octaveNoise0_1((double)xx / fx, (double)yy / fx, (double)zz/fx, 8);
					Color c(p, p, p);
					pic3D->SetPixel(xx, yy, zz, c);
				}
			}
		}
		perlin3D->SetData(pic3D, false);

		Node * nebulaNode = scene_->CreateChild("Nebula");
		nebulaNode->SetPosition(Vector3(-40.5f, 60.0f, -40.5f));
		nebulaNode->SetScale(Vector3(50.0f, 50.0f, 50.f));
		auto* nebulaObject = nebulaNode->CreateComponent<StaticModel>();
		nebulaObject->SetModel(cache->GetResource<Model>("Models/Sphere.mdl"));
		auto* m = cache->GetResource<Material>("Materials/nebular.xml");
		m->SetShaderParameter("NebularColor", Vector3(Random(1.0f), Random(1.0f), Random(1.0f)));
		m->SetShaderParameter("NebularOffset", Vector3(Random(1.0f) * 2000 - 1000, Random(1.0f) * 2000 - 1000, Random(1.0f) * 2000 - 1000));
		m->SetShaderParameter("NebularScale", Random(1.0f) * 0.5f + 0.25f);
		m->SetShaderParameter("NebularIntensity", Random(1.0f) * 0.2f + 0.9f);
		m->SetShaderParameter("NebularFalloff", Random(1.0f) * 3 + 3);
		//m->SetTexture(TU_VOLUMEMAP, perlin3D);
		nebulaObject->SetMaterial(m);
#endif

        // Create a "floor" consisting of several tiles
        for (int y = -5; y <= 5; ++y)
        {
            for (int x = -5; x <= 5; ++x)
            {
                Node* floorNode = scene_->CreateChild("FloorTile");
                floorNode->SetPosition(Vector3(x * 20.5f, -0.5f, y * 20.5f));
                floorNode->SetScale(Vector3(20.0f, 1.0f, 20.f));
                auto* floorObject = floorNode->CreateComponent<StaticModel>();
                floorObject->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
                floorObject->SetMaterial(cache->GetResource<Material>("Materials/Stone.xml"));
				floorObject->SetCastShadows(true);
            }
        }

        // Create the camera which we will move around. Limit far clip distance to match the fog
        cameraNode_ = scene_->CreateChild("Camera");
        auto* camera = cameraNode_->CreateComponent<Camera>();
        camera->SetFarClip(800.0f);

        // Set an initial position for the camera scene node above the plane
        cameraNode_->SetPosition(Vector3(0.0f, 7.0f, -30.0f));
    }
}

void RenderToTexture::CreateInstructions()
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* ui = GetSubsystem<UI>();
	ui->GetRoot()->SetDefaultStyle(cache->GetResource<XMLFile>("UI/DefaultStyle.xml"));

    // Construct new Text object, set string to display and font to use
    auto* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use WASD keys to move/ Press Space to toggle free mouse");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void RenderToTexture::SetupViewport()
{
    auto* renderer = GetSubsystem<Renderer>();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void RenderToTexture::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    auto* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 30.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

	cameraNode_->Translate(Vector3::FORWARD * input->GetMouseMoveWheel() * MOVE_SPEED);

	if (mouseFree == false || (mouseFree && input->GetMouseButtonDown(MOUSEB_RIGHT))) {
		// Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
		IntVector2 mouseMove = input->GetMouseMove();
		yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
		pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
		pitch_ = Clamp(pitch_, -90.0f, 90.0f);

		// Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
		cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
	}

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void RenderToTexture::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(RenderToTexture, HandleUpdate));

	// Subscribe HandlePostRenderUpdate() function for processing the post-render update event, during which we request debug geometry
	SubscribeToEvent(E_POSTRENDERUPDATE, URHO3D_HANDLER(RenderToTexture, HandlePostRenderUpdate));
}

void RenderToTexture::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);

	auto* input = GetSubsystem<Input>();
	if (input->GetKeyPress(Key::KEY_SPACE))
	{
		mouseFree = !mouseFree;
		if (mouseFree)
			Sample::InitMouseMode(MM_FREE);
		else
			Sample::InitMouseMode(MM_RELATIVE);
	}
}

void RenderToTexture::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
	DebugRenderer * debug = scene_->GetComponent<DebugRenderer>();
	for (unsigned i = 0; i < planes_.Size(); ++i)
	{
		StaticModel * s = planes_[i]->GetComponent<StaticModel>();
		s->DrawDebugGeometry(debug, true);
	}
}
