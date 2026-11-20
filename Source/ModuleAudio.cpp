#include "Globals.h"
#include "Application.h"
#include "ModuleAudio.h"

#include "raylib.h"

#define MAX_FX_SOUNDS   64

ModuleAudio::ModuleAudio(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	fx_count = 0;
	music_enabled = true;
	music = Music{ 0 };
}

// Destructor
ModuleAudio::~ModuleAudio()
{}

// Called before render is available
bool ModuleAudio::Init()
{
	LOG("Loading Audio Mixer");
	bool ret = true;

    LOG("Loading raylib audio system");

    InitAudioDevice();

	return ret;
}

// Raylib music streams must be fed every frame
update_status ModuleAudio::Update()
{
	if (music_enabled && IsMusicReady(music))
		UpdateMusicStream(music);

	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModuleAudio::CleanUp()
{
	LOG("Freeing sound FX, closing Mixer and Audio subsystem");

    // Unload sounds
	for (unsigned int i = 0; i < fx_count; i++)
	{
		UnloadSound(fx[i]);
	}

    // Unload music
	if (IsMusicReady(music))
	{
		StopMusicStream(music);
		UnloadMusicStream(music);
	}

    CloseAudioDevice();

	return true;
}

// Play a music file
bool ModuleAudio::PlayMusic(const char* path, float fade_time)
{
	if(IsEnabled() == false)
		return false;

	bool ret = true;
	
    StopMusicStream(music);
    music = LoadMusicStream(path);
    music.looping = true;

    PlayMusicStream(music);

	LOG("Successfully playing %s", path);

	return ret;
}

// Load WAV
unsigned int ModuleAudio::LoadFx(const char* path)
{
	if(IsEnabled() == false)
		return 0;

	unsigned int ret = 0;

	Sound sound = LoadSound(path);

	if(sound.stream.buffer == NULL)
	{
		LOG("Cannot load sound: %s", path);
	}
	else
	{
        fx[fx_count++] = sound;
		ret = fx_count;
	}

	return ret;
}

// Play WAV (ids start at 1, 0 is the "not loaded" id)
bool ModuleAudio::PlayFx(unsigned int id, float volume, float pitch)
{
	if (IsEnabled() == false)
	{
		return false;
	}

	bool ret = false;

	if (id > 0 && id <= fx_count)
	{
		SetSoundVolume(fx[id - 1], volume);
		SetSoundPitch(fx[id - 1], pitch);
		PlaySound(fx[id - 1]);
		ret = true;
	}

	return ret;
}

void ModuleAudio::PlayLoopFx(unsigned int id, float volume, float pitch)
{
	if (IsEnabled() == false || id == 0 || id > fx_count)
		return;

	Sound& sound = fx[id - 1];

	SetSoundVolume(sound, volume);
	SetSoundPitch(sound, pitch);

	if (!IsSoundPlaying(sound))
		PlaySound(sound);
}

void ModuleAudio::StopFx(unsigned int id)
{
	if (id > 0 && id <= fx_count)
		StopSound(fx[id - 1]);
}

void ModuleAudio::SetMusicVolume(float volume)
{
	if (IsMusicReady(music))
		::SetMusicVolume(music, volume);
}

void ModuleAudio::ToggleMusic()
{
	music_enabled = !music_enabled;

	if (IsMusicReady(music))
	{
		if (music_enabled)
			ResumeMusicStream(music);
		else
			PauseMusicStream(music);
	}
}

bool ModuleAudio::IsMusicPlaying() const
{
	return music_enabled && IsMusicReady(music);
}
