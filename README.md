# Introduction
Modular-glsl is a handy extension to the OpenGL Shading Language, that allows for writing shaders in more modular, structured way, with functionality separated into different files, without worrying about assembling them later into your final shader. It also makes it trivial to implement hot rebuild in case of using live shader development tools like [glslViewer](https://github.com/patriciogonzalezvivo/glslViewer).

Whole library basically consists of a single class - ShaderBuilder, that is storing, parsing and assembling your modules.

# Usage

## Defining modules
You define modules by simply adding #module directive with the module name to each of your files.
```glsl
// main.frag
#module "mainfrag"		// module name

void main() {
	// your cool shader here
}
```
```glsl
// matrix.glsl
#module "matrix_math"			// module name

void some_big_fat_matrix_transformation() {
	// ...
}
```
If a given script doesn't specify its name, library is going to try to figure it out from the filename. So for example unnamed module in file "math.glsl" would result in module with the name "math".

## Dependencies
To resolve dependencies and warn you if circular or missing ones are detected, ShaderBuilder needs every module to declare modules used by it.

```glsl
#modules "uniforms"

uniform float u_time;
```

```glsl
#modules "structs"

struct Sphere {
    vec3 position;
    float radius;
}

```

```glsl
#modules "random"
#uses "unifroms"

float rand(vec2 seed) {
    return fract(sin(dot(seed.xy, vec2(12.9898,78.233))) * 43758.5453123 + u_time);
}

```

```glsl
#modules "util"
#uses "random"
#uses "structs"

vec3 random_sphere(vec3 position, vec2 seed) {
    return Sphere(pos, rand(seed));
}
```

## Adding modules
Firstly you need ShaderBuilder instance. It is highly unlikely that you ever going to need more than one.
```c++
//main.cpp
ShaderBuilder builder;
```
Secondly you need to let ShaderBuilder know where it should look for your modules by specifying include directories.
```c++
//main.cpp
	builder.add_include_dir("../src/glsl/");
	builder.add_include_dir("../res/shaders/");
// ...
```
Finally you can add your modules.
```c++
//main.cpp
	add_module("module0.glsl");
	add_module("module1.frag");
	add_module("module2.geom");
// ...

// or

	add_modules({"module0.glsl", "module1.frag", "module2.geom" /*...*/})
```

alternatively you can keep record of your modules in separate file and import it...
```
// glslmodules
module0.glsl
module1.frag
module2.geom
```

```c++
// main.cpp

import_modules_from_file("glslmodules");
```
...which is particularly useful when hot-rebuilding.

Nothing prevents you also from importing from many different files.

Keep in mind that order in which you add your modules does not matter, ShaderBuilder takes care of figuring out dependencies and correctly assembling final shader.
Also if you add module and it is not used by build root module or any of it's dependencies it's not going to be included in the final shader(s).

## Building
To build your shader just call build method, specifying a root module.
```C++
std::string vert_source = builder.build("mainvert");
std::string frag_source = builder.build("mainfrag");
```
Of course both vertex and fragment shader (or any other) can share the same modules. 

## Hot-rebuilding
> requires SHADER_BUILDER_HOT_REBUILD

ShaderBuilder includes function called hot_rebuild which takes root module name and reference to output string as parameters and monitors all modules for changes. It returns whether or not your shader any module changed. If it is the case, you should find newly rebuilt shader in string you passed as output. Here is a implementation of a simple watcher that can be used with glslViewer or any ShaderToy-like viewing tool. 
```c++
// watcher.cpp - can be found in ./examples/watcher/
#include "modular-glsl.h"
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
```

## Header
You might want to explicitly put something at the top of your shader, e.g. glsl version declaration. 
You can do that by setting it as the header which is always put at the top of built file.

```c++
//main.cpp

builder.set_header(
  "#ifndef GLSLVIEWER\n"
  "  #version 330\n"
  "#endif\n"
);
```
## Logging
> requires SHADER_BUILDER_LOGGING

Although this library is not the most talkative, it can provide some helpful messages. To enable them register log function and use your logging solution of choice to put messages on the screen.
```c++
builder.register_log_function(log);

void log(std::string msg) {
	std::cout << msg << '\n';
}

// ...somewhere later
builder.register_log_callback(log);

// or just
builder.register_log_callback(
        [](std::string msg) -> void { std::puts(msg.c_str()); } );

```

## Error Handling
Because shader building errors - like shader compilation errors - are not exceptional during the development, this library does not throw any exceptions (though I am not yet sure about exception safety, except for methods that are explicitly marked noexcept). Instead it uses std::error_code, all methods that can fail have overload with last parameter of type std::error_code, you can use this overload to obtain information about the result of a given function call. It's similar to how <filesystem> from standard library does it, but unlike in std::filesysten calls without error code parameter are not throwing but only setting internal std::error_code - last_ec - much like C program would do. Internally error codes are also forwarded to std::filesystem calls, so if there is problem on the way there, you can recieve not only error codes from ShaderBuilderErrc enum but also internal standard library ones. 
```c++
// Example usage
std::error_code ec;
builder.add_module("invalid_module.glsl", ec)
if(ec) log(ec.message());
```
(Note that not using exceptions is not motivated by fact that this library can be used for games or other graphical performance applications because it should not be. It's rather a development time tool that lets you easly and elegantly deal with shaders. Especially at this early stage of this library, once finished - shader(s) ought to be build and included in one piece in whatever you are developing, and this code should not make it's way to any release build.)
