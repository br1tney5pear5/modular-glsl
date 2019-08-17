#include "modular-glsl.h"

#include <regex>
#include <tuple>
#include <fstream>
#include <map>
#include <string>


namespace fs = std::filesystem;

namespace std {
    template <>
    struct is_error_code_enum<ShaderBuilderErrc> : true_type {};
}

namespace {
    struct ShaderBuilderErrCategory : std::error_category {
        const char* name() const noexcept override;
        std::string message(int ev) const override;
    };

    const char* ShaderBuilderErrCategory::name() const noexcept {
        return "shader_builder";
    }

    std::string ShaderBuilderErrCategory::message(int ev) const {
        switch (static_cast<ShaderBuilderErrc>(ev)) {
        case ShaderBuilderErrc::success:
            return "Success";
        case ShaderBuilderErrc::file_read_error:
            return "Failed to read file";
        case ShaderBuilderErrc::file_does_not_exist:
            return "Specified file does not exist.";
        case ShaderBuilderErrc::file_is_not_regular_file:
            return "Specified module file is not a regular file.";
        case ShaderBuilderErrc::include_dir_does_not_exist :
            return "Specified include directory does not exist";
        case ShaderBuilderErrc::include_dir_is_not_a_directory:
            return "Specified include directory path does not lead to directory";
        case ShaderBuilderErrc::circular_dependency:
            return "Circular dependency found; dependency graph is not acyclic.";
        case ShaderBuilderErrc::missing_dependency:
            return "Missing dependency; Cannot find required module";
        case ShaderBuilderErrc::syntax_error:
            return "Failed to parse shader module";
        case ShaderBuilderErrc::ill_formed_module:
            return "Module is ill formed";
        default:
            return "(unrecognized error)";
        }
    }

    const ShaderBuilderErrCategory theShaderBuilderErrCategory {};
}

static std::error_code make_error_code(ShaderBuilderErrc e) {
    return {static_cast<int>(e), theShaderBuilderErrCategory};
}

inline void ShaderBuilder::log(std::string message) {
#ifdef SHADER_BUILDER_LOGGING
    if(log_callback) log_callback(message);
#endif
}

void ShaderBuilder::register_log_callback(std::function<void(std::string)> log_callback){
    this->log_callback = log_callback;
}
/// Removes all modules in builder
void ShaderBuilder::clear_modules() noexcept {
    modules.clear();
}


/// Checks if builder has a given module
///
/// @param module_name Name of module to look for
/// @return True if module is present, false otherways
bool ShaderBuilder::has_module(std::string module_name) const {
    bool ret = false;
    for(auto& m : modules)
        if(m.name == module_name) ret = true;
    return ret;
}


/// Sets a shader header included at the top of every built file
/// @param header header contents
void ShaderBuilder::set_header(const std::string header) {
    this->header = header;
}


/// Returns number of modules
/// @return number of modules
size_t ShaderBuilder::get_modules_count() const noexcept {
    return modules.size();
}

/// Returns number of include directories
/// @return number of include directories
size_t ShaderBuilder::get_include_dirs_count() const noexcept {
    return include_dirs.size();
}


/// Adds module to the builder
/// @param filename Name of the file with module
void ShaderBuilder::add_module(std::string filename) {
    add_module(filename, last_ec);
}

/// Adds module to the builder
/// @param filename Name of the file with module
/// @param ec Error code
void ShaderBuilder::add_module(std::string filename, std::error_code& ec) {
    ec = ShaderBuilderErrc::success;
    auto module = parse(filename, ec);
    if(ec) {
        last_ec = ec;
        return;
    }

#ifdef SHADER_BUILDER_HOT_REBUILD_SUPPORT
    module.path = get_file_path(filename, ec);
    if(ec) {
        last_ec = ec;
        return;
    }
    module.last_write_time = fs::last_write_time(module.path, ec);
#endif
    if(!ec && !has_module(module.name))
        modules.push_back(module);
    last_ec = ec;
    return;
}


/// Adds one or more modules to the builder
/// @param modules Brace-enclosed initializer list of names of files with modules to add
void ShaderBuilder::add_modules(std::initializer_list<std::string> modules){
    add_modules(modules, last_ec);
}

/// Adds one or more modules to the builder
/// @param modules Brace-enclosed initializer list of names of files with modules to add
/// @param ec Error code
/// @todo Should it perhaps break on return on first error?
/// @todo Should there be bool for it like add_modules_fail_fast ?
void ShaderBuilder::add_modules(std::initializer_list<std::string> modules,
                                std::error_code& ec){
    for(auto& module : modules) add_module(module, ec); 
    last_ec = ec;
}


/// Imports modules listed in file
/// @param modules_list_filename Name of the file with list of modules to import
void ShaderBuilder::import_modules_from_file(std::string modules_list_filename) {
    import_modules_from_file(modules_list_filename, last_ec);
}

/// Imports modules listed in file
/// @param modules_list_filename Name of the file with list of modules to import
/// @param ec Error code
void ShaderBuilder::import_modules_from_file(std::string modules_list_filename,
                                          std::error_code& ec) {
    ec = ShaderBuilderErrc::success;
    std::stringstream ss(read_file(modules_list_filename, ec));
    if(ec) {
        last_ec = ec;
        return;
    }

    for(std::string line; std::getline(ss,line);) {
        add_module(line, ec);
        if(ec) break;
    }
    last_ec = ec;
    return;
}


/// Returns given module by value
/// @param module_name
/// @return module specified by module_name or nothing if no such module is present
std::optional<ShaderModule> ShaderBuilder::get_module(std::string module_name) const {
    for(auto& m : modules) {
        if(m.name == module_name) {
            return std::optional<ShaderModule>(m);
        }
    }
    return std::optional<ShaderModule>();
}


/// Returns given module by referece
/// @param module_name
/// @return module specified by module_name or nothing if no such module is present
std::optional<std::reference_wrapper<ShaderModule>>
ShaderBuilder::get_module_mut(std::string module_name){
    for(auto& m : modules) {
        if(m.name == module_name) {
            return std::optional<std::reference_wrapper<ShaderModule>>(m);
        }
    }
    return std::optional<std::reference_wrapper<ShaderModule>>();
}


/// Returns list of all builder modules
/// @return vector of modules
const std::vector<ShaderModule>& ShaderBuilder::get_modules_list() {
    return modules;
}


/// Returns list of topologicaly sorted modules
///
/// Returns reference to vector which is buffer for topological sort function.
/// After build this vector of modules in the same order as they went to the output file.
/// Should not be used for anything but debugging.
/// @return vector of sorted modules
const std::vector<ShaderModule>& ShaderBuilder::get_sorted_modules_list() {
    return sorted_modules;
}


/// Appends directory to search-for-modules directories
/// @param dir new include path
/// @todo take std::filesystem::path instead of string
void ShaderBuilder::add_include_dir(std::string dir){
    add_include_dir(dir, last_ec);
}

/// Appends directory to search-for-modules directories
/// @param dir new include path
/// @param ec Error code
void ShaderBuilder::add_include_dir(std::string dir, std::error_code& ec){
    ec = ShaderBuilderErrc::success;
    fs::path dirpath(dir);

    for(auto& include_dir : include_dirs)
        if(fs::equivalent(dirpath, include_dir)) return;

    if(fs::exists(dirpath, ec) && !ec) {
        if(fs::is_directory(dirpath, ec) && !ec) {
            include_dirs.push_back(dir);
        } else {
            if(!ec) ec = ShaderBuilderErrc::include_dir_is_not_a_directory;
                // else let std::filesystem error_code through
        }
    } else {
        if(!ec) ec = ShaderBuilderErrc::include_dir_does_not_exist;
            // else let std::filesystem error_code through
    }
    last_ec = ec;
}


/// Builds shader
///
/// Takes name of the init module as parameter and builds shader incuding it,
/// all of it's dependencies and all dependencies of dependecies etc.
/// Noteworthy, modules that were added to the builder but are not used
/// in a given shader are not going to be included in the output shader.
/// @param init_module_name name of root module to start with - your main method
/// @return resulting shader as string
std::string ShaderBuilder::build(std::string init_module_name){
    return build(init_module_name, last_ec);
}

/// Builds shader
///
/// Takes name of the init module as parameter and builds shader incuding it,
/// all of it's dependencies and all dependencies of dependecies etc.
/// Noteworthy, modules that were added to the builder but are not used
/// in a given shader are not going to be included in the output shader.
/// @param init_module_name name of root module to start with - your main method
/// @param ec Error code
/// @return resulting shader as string
std::string ShaderBuilder::build(std::string init_module_name, std::error_code& ec){
    std::string ret = "";

    auto opt_init_module = get_module_mut(init_module_name);
    if(opt_init_module.has_value())
        topo_sort_modules(opt_init_module.value(),ec);
    else {
        log("init build module " + init_module_name + " not found");
        last_ec = ec;
        return ret;
    }

    size_t final_size = header.size();
    for(auto m : modules) final_size += m.source.size();

    ret.reserve(final_size);

    ret += header;
    for(auto m : sorted_modules) ret += m.source;

    last_ec = ec;
    return ret;
}


std::error_code ShaderBuilder::get_last_ec() const {
    return last_ec;
}


/// Sorts modules according to their dependencies
///
/// Performs module topological sort so modules are included in correct order
/// Fails if finds circular dependency or missing dependency
///
/// @param ec error_code - unused
/// @return true, unless it fails
/// @todo optimize sort, make non recursive
bool ShaderBuilder::topo_sort_modules(ShaderModule& root_module,
                                      std::error_code& ec) {
   for(auto& m : modules){
        m.temp_mark = false;
        m.perm_mark = false;
    }

    sorted_modules.clear();
    sorted_modules.reserve(modules.size());

    topo_sort_recursive_visit(root_module, ec);
    if(ec){
        log("failed to resolve dependencies");
        last_ec = ec;
        return false;
    }
    last_ec = ec;
    return true;
}

/// Recursive part of topo_sort_modules
///
/// @param module currenty "visited" module
/// @return true on success, false if there is circular or missing dependency
void ShaderBuilder::topo_sort_recursive_visit(ShaderModule& module, std::error_code& ec){
    if(module.perm_mark == true) return;
    if(module.temp_mark == true) {
        ec = ShaderBuilderErrc::circular_dependency;

        return; }

    module.temp_mark = true;

    for(auto& dep_name : module.used_modules) {
        auto opt_dep = get_module_mut(dep_name); // NOTE: opt_ prefix marks optional
        if(opt_dep.has_value()) {
            topo_sort_recursive_visit(opt_dep.value(), ec);
            if(ec){
                if(ec.value() ==
                   static_cast<int>(ShaderBuilderErrc::circular_dependency)){
                       log("circular dependency " + dep_name + " used by " + module.name);
                }
                return;
            }
        } else {
            ec = ShaderBuilderErrc::missing_dependency;
            log("missing module " + dep_name + "used by " + module.name);
            return;
        }
    }

    module.temp_mark = false;
    module.perm_mark = true;

    sorted_modules.push_back(module);
}


/// Detects shader type depending on filename extension
///
/// For filename with a given extension it will return
/// "*.vert"      - VERT
/// "*.frag"      - FRAG
/// "*.geom"      - GEOM
/// "*.comp"      - COMP
/// "*.tess_ctrl" - TESS_CTRL
/// "*.tess_eval" - TESS_EVAL
///
/// @param filename name of the file
/// @return return detected type or type NONE
ShaderType ShaderBuilder::detect_type(std::string filename){
    //NOTE: not perfect but certainly good enough
    auto match_ext = [](std::string str, std::string ext) -> bool {
                         [[maybe_unused]]std::smatch match;
                         auto regex = std::regex(".+\\." + ext ,
                                                 std::regex_constants::ECMAScript);
                         return std::regex_match(str, match, regex);
                     };

         if(match_ext(filename, "vert"))
        return ShaderType::VERT;
    else if(match_ext(filename, "frag"))
        return ShaderType::FRAG;
    else if(match_ext(filename, "geom"))
        return ShaderType::GEOM;
    else if(match_ext(filename, "comp"))
        return ShaderType::COMP;
    else if(match_ext(filename, "tess_ctrl"))
        return ShaderType::TESS_CTRL;
    else if(match_ext(filename, "tess_eval"))
        return ShaderType::TESS_EVAL;

    return ShaderType::NONE;
}

/// Detects module name from it's filename
///
/// @param filename name of the file
/// @return return detected name or empty string if fails
std::string ShaderBuilder::detect_name(std::string filename){
    //NOTE: not perfect but certainly good enough
    std::smatch match;
    auto regex = std::regex("(.*)\\.(.*)",
                            std::regex_constants::ECMAScript);

    if(std::regex_match(filename, match, regex)) return match[1];
    return "";
}


/// Reads and parses shader module file
///
/// Reads and parses directive starting with "#"
/// Currenly it supports:
///
/// #module "name"      - Specifies name of parsed module
///                        Required.
///
/// #type "shadertype"  - Specifies shader type,
///                        overwrites type file extension detected type.
///
/// #uses "modulename"  - Declares module of given name
///                        as dependency of module that is parsed/
///
/// #include "filename" - With literally inserts contents of file with given
///                        filename into parsed file. It ought to be used only internally.
/// @param filename Name of the file to be parsed
/// @return Shader module object
/// @todo multipass
ShaderModule ShaderBuilder::parse(const std::string filename, std::error_code& ec) {
    ec = ShaderBuilderErrc::success;
    auto filepath = find_file(filename, ec);

    if(ec) {
        last_ec = ec;
        return ShaderModule();
    }

    return parse(filepath, ec);
}

/// Parses shader module source
///
/// Reads and parses directive starting with "#"
/// Currenly it supports:
///
/// #module "name"      - Specifies name of parsed module
///                        Required.
///
/// #type "shadertype"  - Specifies shader type,
///                        overwrites type file extension detected type.
///
/// #uses "modulename"  - Declares module of given name
///                        as dependency of module that is parsed/
///
/// #include "filename" - With literally inserts contents of file with given
///                        filename into parsed file. It ought to be used only internally.
/// @param filename Name of the file to be parsed
/// @param ec Error code
/// @return Shader module object
/// @todo multipass
ShaderModule ShaderBuilder::parse(const fs::path source_path, std::error_code& ec) {
    ec = ShaderBuilderErrc::success;

    std::string output;
    ShaderModule module;

#ifdef SHADER_BUILDER_HOT_REBUILD_SUPPORT
    module.path = source_path;
    // module.last_write_time = fs::last_write_time(module.path, ec);
#endif

    if(ec) {last_ec = ec; return module;}

    std::string source = read_file(source_path, ec);

    if(ec) {last_ec = ec; return module;}

    module.type = detect_type(source_path.filename());
    module.name = detect_name(source_path.filename());

    auto line_no_at = [](const std::string& str, size_t pos) -> size_t {
                            size_t line_no = 1;
                            for(size_t i=0; i<pos; ++i)
                                if(str[i] == '\n') line_no++;
                            return line_no;
                        };

    auto parse_err = [&](size_t pos, std::string source_fragment, std::string msg){
                            ec = ShaderBuilderErrc::syntax_error;
                            log("while parsing" + source_path.filename().string() +
                                ":" + std::to_string(line_no_at(source, pos)) +
                                source_fragment + msg);
                        };

    // if is surrounded by quotes deletes them and returns true, false otherwise
    auto remove_quotes = [](std::string& str) -> bool {
                             if(*(str.begin()) == '"' && *(str.end()-1) == '"') {
                                 str.erase(0,1);
                                 str.erase(str.size()-1,1);
                                 return true;
                             } else return false;
                        };





    std::regex preprocess_directive_regex("(?:\\n)?((?:\\#)[a-z_]+)(?:\\s+)(\".*?\")(?:\\s+)?",
                                        std::regex_constants::ECMAScript
                                        // std::regex_constants::multiline
                                        );

    //       line no        delete       include
    std::map<int, std::pair<std::string, std::string>> edit_map;

    // edits are executed after parsing
    auto edit_at = [&](size_t pos,
                        const std::string& deletion,
                        const std::string& insertion)
                    {
                        edit_map.insert(make_pair(pos,
                                                    make_pair(deletion, insertion))); 
                    };

    auto begin = std::sregex_iterator(source.cbegin(),
                                      source.cend(),
                                      preprocess_directive_regex);
    auto end = std::sregex_iterator();

    size_t module_keyword_hits = 0; //NOTE: there ought to be only one module per file (for now)
    size_t type_keyword_hits = 0; //NOTE: there ought to be only one type per file (for now)

    for(auto i = begin;  i != end; ++i) {
        std::smatch match = *i;
        const std::string& keyword = match[1];

        if(match.size() == 3 && keyword == "#include") {
            std::string include_filename = match[2];

            if(remove_quotes(include_filename)) {
                std::string include_file_contents = read_file(include_filename, ec);

                if(!ec) {
                    edit_at(match.position(0), match[0], include_file_contents);
                } else{
                    parse_err(match.position(0), match[0],
                                std::string("file " + match[2].str() + " not found") );
                }
            } else {
                parse_err(match.position(0), match[0],
                            "syntax error");
            }
        } else if(match.size() == 3 && keyword == "#module") {
            std::string module_name = match[2];

            module_keyword_hits++;
            if(module_keyword_hits > 1) 
                parse_err(match.position(0), match[0],
                            "more than one module definition");

            if(remove_quotes(module_name)) {

                module.name = module_name;

                edit_at(match.position(0), match[0], "");
            } else {
                parse_err(match.position(0), match[0],
                          "syntax error");
            }
        } else if(match.size() == 3 && keyword == "#uses") {
            std::string used_module_name = match[2];

            if(remove_quotes(used_module_name)) {

                module.used_modules.push_back(used_module_name);

                edit_at(match.position(0), match[0], "");
            }
        } else if(match.size() == 3 && keyword == "#type") {
            std::string module_type = match[2];

            type_keyword_hits++;
            if(type_keyword_hits > 1) 
                parse_err(match.position(0), match[0],
                            "more than one type definition");

            if(remove_quotes(module_type)) {

                     if(module_type == "NONE") module.type = ShaderType::NONE;
                else if(module_type == "FRAG") module.type = ShaderType::FRAG;
                else if(module_type == "VERT") module.type = ShaderType::VERT;
                else if(module_type == "GEOM") module.type = ShaderType::GEOM;
                else if(module_type == "COMP") module.type = ShaderType::COMP;
                else if(module_type == "TESS_CTRL") module.type = ShaderType::TESS_CTRL;
                else if(module_type == "TESS_EVAL") module.type = ShaderType::TESS_EVAL;

                edit_at(match.position(0), match[0], "");
            }
        }
        // else if
        // else if
        // else if
        // ...
        else {
            parse_err(match.position(0), match[0],
                        std::string("unrecognised keyword " + keyword));
            last_ec = ec;
            return module;
        }

        if(ec) {
            last_ec = ec;
            return module;
        }
    }

    if(module.name.empty()){
        log("unnamed module in " + source_path.filename().string());
        ec = ShaderBuilderErrc::ill_formed_module;
        last_ec = ec;
        return module;
    }


    size_t deleted_chars = 0;
    size_t inserted_chars = 0;

    for(auto const& edit : edit_map) {
        auto const& deletion = edit.second.first; 
        auto const& insertion = edit.second.second; 

        deleted_chars  += deletion.size();
        inserted_chars += insertion.size();
    }

    std::string header =
        "\n//=============================== " + module.name + "\n";

    if(inserted_chars > deleted_chars) output.reserve(header.size() +
                                                      source.size() +
                                                      inserted_chars -
                                                      deleted_chars);
    output.assign(header + source);

    deleted_chars = 0;
    inserted_chars = 0;

    for(auto const& edit : edit_map) {
        auto pos =
            edit.first +
            header.size() +
            inserted_chars -
            deleted_chars;
        auto const& deletion = edit.second.first; 
        auto const& insertion = edit.second.second; 

        output.erase(pos, deletion.size());
        output.insert(pos, insertion);

        deleted_chars  += deletion.size();
        inserted_chars += insertion.size();
    }
    module.source = output;

    last_ec = ec;
    return module;
}


fs::path ShaderBuilder::find_file(std::string filename, std::error_code& ec) {
    for(auto include_dir : include_dirs) {
        fs::path filepath(include_dir / filename);
        if(fs::exists(filepath, ec) && !ec) return filepath;
    }
    return fs::path();
}


/// Reads file at the given path
///
/// @param filepath Path to file
/// @param ec Error code
std::string ShaderBuilder::read_file(fs::path filepath, std::error_code& ec){

    std::string file_contents;
    std::ifstream file;

    ec = ShaderBuilderErrc::file_read_error;

    if(fs::exists(filepath, ec) && !ec) {
        if(fs::is_regular_file(filepath, ec) && !ec) {
            auto file_size = fs::file_size(filepath);
            file.open(filepath);
            if(file.is_open() && file.good() ) {
                file_contents
                    .reserve(file_size);

                file_contents
                    .assign((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

                ec = ShaderBuilderErrc::success;
                last_ec = ec;
                return file_contents;

            } else {
                ec = ShaderBuilderErrc::file_read_error;
            }
        } else {
            if(!ec) ec = ShaderBuilderErrc::file_is_not_regular_file;
            // else let std::filesystem error_code through
        }
    } else {
        if(!ec) ec = ShaderBuilderErrc::file_does_not_exist;
        // else let std::filesystem error_code through
    }

    last_ec = ec;
    return file_contents;
}

/// Finds file and reads it's contents
///
/// This function searches for the file with given name in include paths,
/// then reads and returns the first match.
///
/// @param filename Name of the file to be read
/// @param ec Error code
std::string ShaderBuilder::read_file(std::string filename, std::error_code& ec){

    std::string file_contents;
    std::ifstream file;

    ec = ShaderBuilderErrc::file_read_error;

    for(auto include_dir : include_dirs) {
        fs::path filepath(include_dir / filename);

        file_contents = read_file(filepath, ec);

        if(!ec) break;
    }

    last_ec = ec;
    return file_contents;
}

#ifdef SHADER_BUILDER_HOT_REBUILD_SUPPORT

/// Searches for file in include directories and returns it's path
///
/// Note that this function is only available when SHADER_BUILDER_HOT_REBUILD_SUPPORT is set
///
/// @param filename Name of the file to look for
/// @param ec Error code
/// @return Path to the given file
fs::path ShaderBuilder::get_file_path(std::string filename, std::error_code& ec){

    bool filepath_status = false;

    fs::path filepath;

    for(auto include_dir : include_dirs) {
        filepath = fs::path(include_dir / filename);

        if(fs::exists(filepath, ec) && !ec &&
           fs::is_regular_file(filepath, ec) && !ec){
            filepath_status = true; 
            break;
        }
    }
    if(!filepath_status) {
        //TODO: hadnle error
    }

    last_ec = ec;
    return filepath;
}


/// Reloads module from attached file
///
/// Note that this function is only available when SHADER_BUILDER_HOT_REBUILD_SUPPORT is set
///
/// @param module reference to module to reload
/// @param ec Error code
void ShaderBuilder::reload_module(ShaderModule& module, std::error_code& ec){
   ec = ShaderBuilderErrc::success;
   auto newmodule = parse(module.path, ec);
   if(ec) {
       last_ec = ec; return;
   }
   module = newmodule;
}


/// Checks any of the modules sources were modified and rebuilds shader if so
///
/// Note that it would rebuild even if dirty module is not used by root module
/// or any of its dependencies
/// Note that this function is only available when SHADER_BUILDER_HOT_REBUILD_SUPPORT is set
///
/// @param root_module Shader root module name - your main function
/// @param output reference to string, to which save rebuilt shader
/// @return true if rebuild was performed, false otherways
bool ShaderBuilder::hot_rebuild(const std::string root_module, std::string& output) {
    return hot_rebuild(root_module, output, last_ec);
}

/// Checks any of the modules sources were modified and rebuilds shader if so
///
/// Note that it would rebuild even if dirty module is not used by root module
/// or any of its dependencies
/// Note that this function is only available when SHADER_BUILDER_HOT_REBUILD_SUPPORT is set
///
/// @param root_module Shader root module name - your main function
/// @param output reference to string, to which save rebuilt shader
/// @return true if rebuild was performed, false otherways
bool ShaderBuilder::hot_rebuild(const std::string root_module,
                                std::string& output,
                                std::error_code& ec) {
    ec = ShaderBuilderErrc::success;
    bool any_dirty = false;
    for(auto& module : modules){
        auto newtime = fs::last_write_time(module.path, ec);
        if(ec) return false;

        if(module.last_write_time != newtime){
            reload_module(module, ec);
            module.last_write_time = newtime;
            any_dirty = true;

            if(ec) {
                last_ec = ec;
                return false;
            }
        }
    }
    if(any_dirty){
        output = build(root_module, ec);
        return true;
    }
    return false;
}
#endif
