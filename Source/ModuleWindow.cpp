#include "Globals.h"
#include "Application.h"
#include "ModuleWindow.h"

ModuleWindow::ModuleWindow(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	target_fps = DEFAULT_FPS;
}

// Destructor
ModuleWindow::~ModuleWindow()
{
}

// Called before render is available
bool ModuleWindow::Init()
{
	LOG("Init window");
	bool ret = true;

	unsigned int flags = 0;
	bool fullscreen = WIN_FULLSCREEN;
	bool borderless = WIN_BORDERLESS;
	bool resizable = WIN_RESIZABLE;
	bool vsync = VSYNC;

	width = SCREEN_WIDTH;
	height = SCREEN_HEIGHT;

	if (fullscreen == true) flags |= FLAG_FULLSCREEN_MODE;

	if (borderless == true) flags |= FLAG_WINDOW_UNDECORATED;

	if (resizable == true) flags |= FLAG_WINDOW_RESIZABLE;

    if (vsync == true) flags |= FLAG_VSYNC_HINT;

    LOG("Init raylib window");

    SetConfigFlags(flags);
	InitWindow(width, height, TITLE);
	SetTargetFPS(target_fps);

	return ret;
}

// Called each loop iteration
update_status ModuleWindow::PreUpdate()
{
    if (WindowShouldClose()) windowEvents[WINDOW_EVENT_QUIT] = true;
    if (IsWindowMinimized()) windowEvents[WINDOW_EVENT_HIDE] = true;
    //if (IsWindowRestored()) windowEvents[WINDOW_EVENT_SHOW] = true;   // Not available
    if (IsWindowResized()) windowEvents[WINDOW_EVENT_RESIZE] = true;

    return UPDATE_CONTINUE;
}

update_status ModuleWindow::Update()
{
	return UPDATE_CONTINUE;
}

update_status ModuleWindow::PostUpdate()
{
	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModuleWindow::CleanUp()
{
	LOG("Close window");

    CloseWindow();

	return true;
}

void ModuleWindow::SetTitle(const char* title)
{
	SetWindowTitle(title);
}

void ModuleWindow::SetTargetFps(int fps)
{
	target_fps = (fps < MIN_FPS) ? MIN_FPS : (fps > MAX_FPS) ? MAX_FPS : fps;
	SetTargetFPS(target_fps);
}

int ModuleWindow::GetTargetFps() const
{
	return target_fps;
}

uint ModuleWindow::GetWidth() const
{
	return width;
}

uint ModuleWindow::GetHeight() const
{
    return height;
}

bool ModuleWindow::GetWindowEvent(WindowEvent ev)
{
    return windowEvents[ev];
}