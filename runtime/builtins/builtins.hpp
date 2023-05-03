#pragma once

#include <string>
#include <unordered_map>

#include "runtime/cell.hpp"
#include "runtime/environment.hpp"

namespace builtins {

//! \brief Retrieve a reference to a map that ties symbols to their
//!        corresponding builtin function.
std::unordered_map<std::string, function_info_s> &get_builtin_symbols_map();

//! \brief A function similar to the builtins that
//!        will load a lambda function and execute it
//!        using the global runtime object
//! \param list The list containing the lambda function and args
//! \param env The environment that will be used during execution
extern cell_ptr execute_suspected_lambda(cell_list_t &list, env_c &env);

// @ commands

extern cell_ptr builtin_fn_at_debug(cell_list_t &list, env_c &env);

// Debug helper functions

extern cell_ptr builtin_fn_debug_dbg_dbg(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_debug_dbg_var(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_debug_dbg_out(cell_list_t &list, env_c &env);

// Environment modification functions

extern cell_ptr builtin_fn_env_assignment(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_env_set(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_env_drop(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_env_fn(cell_list_t &list, env_c &env);

// Exception throwing and handling functions

extern cell_ptr builtin_fn_except_try(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_except_throw(cell_list_t &list, env_c &env);

// List functions

extern cell_ptr builtin_fn_list_push_front(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_list_push_back(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_list_iter(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_list_at(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_list_spawn(cell_list_t &list, env_c &env);

// Common functions

extern cell_ptr builtin_fn_common_len(cell_list_t &list, env_c &env);

// Checking functions

extern cell_ptr builtin_fn_check_nil(cell_list_t &list, env_c &env);
// check is true (true?)
// check is true (false?)
// check exists (exists?)
// check is nil (nil?)
// check is int (int?)
// check is real (real?)
// check is string (string?)
// check is list (list?)
// check is function (fn?)

// Assertions

extern cell_ptr builtin_fn_assert_true(cell_list_t &list, env_c &env);

// Arithmetic functions

extern cell_ptr builtin_fn_arithmetic_add(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_arithmetic_sub(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_arithmetic_div(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_arithmetic_mul(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_arithmetic_mod(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_arithmetic_pow(cell_list_t &list, env_c &env);

// Comparison functions

extern cell_ptr builtin_fn_comparison_eq(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_comparison_neq(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_comparison_lt(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_comparison_gt(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_comparison_lte(cell_list_t &list, env_c &env);
extern cell_ptr builtin_fn_comparison_gte(cell_list_t &list, env_c &env);

} // namespace builtins