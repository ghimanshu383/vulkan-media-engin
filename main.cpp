#include <iostream>
#include "FrameGenerator.h"
#include "RenderWindow.h"

int main() {
    fd::RenderWindow *window = new fd::RenderWindow();
    window->render();
    delete window;
    return 0;
}
