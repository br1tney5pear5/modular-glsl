#include "ShaderBuilder.h"
#include <thread>
#include <fstream>

int main(){
    using namespace std::literals::chrono_literals;

    ShaderBuilder builder;
    builder.add_include_dir("./shaders/");

    builder.register_log_callback(
        [](std::string msg) -> void { std::puts(msg.c_str()); } );

    std::ofstream output_file;

    std::string shader;
    std::error_code ec;

    while(true) {
        std::this_thread::sleep_for(0.5s);

        builder.import_modules_from_file("glslmodules", ec);
        if(ec) std::puts( ec.message().c_str() );

        bool update = builder.hot_rebuild("main", shader, ec); 
        if(ec) std::puts( ec.message().c_str() );

        if(update) {
            output_file.open("shader.glsl", 
                    std::ios::trunc | std::ios::out);
            output_file << shader;
            output_file.close();

            std::puts("Shader updated!");
        }
    }
}

// g++ watcher.cpp -L../../lib/ -I../../include/ -lmodular-glsl -lstdc++fs -std=c++17 -pthread
