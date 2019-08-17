#pragma once
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <system_error>
#include <filesystem>
#include <functional>

#define SHADER_BUILDER_HOT_REBUILD_SUPPORT
#define SHADER_BUILDER_LOGGING

enum class ShaderType {NONE, VERT, FRAG, GEOM, COMP, TESS_CTRL, TESS_EVAL};

struct ShaderModule {
    std::string name;
    std::vector<std::string> used_modules;
    std::string source;
    ShaderType type = ShaderType::NONE;

#ifdef SHADER_BUILDER_HOT_REBUILD_SUPPORT
    std::filesystem::path path;
    std::filesystem::file_time_type last_write_time;
#endif

    //NOTE: used for topological sorting
    bool temp_mark;
    bool perm_mark;
};

enum class ShaderBuilderErrc {success = 0,
                              file_read_error,
                              file_does_not_exist,
                              file_is_not_regular_file,
                              include_dir_does_not_exist,
                              include_dir_is_not_a_directory,
                              circular_dependency,
                              missing_dependency,
                              syntax_error,
                              ill_formed_module
};

/// Stores and assembles shader modules
///
/// Rather hefty class handling parsing and storing modules and their interdependencies.
/// Meant to have only one instance, that builds one or many shaders sharing dependencies.
class ShaderBuilder {
public:

#ifdef SHADER_BUILDER_LOGGING
    void register_log_callback(std::function<void(std::string)> log_callback);
#endif

    void clear_modules() noexcept;

    bool has_module(std::string module_name) const;

    void set_header(std::string header);

    size_t get_modules_count() const noexcept;

    void add_module(std::string filename);
    void add_module(std::string filename, std::error_code& ec);

    void add_modules(std::initializer_list<std::string> modules);
    void add_modules(std::initializer_list<std::string> modules, std::error_code& ec);

    void import_modules_from_file(std::string modules_list_filename);
    void import_modules_from_file(std::string modules_list_filename, std::error_code& ec);

    std::optional<ShaderModule> get_module(std::string module_name) const;
    std::optional<std::reference_wrapper<ShaderModule>>
      get_module_mut(std::string module_name);

    const std::vector<ShaderModule>& get_modules_list();
    const std::vector<ShaderModule>& get_sorted_modules_list();

    size_t get_include_dirs_count() const noexcept;

    void add_include_dir(std::string dir);
    void add_include_dir(std::string dir, std::error_code& ec);

    std::string build(std::string init_module_name);
    std::string build(std::string init_module_name, std::error_code& ec);


#ifdef SHADER_BUILDER_HOT_REBUILD_SUPPORT
    bool hot_rebuild(const std::string root_module, std::string& output);

    bool hot_rebuild(const std::string root_module,
                     std::string& output,
                     std::error_code& ec);

#endif

    std::error_code get_last_ec() const;

public:

    bool topo_sort_modules(ShaderModule& root_module, std::error_code& ec);
    void topo_sort_recursive_visit(ShaderModule& module, std::error_code& ec);

    ShaderType detect_type(std::string filename);

    std::string detect_name(std::string filename);

    ShaderModule parse(const std::string filename, std::error_code& ec);

    ShaderModule parse(const std::filesystem::path source_path, std::error_code& ec);

    std::filesystem::path find_file(std::string filenmae, std::error_code& ec);

    std::string read_file(std::filesystem::path filepath, std::error_code& ec);

    std::string read_file(std::string filename, std::error_code& ec);

    void log(std::string message);

#ifdef SHADER_BUILDER_HOT_REBUILD_SUPPORT
    std::filesystem::path get_file_path(std::string filename, std::error_code& ec);

    void reload_module(ShaderModule& module, std::error_code& ec);
#endif

#ifdef SHADER_BUILDER_LOGGING
    std::function<void(std::string)> log_callback;
#endif
    std::string header;
    std::vector<ShaderModule> modules;
    std::vector<ShaderModule> sorted_modules;
    std::vector<std::filesystem::path> include_dirs; //change type to fs::path?
    std::error_code last_ec;
};

