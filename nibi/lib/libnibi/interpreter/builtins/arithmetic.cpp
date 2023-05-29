#include <iostream>

#include "arithmetic_helpers.hpp"
#include "interpreter/builtins/builtins.hpp"
#include "interpreter/interpreter.hpp"
#include "libnibi/cell.hpp"
#include "macros.hpp"

#include <cmath>

namespace nibi {

namespace builtins {

#define PERFORM_OPERATION(___op_fn)                                            \
  auto first_arg = list_get_nth_arg(ci, 1, list, env);                         \
  switch (first_arg->type) {                                                   \
  case cell_type_e::INTEGER: {                                                 \
    return allocate_cell(___op_fn<int64_t>(                                    \
        first_arg->to_integer(), ci,                                           \
        [](cell_ptr arg) -> int64_t { return arg->to_integer(); }, list,       \
        env));                                                                 \
  }                                                                            \
  case cell_type_e::DOUBLE: {                                                  \
    return allocate_cell(___op_fn<double>(                                     \
        first_arg->to_double(), ci,                                            \
        [](cell_ptr arg) -> double { return arg->to_double(); }, list, env));  \
  }                                                                            \
  default: {                                                                   \
    std::string msg = "Incorrect argument type for arithmetic function: ";     \
    msg += cell_type_to_string(first_arg->type);                               \
    ci.halt_with_error(error_c(first_arg->locator, msg));                      \
    break;                                                                     \
  }                                                                            \
  }                                                                            \
  return allocate_cell(cell_type_e::NIL);

cell_ptr builtin_fn_arithmetic_add(interpreter_c &ci, cell_list_t &list,
                                   env_c &env){
    NIBI_LIST_ENFORCE_SIZE("+", >=, 2) PERFORM_OPERATION(list_perform_add)}

cell_ptr
    builtin_fn_arithmetic_sub(interpreter_c &ci, cell_list_t &list, env_c &env){
        NIBI_LIST_ENFORCE_SIZE("-", >=, 2) PERFORM_OPERATION(list_perform_sub)}

cell_ptr
    builtin_fn_arithmetic_div(interpreter_c &ci, cell_list_t &list, env_c &env){
        NIBI_LIST_ENFORCE_SIZE("/", >=, 2) PERFORM_OPERATION(list_perform_div)}

cell_ptr
    builtin_fn_arithmetic_mul(interpreter_c &ci, cell_list_t &list, env_c &env){
        NIBI_LIST_ENFORCE_SIZE("*", >=, 2) PERFORM_OPERATION(list_perform_mul)}

cell_ptr builtin_fn_arithmetic_mod(interpreter_c &ci, cell_list_t &list,
                                   env_c &env) {
  NIBI_LIST_ENFORCE_SIZE("%", >=, 2)
  auto first_arg = list_get_nth_arg(ci, 1, list, env);
  if (first_arg->type == cell_type_e::DOUBLE) {
    double accumulate{first_arg->to_double()};
    NIBI_LIST_ITER_AND_LOAD_SKIP_N(
        2, { accumulate = std::fmod(accumulate, arg->to_double()); })
    return allocate_cell(accumulate);
  } else {
    int64_t accumulate{first_arg->to_integer()};
    NIBI_LIST_ITER_AND_LOAD_SKIP_N(2, { accumulate %= arg->to_integer(); })
    return allocate_cell(accumulate);
  }
}

cell_ptr builtin_fn_arithmetic_pow(interpreter_c &ci, cell_list_t &list,
                                   env_c &env) {
  NIBI_LIST_ENFORCE_SIZE("**", >=, 2)
  PERFORM_OPERATION(list_perform_pow)
}

} // namespace builtins

} // namespace nibi