#pragma once

#include "Globals.h"
#include "Module.h"
#include "Track.h"

#include "raylib.h"
#include <vector>

class PhysicEntity;
class Barrel;
class Car;

class ModuleGame : public Module
{
public:
	ModuleGame(Application* app, bool start_enabled = true);
	~ModuleGame();

	bool Start();
	update_status Update();
	update_status PostUpdate();
	bool CleanUp();

	// Called by ModulePhysics before every step of the world
	void OnPhysicsStep() override;

private:
	void BuildWorld();
	void DestroyAllEntities();
	void ResetWorld();

	void UpdateTuning();
	void UpdatePlayerInput();

	void DrawTrack() const;

private:
	Track track;

	std::vector<PhysicEntity*> entities; // owned (walls, barrels, car)
	std::vector<Barrel*> barrels; // shortcuts, not owned
	Car* car;
};
