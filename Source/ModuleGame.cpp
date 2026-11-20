#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModuleGame.h"
#include "ModulePhysics.h"
#include "ModuleWindow.h"
#include "Entities.h"

#include <math.h>

static float Clamp(float value, float min, float max)
{
	return (value < min) ? min : (value > max) ? max : value;
}

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	car = NULL;
}

ModuleGame::~ModuleGame()
{
}

bool ModuleGame::Start()
{
	LOG("Loading game");

	// The physics calls OnPhysicsStep before every step of the world
	App->physics->SetStepListener(this);


	BuildWorld();
	ResetWorld();

	return true;
}

bool ModuleGame::CleanUp()
{
	LOG("Unloading game");

	App->physics->SetStepListener(NULL);
	DestroyAllEntities();

	return true;
}

void ModuleGame::DestroyAllEntities()
{
	for (PhysicEntity* entity : entities)
	{
		App->physics->DestroyBody(entity->body);
		delete entity;
	}

	entities.clear();
	barrels.clear();
	car = NULL;
}

// World ----------------------------------------------------------------------------------------

void ModuleGame::BuildWorld()
{
	ModulePhysics* physics = App->physics;

	// Walls. The outer one goes counter-clockwise and the inner one clockwise ON SCREEN (see TrackData.h)
	entities.push_back(new Wall(physics, TRACK_OUTER_WALL, TRACK_OUTER_POINTS * 2));
	entities.push_back(new Wall(physics, TRACK_INNER_WALL, TRACK_INNER_POINTS * 2));

	// Loose barrels of different size and weight
	for (int i = 0; i < TRACK_BARREL_COUNT; ++i)
	{
		int radius = 10 + (i % 3) * 2;
		float density = 2.0f + (i % 4);
		Barrel* barrel = new Barrel(physics, TRACK_BARRELS[i * 2], TRACK_BARRELS[i * 2 + 1], radius, density);
		barrels.push_back(barrel);
		entities.push_back(barrel);
	}

	car = new Car(physics, this);
	entities.push_back(car);
}

// Puts the car behind the start line and every barrel in its place
void ModuleGame::ResetWorld()
{
	car->Reset(track, track.GetPointCount() - 8, 0.0f);

	for (Barrel* barrel : barrels)
		barrel->Reset();
}

// Physics: forces are applied before EVERY step ------------------------------------------------

void ModuleGame::OnPhysicsStep()
{
	car->ApplyForces(App->physics, track);

	for (Barrel* barrel : barrels)
		barrel->ApplyForces(App->physics, track);
}

// Update ---------------------------------------------------------------------------------------

update_status ModuleGame::Update()
{
	UpdateTuning();
	UpdatePlayerInput();

	if (IsKeyPressed(KEY_R))
		ResetWorld();

	return UPDATE_CONTINUE;
}

void ModuleGame::UpdateTuning()
{
	Surface& asphalt = track.GetSurface(SURF_ASPHALT);

	// Friction of the asphalt (Lesson 5): static (grip) and dynamic (sliding)
	if (IsKeyPressed(KEY_Z) || IsKeyPressedRepeat(KEY_Z)) asphalt.static_friction = Clamp(asphalt.static_friction - 0.05f, 0.05f, 2.0f);
	if (IsKeyPressed(KEY_X) || IsKeyPressedRepeat(KEY_X)) asphalt.static_friction = Clamp(asphalt.static_friction + 0.05f, 0.05f, 2.0f);
	if (IsKeyPressed(KEY_C) || IsKeyPressedRepeat(KEY_C)) asphalt.dynamic_friction = Clamp(asphalt.dynamic_friction - 0.05f, 0.05f, 2.0f);
	if (IsKeyPressed(KEY_V) || IsKeyPressedRepeat(KEY_V)) asphalt.dynamic_friction = Clamp(asphalt.dynamic_friction + 0.05f, 0.05f, 2.0f);

	// Target FPS
	if (IsKeyPressed(KEY_F4)) App->window->SetTargetFps(App->window->GetTargetFps() - 15);
	if (IsKeyPressed(KEY_F5)) App->window->SetTargetFps(App->window->GetTargetFps() + 15);
}

void ModuleGame::UpdatePlayerInput()
{
	CarInput input;
	input.throttle = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) ? 1.0f : 0.0f;
	input.brake = (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) ? 1.0f : 0.0f;
	input.steer = 0.0f;
	if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.steer += 1.0f;
	if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) input.steer -= 1.0f;
	input.handbrake = IsKeyDown(KEY_SPACE);

	input.Nitro = IsKeyDown(KEY_LEFT_SHIFT);

	car->input = input;
}

// Draw (placeholder graphics: flat colors) -----------------------------------------------------

update_status ModuleGame::PostUpdate()
{
	DrawTrack();

	for (const Barrel* barrel : barrels)
		barrel->Draw();

	car->Draw();

	// Debug shapes on top of everything (F1)
	App->physics->DrawDebug();

	return UPDATE_CONTINUE;
}

void ModuleGame::DrawTrack() const
{
	DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 40, 110, 55, 255 }); // grass

	// Road: a thick line for every piece of the centerline
	int n = track.GetPointCount();
	for (int i = 0; i < n; ++i)
	{
		Vector2 a = track.GetPoint(i);
		Vector2 b = track.GetPoint(i + 1);
		DrawLineEx(a, b, (float)(TRACK_ROAD_HALF_WIDTH * 2), Color{ 70, 70, 75, 255 });
		DrawCircleV(a, (float)TRACK_ROAD_HALF_WIDTH, Color{ 70, 70, 75, 255 });
	}

	// Walls
	for (int i = 0; i < TRACK_OUTER_POINTS; ++i)
	{
		int j = (i + 1) % TRACK_OUTER_POINTS;
		DrawLine(TRACK_OUTER_WALL[i * 2], TRACK_OUTER_WALL[i * 2 + 1], TRACK_OUTER_WALL[j * 2], TRACK_OUTER_WALL[j * 2 + 1], WHITE);
	}
	for (int i = 0; i < TRACK_INNER_POINTS; ++i)
	{
		int j = (i + 1) % TRACK_INNER_POINTS;
		DrawLine(TRACK_INNER_WALL[i * 2], TRACK_INNER_WALL[i * 2 + 1], TRACK_INNER_WALL[j * 2], TRACK_INNER_WALL[j * 2 + 1], WHITE);
	}

	// Start line
	Vector2 p = track.GetPoint(0);
	Vector2 l = track.GetNormalLeft(0);
	DrawLineEx(Vector2{ p.x + l.x * TRACK_ROAD_HALF_WIDTH, p.y + l.y * TRACK_ROAD_HALF_WIDTH },
			   Vector2{ p.x - l.x * TRACK_ROAD_HALF_WIDTH, p.y - l.y * TRACK_ROAD_HALF_WIDTH }, 4.0f, WHITE);
}
