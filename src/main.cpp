#include "Core/Application.h"
#include "Screens/Splash.h"
#include "Screens/Game.h"

#include <cstdlib>

int main()
{
    Core::Application app("raylib Android", 1280, 720);

    // Debug aid: GRIDPLAY_SCREEN=game starts straight in the gameplay screen.
    const char* start = std::getenv("GRIDPLAY_SCREEN");
    if (start && start[0] == 'g')
        app.setScreen(new Screens::Game(app));
    else
        app.setScreen(new Screens::Splash(app));

    app.run();
    return 0;
}
