#include "tui_app.h"
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
    TuiApp app;

    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];
        if (std::filesystem::is_directory(path))
            app.browseDirectory(path);
        else
            app.loadFile(path);
    }

    app.run();
    return 0;
}
