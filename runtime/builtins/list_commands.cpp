#include <iostream>

#include "runtime/builtins/builtins.hpp"
#include "runtime/cell.hpp"

#include "runtime/builtins/cpp_macros.hpp"

namespace builtins {

cell_c *builtin_fn_list_push_front(cell_list_t &list, env_c &env) {
  LIST_ENFORCE_SIZE(">|", ==, 3)

  auto *value_to_push = list_get_nth_arg(1, list, env);

  auto *list_to_push_to = list_get_nth_arg(2, list, env);

  auto &list_info = list_to_push_to->as_list_info();

  // Clone the target and push it back
  list_info.list.push_front(value_to_push->clone());

  return list_to_push_to;
}

cell_c *builtin_fn_list_push_back(cell_list_t &list, env_c &env) {
  LIST_ENFORCE_SIZE("|<", ==, 3)

  auto *value_to_push = list_get_nth_arg(1, list, env);

  auto *list_to_push_to = list_get_nth_arg(2, list, env);

  auto &list_info = list_to_push_to->as_list_info();

  // Clone the target and push it back
  list_info.list.push_back(value_to_push->clone());

  return list_to_push_to;
}

cell_c *builtin_fn_list_iter(cell_list_t &list, env_c &env) {

  auto *list_to_iterate = list_get_nth_arg(1, list, env);

  auto &list_info = list_to_iterate->as_list_info();

  auto it = list.begin();
  std::advance(it, 2);
  auto *ins_to_exec_per_item = (*it);

  cell_c *idx = global_runtime->get_runtime_memory().allocate((int64_t)0);
  idx->mark_as_in_use(true);

  // here we use the map directly so we don't accidently create drop the
  // items we are iterating - so we need to manually clean up
  auto &current_env_map = env.get_map();

  current_env_map["$idx"] = idx;

  for (auto *cell : list_info.list) {

    // $it will just point to the element, not copy it
    current_env_map["$it"] = cell;

    // Execute the instructions, allowing [] to execute multiple instructions
    global_runtime->execute_cell(ins_to_exec_per_item, env, true);

    // Increment the index
    (idx->as_integer())++;
  }

  // Clean up the index
  idx->mark_as_in_use(false);
  current_env_map.erase("$idx");

  // We don't need to clean up $it, as it is just a pointer to the element
  current_env_map.erase("$it");

  // Return the list we iterated
  return list_to_iterate;
}

cell_c *builtin_fn_list_at(cell_list_t &list, env_c &env) {
  LIST_ENFORCE_SIZE("at", ==, 3)

  auto *requested_idx = list_get_nth_arg(2, list, env);

  auto *target_list = list_get_nth_arg(1, list, env);

  auto &list_info = target_list->as_list_info();

  auto actual_idx_val = requested_idx->as_integer();

  if (actual_idx_val < 0 || actual_idx_val >= list_info.list.size()) {
    throw std::runtime_error("Index OOB for given list");
  }

  auto it = list_info.list.begin();
  std::advance(it, actual_idx_val);

  return (*it);
}

cell_c *builtin_fn_list_spawn(cell_list_t &list, env_c &env) {
  LIST_ENFORCE_SIZE("<|>", ==, 3)

  auto *list_size = list_get_nth_arg(2, list, env);

  if (list_size->as_integer() < 0) {
    auto it = list.begin();
    std::advance(it, 2);
    throw runtime_c::exception_c("Cannot spawn a list with a negative size",
                                 (*it)->locator);
  }

  auto *target_value = list_get_nth_arg(1, list, env);
  target_value->mark_as_in_use(true);

  cell_list_t new_list;
  for (uint64_t i = 0; i < list_size->as_integer(); i++) {
    new_list.push_back(target_value->clone());
  }

  // Create the list and return it
  return global_runtime->get_runtime_memory().allocate(
      list_info_s{list_types_e::DATA, new_list});
}

} // namespace builtins