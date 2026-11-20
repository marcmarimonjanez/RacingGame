#pragma once

#include "Module.h"

#define MAX_SOUNDS	16
#define DEFAULT_MUSIC_FADE_TIME 2.0f

class ModuleAudio : public Module
{
public:

	ModuleAudio(Application* app, bool start_enabled = true);
	~ModuleAudio();

	bool Init();
	update_status Update();
	bool CleanUp();

	// Play a music file
	bool PlayMusic(const char* path, float fade_time = DEFAULT_MUSIC_FADE_TIME);

	// Load a sound in memory
	unsigned int LoadFx(const char* path);

	// Play a previously loaded sound
	// fx = id returned by LoadFx (0 means "not loaded", it is ignored)
	bool PlayFx(unsigned int fx, float volume = 1.0f, float pitch = 1.0f);

	// Looping sound (engine, skid): call it every frame, it restarts the sound when it ends and updates volume and pitch
	void PlayLoopFx(unsigned int fx, float volume, float pitch);
	void StopFx(unsigned int fx);

	void SetMusicVolume(float volume);
	void ToggleMusic();
	bool IsMusicPlaying() const;

private:

	bool music_enabled;
	Music music;
	Sound fx[MAX_SOUNDS];
    unsigned int fx_count;
};
