#include "modules.hpp"
#include "libnibi/RLL/rll_wrapper.hpp"
#include "libnibi/common/platform.hpp"
#include "libnibi/config.hpp"
#include "libnibi/interpreter/interpreter.hpp"
#include <random>

/*
    Modules loaded into the system have a lifetime that is managed by
    an aberrant cell that is stored in the environment. When the environment
    is deleted, the death callback is called and it removes itself from the
    module system. This is to ensure a user can load and drop modules as they
   see fit.

    The names of these aberrant cells are post-fixed with a randomly generated
    string of length NIBI_MODULE_ABERRANT_ID_SIZE.
    It is very unlikely that two modules will collide with user data
*/

namespace {

inline std::string generate_random_id() {
  static constexpr char POOL[] = "0123456789"
                                 "~!@#$%^&*()_+"
                                 "`-=<>,./?\"';:"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";

  static std::random_device dev;
  static std::mt19937 rng(dev());

  std::uniform_int_distribution<uint32_t> dist(0, sizeof(POOL) - 1);

  std::string result;
  for (auto i = 0; i < nibi::config::NIBI_MODULE_ABERRANT_ID_SIZE; i++) {
    result += POOL[dist(rng)];
  }

  return result;
}

} // namespace

namespace nibi {

// A module death callback
using death_callback = std::function<void()>;

// Aberrant cell used to maintain the lifetime of a loaded dynamic library
// these cells are stored into the environment that are populated when
// a module is loaded. If that module is later dropped, the cell will be
// deleted and the callback will triggeer the removal from the interpreter's
// loaded_modules_ set.
class module_cell_c final : public aberrant_cell_if {
public:
  module_cell_c() = delete;
  module_cell_c(death_callback cb, rll_ptr lib) : cb_(cb), lib_(lib) {}
  ~module_cell_c() {
    if (lib_) {
      cb_();
    }
  }
  virtual std::string represent_as_string() override {
    return "EXTERNAL_MODULE";
  }

private:
  death_callback cb_;
  rll_ptr lib_;
};

// Similar as above, but external imported files don't have an RLL in tow
// so this is just a death callback cell
class import_cell_c final : public aberrant_cell_if {
public:
  import_cell_c() = delete;
  import_cell_c(death_callback cb) : cb_(cb) {}
  ~import_cell_c() { cb_(); }
  virtual std::string represent_as_string() override {
    return "EXTERNAL_IMPORT";
  }

private:
  death_callback cb_;
};

std::filesystem::path modules_c::get_module_path(cell_ptr &module_name) {

  std::string name = module_name->as_string();

  auto opt_path = global_platform->locate_directory(name);

  if (!opt_path.has_value()) {
    global_interpreter->halt_with_error(
        error_c(module_name->locator, "Could not locate module: " + name));
  }

  // load the module

  auto path = opt_path.value();
  auto module_file = path / config::NIBI_MODULE_FILE_NAME;

  if (!std::filesystem::exists(module_file)) {
    global_interpreter->halt_with_error(
        error_c(module_name->locator,
                "Could not locate module file: " + module_file.string()));
  }

  if (!std::filesystem::is_regular_file(module_file)) {
    global_interpreter->halt_with_error(
        error_c(module_name->locator,
                "Module file is not a regular file: " + module_file.string()));
  }
  return path;
}

modules_c::module_info_s modules_c::get_module_info(std::string &module_name) {

  cell_ptr mns = allocate_cell(module_name);

  auto path = get_module_path(mns);

  auto module_file = path / config::NIBI_MODULE_FILE_NAME;

  auto expected_test_directory = path / config::NIBI_MODULE_TEST_DIR;

  env_c module_env;
  source_manager_c module_source_manager;
  interpreter_c module_interpreter(module_env, module_source_manager);
  list_builder_c module_builder(module_interpreter);
  file_reader_c module_reader(module_builder, module_source_manager);
  module_reader.read_file(module_file.string());

  module_info_s result;

  auto authors = module_env.get("authors");
  auto licenses = module_env.get("licenses");
  auto description = module_env.get("description");
  auto version = module_env.get("version");

  if (nullptr != authors) {
    if (authors->type == cell_type_e::STRING) {
      result.authors = std::vector<std::string>(1, authors->as_string());
    } else {
      auto data = authors->as_list();
      result.authors = std::vector<std::string>();
      for (auto &i : data) {
        (*result.authors).push_back(i->as_string());
      }
    }
  }

  if (nullptr != licenses) {
    if (licenses->type == cell_type_e::STRING) {
      result.licenses = std::vector<std::string>(1, licenses->as_string());
    } else {
      auto data = licenses->as_list();
      result.licenses = std::vector<std::string>();
      for (auto &i : data) {
        (*result.licenses).push_back(i->as_string());
      }
    }
  }

  if (nullptr != description) {
    result.description = description->as_string();
  }

  if (nullptr != version) {
    result.version = version->as_string();
  }

  if (!std::filesystem::is_directory(expected_test_directory)) {
    return result;
  }

  result.test_files = std::vector<std::filesystem::path>();

  for (auto &p : std::filesystem::directory_iterator(expected_test_directory)) {
    if (p.is_regular_file()) {
      if (p.path().extension() == config::NIBI_FILE_EXTENSION) {
        (*result.test_files).push_back(p.path());
      }
    }
  }

  return result;
}

void modules_c::load_module(cell_ptr &module_name, env_c &target_env) {
  std::string name = module_name->as_string();

  if (is_module_loaded(name)) {
    return;
  }

  auto path = get_module_path(module_name);
  auto module_file = path / config::NIBI_MODULE_FILE_NAME;

  env_c module_env;
  source_manager_c module_source_manager;
  interpreter_c module_interpreter(module_env, module_source_manager);
  list_builder_c module_builder(module_interpreter);
  file_reader_c module_reader(module_builder, module_source_manager);
  module_reader.read_file(module_file.string());

  // Env that everything will be dumped into and
  // then put into a cell
  environment_info_s module_cell_env = {name, new env_c()};

  bool loaded_something{false};

  auto dylib = module_env.get("dylib");
  if (nullptr != dylib) {
    load_dylib(name, *module_cell_env.env, path, dylib);
    loaded_something = true;
  }

  auto source_list = module_env.get("sources");
  if (nullptr != source_list) {
    load_source_list(name, *module_cell_env.env, path, source_list);
    loaded_something = true;
  }

  if (!loaded_something) {
    global_interpreter->halt_with_error(error_c(
        module_name->locator, "Module did not contain any loadable items"));
  }

  auto new_env_cell = allocate_cell(module_cell_env);
  target_env.set(name, new_env_cell);

  auto post = module_env.get("post");
  if (nullptr != post) {
    execute_post_import_actions(post, path);
  }
}

inline void modules_c::execute_post_import_actions(
  cell_ptr &post_list,
  std::filesystem::path &module_path) {

  auto post_list_info = post_list->as_list_info();

  for(auto &item : post_list_info.list) {
    auto file = module_path / item->as_string();
    if (!std::filesystem::exists(file)) {
      global_interpreter->halt_with_error(
        error_c(post_list->locator,
                "Could not locate post-import file: " + file.string()));
    }
    
    list_builder_c export_builder(*global_interpreter);
    file_reader_c export_reader(export_builder, source_manager_);
    export_reader.read_file(file.string());
  }
}

inline void modules_c::load_dylib(std::string &name, env_c &module_env,
                                  std::filesystem::path &module_path,
                                  cell_ptr &dylib_list) {

  // Ensure that the library file is there

  std::string suspected_lib_file = name + ".lib";
  std::filesystem::path lib_file = module_path / suspected_lib_file;

  if (!std::filesystem::exists(lib_file)) {
    global_interpreter->halt_with_error(
        error_c(dylib_list->locator,
                "Could not locate dylib file: " + lib_file.string()));
  }

  if (!std::filesystem::is_regular_file(lib_file)) {
    global_interpreter->halt_with_error(
        error_c(dylib_list->locator,
                "Dylib file is not a regular file: " + lib_file.string()));
  }

  // Load the library with RLL

  auto target_lib = allocate_rll();

  try {
    target_lib->load(lib_file.string());
  } catch (rll_wrapper_c::library_loading_error_c &e) {
    std::string err =
        "Could not load library: " + name + ".\nFailed with error: " + e.what();
    global_interpreter->halt_with_error(error_c(dylib_list->locator, err));
  }

  // Validate all listed symbolsm and import them to the module environment

  auto listed_functionality = dylib_list->as_list_info();

  for (auto &func : listed_functionality.list) {
    auto sym = func->to_string();
    if (!target_lib->has_symbol(sym)) {
      std::string err =
          "Could not locate symbol: " + sym + " in library: " + name;
      global_interpreter->halt_with_error(error_c(func->locator, err));
    }

    auto target_cell = allocate_cell(
        function_info_s(sym,
                        reinterpret_cast<cell_ptr (*)(cell_list_t &, env_c &)>(
                            target_lib->get_symbol(sym)),
                        function_type_e::EXTERNAL_FUNCTION, &module_env));

    module_env.set(sym, target_cell);
  }

  // Add the RLL to an aberrant cell and add that to the module env so it stays
  // alive

  // We construct it with a lambda that will callback to remove the item from
  // the interpreter's way of managing what libs are already loaded
  auto rll_cell =
      allocate_cell(static_cast<aberrant_cell_if *>(new module_cell_c(
          [this, name]() { this->loaded_modules_.erase(name); }, target_lib)));

  // We store in the environment so it stays alive as long as the module is
  // loaded and is removed if the module is dropped by the user or otherwise
  // made unreachable

  module_env.set(name + generate_random_id(), rll_cell);
}

inline void modules_c::load_source_list(std::string &name, env_c &module_env,
                                        std::filesystem::path &module_path,
                                        cell_ptr &source_list) {

  auto source_list_info = source_list->as_list_info();

  // Create an interpreter/ builder/ reader that will dump
  // all the source into the module env
  interpreter_c module_interpreter(module_env, source_manager_);
  list_builder_c module_builder(module_interpreter);
  file_reader_c module_reader(module_builder, source_manager_);

  // Walk over each file given by the source list
  // and read it into the module env by way of the new interpreter
  for (auto &source_file : source_list_info.list) {
    auto source_file_path = module_path / source_file->as_string();
    if (!std::filesystem::is_regular_file(source_file_path)) {
      global_interpreter->halt_with_error(
          error_c(source_file->locator,
                  "File listed in module: " + name +
                      " is not a regular file: " + source_file_path.string()));
    }
    module_reader.read_file(source_file_path.string());
  }

  // Create a death callback to remove the module from the interpreter's
  // list of imported modules
  auto import_cell =
      allocate_cell(static_cast<aberrant_cell_if *>(new import_cell_c(
          [this, name]() { this->loaded_modules_.erase(name); })));

  // We store in the environment so it stays alive as long as the import is
  // loaded and is removed if the import is dropped by the user or otherwise
  // made unreachable

  module_env.set(name + generate_random_id(), import_cell);
}

} // namespace nibi