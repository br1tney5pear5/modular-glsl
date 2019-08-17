// #define GLFW_RAW_MOUSE_MOTION
// #define GLEW_STATIC
// #include <GL/glew.h>
// #include <GLFW/glfw3.h>

//GLM
//#include <glm/glm.hpp>
//#include <glm/gtc/type_ptr.hpp>



#include <chrono>
#include <thread>

#include "Shader.h"

#include "ShaderBuilder.h"

void log(std::string str) {
    std::cout << str << "\n";
}


int main(){
    ShaderBuilder builder;
    builder.add_include_dir("../shaders/");

    using namespace std::chrono_literals;

    std::string source;

    std::error_code ec;
    builder.import_modules_from_file("glslmodules", ec);

    builder.register_log_callback(log);

    builder.build("mainfrag", ec);


    while(true) {
        std::this_thread::sleep_for(1.0s);

        builder.import_modules_from_file("glslmodules", ec);

        auto flag = builder.hot_rebuild("mainfrag", source, ec);
        if(ec) {
            std::cout << ec.message();
        }

        if(flag) {
            std::cout << "change" << "\n";
            std::cout << source;
        }
    }
    return 0;
}//main


