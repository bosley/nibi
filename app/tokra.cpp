#include <iostream>
#include <vector>
#include <string>

#include <runtime/cell.hpp>
#include <runtime/memory.hpp>
#include "runtime/environment.hpp"
#include "runtime/runtime.hpp"
#include "common/error.hpp"
#include "common/source.hpp"
#include "common/input.hpp"
#include "common/list.hpp"

namespace {
  // Instruction manager used by all list builders
  // this will be utilized by the runtime who is responsible for unmarking
  // and kicking off sweeps of instruction cells
  cell_memory_manager_t* instruction_manager = new cell_memory_manager_t(nullptr);
  env_c* program_global_env{nullptr};
  memory::controller_c<cell_c>* instruction_memory{nullptr};
  source_manager_c* source_manager{nullptr};
  runtime_c* runtime{nullptr};
}

void setup() {

  /*
        TODO: We need to have a thread running specifcally for the 
              memory manager that queries the global env about alive items

              -- Maybe not a thread, but we need to hook up some
                 means to mark and then sweep the global environment
  */


  program_global_env = new env_c(nullptr);
  source_manager = new source_manager_c();
  runtime = new runtime_c(*program_global_env, *source_manager, *instruction_manager);
  delete instruction_manager;
}

void teardown() {
  delete runtime;
  delete source_manager;
  delete program_global_env;
}

void run_from_file(const std::string& file_name) {


  // List builder that will build lists from parsed tokens
  // and pass lists to a runtime
  list_builder_c list_builder(*instruction_manager, *runtime);

  // File reader that reads file and kicks off parser/ scanner 
  // that will send tokens to the list builder
  file_reader_c file_reader(list_builder, *source_manager);

  // Read the file and start the process
  file_reader.read_file(file_name);
}

int main(int argc, char** argv) {

  std::vector<std::string> args(argv, argv + argc);
  for(auto arg : args) {
    std::cout << arg << std::endl;
  }

  if (args.size() == 1) {
    std::cout << "No input file specified" << std::endl;
    return 1;
  }

  auto entry_file = args[1];

  setup();

  run_from_file(entry_file);

  teardown();

  return 0;
}
