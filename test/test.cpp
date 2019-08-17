#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <fstream>

#include "modular-glsl.h"

TEST_CASE( "Include directories manipulation" ) {
    ShaderBuilder builder;
    std::error_code ec;

    std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
    first_file << "#module \"first\"\n";
    first_file.close();

    std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
    second_file << "#module \"second\"\n";
    second_file.close();

    std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
    third_file << "#module \"third\"\n";
    third_file.close();

    REQUIRE(builder.get_include_dirs_count() == 0);
    REQUIRE(builder.get_modules_count() == 0);

    SECTION("Adding include directory"){
        builder.add_include_dir("../test/res/");

        REQUIRE(builder.get_include_dirs_count() == 1);
    }

    SECTION("Adding the same include directory results in only one entry"){
        builder.add_include_dir("../test/res/");
        builder.add_include_dir("../test/res/", ec);

        REQUIRE(builder.get_include_dirs_count() == 1);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));
    }

    SECTION("Non-existent include directories are not added") {
        builder.add_include_dir("../acsal/akjsnd/");
        builder.add_include_dir("../this/directory/doesnt/exist/i/hope", ec);

        REQUIRE(builder.get_include_dirs_count() == 0);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::include_dir_does_not_exist));
    }

    SECTION("Files cannot be added as directories") {
        builder.add_include_dir("../test/res/first.glsl");
        builder.add_include_dir("../test/res/second.glsl");
        builder.add_include_dir("../test/res/third.glsl", ec);

        REQUIRE(builder.get_include_dirs_count() == 0);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::include_dir_is_not_a_directory));
    }

    SECTION("Adding include directories lets you add modules from files in it"){
        builder.add_include_dir("../test/res/", ec);

        REQUIRE(builder.get_include_dirs_count() == 1);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));

        builder.add_module("first.glsl");

        REQUIRE(builder.get_modules_count() == 1);
    }

}


TEST_CASE( "Modules manipulation" ) {
    ShaderBuilder builder;
    std::error_code ec;
    builder.add_include_dir("../test/res/");

    std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
    first_file << "#module \"first\"\n";
    first_file.close();

    std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
    second_file << "#module \"second\"\n";
    second_file.close();

    std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
    third_file << "#module \"third\"\n";
    third_file.close();

    REQUIRE(builder.get_modules_count() == 0);

    SECTION("Adding modules one at the time"){
        builder.add_module("first.glsl");
        builder.add_module("second.glsl");
        builder.add_module("third.glsl");

        REQUIRE(builder.has_module("first"));
        REQUIRE(builder.has_module("second"));
        REQUIRE(builder.has_module("third"));

        REQUIRE(builder.get_modules_count() == 3);
    }

    SECTION("Adding multiple modules one at the time"){
        builder.add_modules({"first.glsl", "second.glsl", "third.glsl"});

        REQUIRE(builder.get_modules_count() == 3);
    }

    SECTION("Non-existent module files are not imported, adding one at the time") {
        builder.add_module("ajdsawj.glsl");
        builder.add_module("ajbksad.glsl");
        builder.add_module("wnnjnvd.glsl");

        REQUIRE(builder.get_modules_count() == 0);
    }

    SECTION("Non-existent module files are not imported, adding multiple at the time") {
        builder.add_modules({"ajdsawj.glsl", "ajbksad.glsl", "wnnjnvd.glsl"});

        REQUIRE(builder.get_modules_count() == 0);
    }

    SECTION("Importing modules from file"){
        std::fstream modules_list_file;
        modules_list_file.open("../test/res/glslmodules",
                               std::ios_base::in | std::ios_base::out | std::ios_base::trunc);

        modules_list_file <<
            "first.glsl\n"
            "second.glsl\n"
            "third.glsl\n" ;

        modules_list_file.close(); 

        builder.import_modules_from_file("glslmodules", ec);

        REQUIRE(builder.get_modules_count() == 3);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));
    }

    SECTION("Non-existent modules are not imported from file") {
        std::fstream modules_list_file;
        modules_list_file.open("../test/res/glslmodules",
                               std::ios_base::in | std::ios_base::out | std::ios_base::trunc);

        modules_list_file <<
            "first.glsl\n"
            "second.glsl\n"
            "third.glsl\n"
            "missing.glsl\n" ;

        modules_list_file.close(); 

        builder.import_modules_from_file("glslmodules", ec);

        REQUIRE(builder.get_modules_count() == 3);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::file_does_not_exist));

        REQUIRE(builder.has_module("first"));
        REQUIRE(builder.has_module("second"));
        REQUIRE(builder.has_module("third"));
    }

    SECTION("Improting from file is interrupted on missing module file") {
        std::fstream modules_list_file;
        modules_list_file.open("../test/res/glslmodules",
                               std::ios_base::in | std::ios_base::out | std::ios_base::trunc);

        modules_list_file <<
            "first.glsl\n"
            "second.glsl\n"
            "missing.glsl\n"
            "third.glsl\n";

        modules_list_file.close(); 

        builder.import_modules_from_file("glslmodules", ec);

        REQUIRE(builder.get_modules_count() == 2);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::file_does_not_exist));

        REQUIRE(builder.has_module("first"));
        REQUIRE(builder.has_module("second"));
    }

    SECTION("Ill-formed modules are not parsed") {
        REQUIRE(builder.get_modules_count() == 0);
        std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
        first_file << "#eludom \"first\"\n"; // bad keyword
        first_file.close();

        std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
        third_file << "#module \"\"\n";  // no module name
        third_file.close();

        builder.add_module("first.glsl", ec);
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::syntax_error));

        REQUIRE(!builder.has_module("first"));

        builder.add_module("third.glsl");
        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::syntax_error));
        REQUIRE(!builder.has_module("third"));

        REQUIRE(builder.get_modules_count() == 0);
    }

}

TEST_CASE( "Building shader" ) {
    SECTION("Building good shader with dependencies") {
        ShaderBuilder builder;
        std::error_code ec;
        builder.add_include_dir("../test/res/");

        std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
        first_file <<
            "#module \"first\"\n"
            "#uses \"second\"\n"
            "#uses \"third\"\n";
        first_file.close();

        std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
        second_file <<
            "#module \"second\"\n"
            "#uses \"third\"\n";
        second_file.close();

        std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
        third_file << "#module \"third\"\n";
        third_file.close();

        builder.add_modules({"first.glsl", "second.glsl", "third.glsl"});

        REQUIRE(builder.get_modules_count() == 3);

        builder.build("first", ec);

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));

        auto& sorted_list = builder.get_sorted_modules_list();

        REQUIRE(sorted_list[2].name == "first");
        REQUIRE(sorted_list[1].name == "second");
        REQUIRE(sorted_list[0].name == "third");
    }

    SECTION("Building with circular dependency fails") {
        ShaderBuilder builder;
        std::error_code ec;
        builder.add_include_dir("../test/res/");

        std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
        first_file <<
            "#module \"first\"\n"
            "#uses \"second\"\n"
            "#uses \"third\"\n";
        first_file.close();

        std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
        second_file <<
            "#module \"second\"\n"
            "#uses \"third\"\n";
        second_file.close();

        std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
        third_file <<
            "#module \"third\"\n"
            "#uses \"first\"\n";
        third_file.close();

        builder.add_modules({"first.glsl", "second.glsl", "third.glsl"});

        REQUIRE(builder.get_modules_count() == 3);

        builder.build("first", ec);

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::circular_dependency));
    }

    SECTION("Building with circular missing fails") {
        ShaderBuilder builder;
        std::error_code ec;
        builder.add_include_dir("../test/res/");

        std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
        first_file <<
            "#module \"first\"\n"
            "#uses \"second\"\n"
            "#uses \"third\"\n";
        first_file.close();

        std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
        second_file <<
            "#module \"second\"\n"
            "#uses \"third\"\n";
        second_file.close();

        std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
        third_file <<
            "#module \"third\"\n"
            "#uses \"fourth\"\n";
        third_file.close();

        builder.add_modules({"first.glsl", "second.glsl", "third.glsl"});

        REQUIRE(builder.get_modules_count() == 3);

        builder.build("first", ec);

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::missing_dependency));
    }

    SECTION("Unused module does not get included in the output shader") {
        ShaderBuilder builder;
        std::error_code ec;
        builder.add_include_dir("../test/res/");

        std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
        first_file <<
            "#module \"first\"\n"
            "#uses \"second\"\n";
        first_file.close();

        std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
        second_file << "#module \"second\"\n";
        second_file.close();

        std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
        third_file << "#module \"third\"\n";
        third_file.close();

        builder.add_modules({"first.glsl", "second.glsl", "third.glsl"});

        REQUIRE(builder.get_modules_count() == 3);

        builder.build("first", ec);

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));

        auto& sorted_list = builder.get_sorted_modules_list();

        REQUIRE(sorted_list.size() == 2);

        REQUIRE(sorted_list[1].name == "first");
        REQUIRE(sorted_list[0].name == "second");

    }


}

#ifdef SHADER_BUILDER_HOT_REBUILD_SUPPORT

TEST_CASE( "Hot rebuild" ) {
    SECTION("Hot rebuilding good shader with dependencies") {
        ShaderBuilder builder;
        std::error_code ec;
        builder.add_include_dir("../test/res/");

        std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
        first_file <<
            "#module \"first\"\n"
            "#uses \"second\"\n"
            "#uses \"third\"\n";

        first_file.close();

        std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
        second_file <<
            "#module \"second\"\n"
            "#uses \"third\"\n";
        second_file.close();

        std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
        third_file << "#module \"third\"\n";
        third_file.close();

        builder.add_modules({"first.glsl", "second.glsl", "third.glsl"});

        REQUIRE(builder.get_modules_count() == 3);

        std::string first_shader = builder.build("first", ec);

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));

        auto& sorted_list = builder.get_sorted_modules_list();

        REQUIRE(sorted_list[2].name == "first");
        REQUIRE(sorted_list[1].name == "second");
        REQUIRE(sorted_list[0].name == "third");

        // Simulate editing third file
        
        third_file.open("../test/res/third.glsl",
                                      std::ios_base::out | std::ios_base::app);
        third_file << "void foo() {}\n";
        third_file.close();

        std::string second_shader;
        REQUIRE(builder.hot_rebuild("first", second_shader, ec));

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));

        REQUIRE(first_shader != second_shader);
    }

    SECTION("Hot rebuilding doesn't happen if dependencies stays unchanged") {
        ShaderBuilder builder;
        std::error_code ec;
        builder.add_include_dir("../test/res/");

        std::ofstream first_file("../test/res/first.glsl", std::ios_base::trunc);
        first_file <<
            "#module \"first\"\n"
            "#uses \"second\"\n"
            "#uses \"third\"\n";
        first_file.close();

        std::ofstream second_file("../test/res/second.glsl", std::ios_base::trunc);
        second_file <<
            "#module \"second\"\n"
            "#uses \"third\"\n";
        second_file.close();

        std::ofstream third_file("../test/res/third.glsl", std::ios_base::trunc);
        third_file << "#module \"third\"\n";
        third_file.close();

        builder.add_modules({"first.glsl", "second.glsl", "third.glsl"});

        REQUIRE(builder.get_modules_count() == 3);

        std::string first_shader = builder.build("first", ec);

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));

        auto& sorted_list = builder.get_sorted_modules_list();

        REQUIRE(sorted_list[2].name == "first");
        REQUIRE(sorted_list[1].name == "second");
        REQUIRE(sorted_list[0].name == "third");

        // No editing

        std::string second_shader;
        REQUIRE(!builder.hot_rebuild("first", second_shader, ec));

        REQUIRE(ec.value()
                == static_cast<int>(ShaderBuilderErrc::success));

        REQUIRE(second_shader.empty());
    }
}

#endif
