#include "Core/Application.h"
#include "Screens/Splash.h"

int main()
{
    Core::Application app("raylib Android", 1280, 720);
    app.setScreen(new Screens::Splash(app));
    app.run();
    return 0;
}
