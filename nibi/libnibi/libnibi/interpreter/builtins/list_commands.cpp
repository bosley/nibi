#include <iostream>

#include "interpreter/builtins/builtins.hpp"
#include "libnibi/cell.hpp"

#include "list_helpers.hpp"

namespace nibi {
namespace builtins {

cell_ptr builtin_fn_list_push_front(interpreter_c &ci, cell_list_t &list,
                                    env_c &env) {
  NIBI_LIST_ENFORCE_SIZE(">|", ==, 3)

  auto value_to_push = list_get_nth_arg(ci, 1, list, env);

  auto list_to_push_to = list_get_nth_arg(ci, 2, list, env);

  auto &list_info = list_to_push_to->as_list_info();

  // Clone the target and push it back
  list_info.list.push_front(value_to_push->clone());

  return list_to_push_to;
}

cell_ptr builtin_fn_list_push_back(interpreter_c &ci, cell_list_t &list,
                                   env_c &env) {
  NIBI_LIST_ENFORCE_SIZE("|<", ==, 3)

  auto value_to_push = list_get_nth_arg(ci, 1, list, env);

  auto list_to_push_to = list_get_nth_arg(ci, 2, list, env);

  auto &list_info = list_to_push_to->as_list_info();

  // Clone the target and push it back
  list_info.list.push_back(value_to_push->clone());

  return list_to_push_to;
}

cell_ptr builtin_fn_list_pop_back(interpreter_c &ci, cell_list_t &list,
                                  env_c &env) {
  NIBI_LIST_ENFORCE_SIZE("|>>", ==, 2)

  auto target = list_get_nth_arg(ci, 1, list, env);

  auto &list_info = target->as_list_info();

  if (list_info.list.empty()) {
    return target;
  }

  list_info.list.pop_back();

  return target;
}

cell_ptr builtin_fn_list_pop_front(interpreter_c &ci, cell_list_t &list,
                                   env_c &env) {
  NIBI_LIST_ENFORCE_SIZE("<<|", ==, 2)

  auto target = list_get_nth_arg(ci, 1, list, env);

  auto &list_info = target->as_list_info();

  if (list_info.list.empty()) {
    return target;
  }

  list_info.list.pop_front();

  return target;
}

cell_ptr builtin_fn_list_iter(interpreter_c &ci, cell_list_t &list,
                              env_c &env) {

  NIBI_LIST_ENFORCE_SIZE("iter", ==, 4)

  auto list_to_iterate = list_get_nth_arg(ci, 1, list, env);

  auto &list_info = list_to_iterate->as_list_info();

  auto it = list.begin();

  std::advance(it, 2);
  auto symbol_to_bind = (*it)->as_symbol();

  std::advance(it, 1);
  auto ins_to_exec_per_item = (*it);

  auto iter_env = env_c(&env);

  auto &current_env_map = iter_env.get_map();

  for (auto cell : list_info.list) {

    current_env_map[symbol_to_bind] = ci.execute_cell(cell, iter_env);

    ci.execute_cell(ins_to_exec_per_item, iter_env, true);
  }

  // Return the list we iterated
  return list_to_iterate;
}

cell_ptr builtin_fn_list_at(interpreter_c &ci, cell_list_t &list, env_c &env) {
  NIBI_LIST_ENFORCE_SIZE("at", ==, 3)

  auto requested_idx = list_get_nth_arg(ci, 2, list, env);

  auto target_list = list_get_nth_arg(ci, 1, list, env);

  auto &list_info = target_list->as_list_info();

  auto actual_idx_val = requested_idx->as_integer();

  if (actual_idx_val < 0 || actual_idx_val >= list_info.list.size()) {
    throw std::runtime_error("Index OOB for given list");
  }

  return list_get_nth_arg(ci, actual_idx_val, list_info.list, env);
}

cell_ptr builtin_fn_list_spawn(interpreter_c &ci, cell_list_t &list,
                               env_c &env) {
  NIBI_LIST_ENFORCE_SIZE("<|>", ==, 3)

  auto list_size = list_get_nth_arg(ci, 2, list, env);

  if (list_size->as_integer() < 0) {
    auto it = list.begin();
    std::advance(it, 2);
    throw interpreter_c::exception_c("Cannot spawn a list with a negative size",
                                     (*it)->locator);
  }

  // Create the list and return it
  return allocate_cell(
      list_info_s{list_types_e::DATA,
                  cell_list_t(list_size->as_integer(),
                              list_get_nth_arg(ci, 1, list, env)->clone())});
}

} // namespace builtins
} // namespace nibi
