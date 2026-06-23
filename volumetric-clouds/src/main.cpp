#include "Application.h"
#include <iostream>

int main()
{
    try {
        Application app(1280, 720, "Real-time Rendering of Weather Clouds");
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "[ERRO] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
