#pragma once

#include "libnibi/RLL/rll_wrapper.hpp"
#include "libnibi/source.hpp"
#include <any>
#include <cassert>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#define CELL_LIST_USE_STD_VECTOR 1

#if CELL_LIST_USE_STD_VECTOR
#include <vector>
#else
#include <deque>
#endif

namespace nibi {

#if CELL_LIST_USE_STD_VECTOR
static constexpr std::size_t CELL_VEC_RESERVE_SIZE = 1;
#endif

//! \brief The type of a cell
//! \note The aberrant type is Mysterious externally defined type
//!       meant to be used by external libraries that
//!       need to store data in the cell
enum class cell_type_e {
  NIL,
  INTEGER,
  DOUBLE,
  STRING,
  LIST,
  ABERRANT,
  FUNCTION,
  SYMBOL,
  ENVIRONMENT,
  DICT,
};

extern const char *cell_type_to_string(const cell_type_e type);

//! \brief The type of a function that a cell holds
enum class function_type_e {
  UNSET,                // Function type is not set
  BUILTIN_CPP_FUNCTION, // Function implemented in C++
  EXTERNAL_FUNCTION,    // Function imported from shared lib
  LAMBDA_FUNCTION,      // Function defined in source code by user
  FAUX                  // Function used to redirect keywords for special
                        // processing
};

enum class list_types_e {
  INSTRUCTION, // A single list of instruction (+ 1 2 3)
  DATA,        // A data list [1 2 3]
  ACCESS       // An access list {a b c}
};

// Forward declarations
class env_c;
class cell_c;
class interpreter_c;
class cell_processor_if;

//! \brief A cell pointer type
using cell_ptr = std::shared_ptr<cell_c>;

constexpr auto allocate_cell = [](auto... args) -> nibi::cell_ptr {
  return std::make_shared<nibi::cell_c>(args...);
};

//! \brief A list of cells

#if CELL_LIST_USE_STD_VECTOR
using cell_list_t = std::vector<cell_ptr>;
#else
using cell_list_t = std::deque<cell_ptr>;
#endif

//! \brief A function that takes a list of cells and an environment
using cell_fn_t =
    std::function<cell_ptr(cell_processor_if &ci, cell_list_t &, env_c &)>;

using cell_dict_t = std::unordered_map<std::string, cell_ptr>;

//! \brief Lambda information that can be encoded into a cell
struct lambda_info_s {
  std::vector<std::string> arg_names;
  cell_ptr body{nullptr};
};

//! \brief Function wrapper that holds the function
//!        pointer, the name, and the type of the function
//! \note  The operating env is a raw pointer as it may
//!        be owned by the cell or not. Lambdas for instance
//!        point to the environment they were defined in
//!        but do not own it, while MACROS own the environment
//!        to hold onto construction data. While two pointers
//!        or a further wrapper could be used, this is lighter
struct function_info_s {
  std::string name;
  cell_fn_t fn;
  function_type_e type;
  std::optional<lambda_info_s> lambda{std::nullopt};
  env_c *operating_env{nullptr};
  function_info_s() : name(""), fn(nullptr), type(function_type_e::UNSET){};
  function_info_s(std::string name, cell_fn_t fn, function_type_e type,
                  env_c *env = nullptr)
      : name(name), fn(fn), type(type), operating_env(env) {}
};

//! \brief List wrapper that holds list meta data
struct list_info_s {
  list_types_e type;
  cell_list_t list;
  list_info_s(list_types_e type, cell_list_t list) : type(type), list(list) {}

  list_info_s(list_types_e type) : type(type) {
#if CELL_LIST_USE_STD_VECTOR
    list.reserve(CELL_VEC_RESERVE_SIZE);
#endif
  }
};

// Temporary wrapper to distnguish strings from symbols
// in the cell constructor
struct symbol_s {
  std::string data;
};

//! \brief Environment information that can be encoded into a cell
struct environment_info_s {
  std::string name;
  std::shared_ptr<env_c> env{nullptr};
};

//! \brief An exception that is thrown when a cell is accessed
//!        in a way that does not correspond to its type
class cell_access_exception_c : public std::exception {
public:
  //! \brief Create the exception
  //! \param message The message to display
  //! \param source_location The location in the source code
  cell_access_exception_c(std::string message, locator_ptr source_location)
      : message_(message), source_location_(source_location) {}

  //! \brief Get the message
  char *what() { return const_cast<char *>(message_.c_str()); }

  //! \brief Get the source location
  locator_ptr get_source_location() const { return source_location_; }

private:
  std::string message_;
  locator_ptr source_location_;
};

//! \brief A cell type that can be used to store data
//!        in the interpreter from an external dynamic library
//!
//!       This is a callback interface that the external library
class aberrant_cell_if {
public:
  //! \brief Create the cell
  aberrant_cell_if() = default;

  //! \brief Create the cell with a tag
  //! \param tag The tag to set
  aberrant_cell_if(std::size_t tag) : tag_(tag) {}

  virtual ~aberrant_cell_if() = default;
  //! \brief Convert the cell to a string
  //! \note If an exception occurs, throw a cell_access_exception_c
  //!       with the message
  virtual std::string represent_as_string() = 0;

  //! \brief Clone the cell
  virtual aberrant_cell_if *clone() = 0;

  //! \brief Get the tag of the cell
  std::size_t get_tag() const { return tag_; }

  //! \brief Check if the cell is tagged
  //! \return True if the cell is tagged
  bool is_tagged() const { return tag_ != 0; }

protected:
  //! \brief Set the tag of the cell
  //! \param tag The tag to set
  void set_tag(const std::size_t tag) {
    assert(0 != tag);
    tag_ = tag;
  }

private:
  std::size_t tag_{0};
};

//! \brief A cell
class cell_c {
public:
  //! \brief Create a cell with a given type
  cell_c(cell_type_e type) : type(type) {
    // Initialize the data based on given type
    switch (type) {
    case cell_type_e::NIL:
      [[fallthrough]];
    case cell_type_e::ABERRANT:
      data = nullptr;
      break;
    case cell_type_e::ENVIRONMENT:
      data = environment_info_s{"", nullptr};
      break;
    case cell_type_e::FUNCTION:
      data = function_info_s("", nullptr, function_type_e::UNSET);
      break;
    case cell_type_e::INTEGER:
      data = int64_t(0);
      break;
    case cell_type_e::DOUBLE:
      data = double(0.00);
      break;
    case cell_type_e::STRING:
      [[fallthrough]];
    case cell_type_e::SYMBOL:
      data = std::string();
      break;
    case cell_type_e::LIST:
      data = list_info_s(list_types_e::DATA);
      break;
    }
  }
  cell_c(int64_t data) : type(cell_type_e::INTEGER), data(data) {}
  cell_c(double data) : type(cell_type_e::DOUBLE), data(data) {}
  cell_c(std::string data) : type(cell_type_e::STRING), data(data) {}
  cell_c(symbol_s data) : type(cell_type_e::SYMBOL), data(data.data) {}
  cell_c(list_info_s list) : type(cell_type_e::LIST), data(list) {}
  cell_c(aberrant_cell_if *acif) : type(cell_type_e::ABERRANT), data(acif) {}
  cell_c(function_info_s fn) : type(cell_type_e::FUNCTION), data(fn) {}
  cell_c(environment_info_s env) : type(cell_type_e::ENVIRONMENT), data(env) {}
  cell_c(cell_dict_t dict) : type(cell_type_e::DICT), data(dict) {}

  cell_c() = delete;
  cell_c(const cell_c &other) = delete;
  cell_c(cell_c &&other) = delete;
  cell_c &operator=(const cell_c &other) = delete;
  cell_c &operator=(cell_c &&other) = delete;
  virtual ~cell_c();

  cell_type_e type{cell_type_e::NIL};
  std::any data{0};
  locator_ptr locator{nullptr};

  //! \brief Deep copy the cell
  cell_ptr clone(env_c &env);

  //! \brief Update the cell data and type to match another cell
  //! \param other The other cell to match
  //! \note This will not update the locator
  void update_from(cell_c &other, env_c &env);

  //! \brief Get a copy of the cell value
  //! \throws cell_access_exception_c if the cell is not an integer type
  int64_t to_integer();

  //! \brief Get a reference to the cell value
  //! \throws cell_access_exception_c if the cell is not an integer type
  int64_t &as_integer();

  //! \brief Get a copy of the cell value
  //! \throws cell_access_exception_c if the cell is not a double type
  double to_double();

  //! \brief Get a reference to the cell value
  //! \throws cell_access_exception_c if the cell is not a double type
  double &as_double();

  //! \brief Attempt to convert whatever data type exists to a string
  //!        and return it
  //! \param quote_strings If true, quote strings
  //! \param flatten_complex If true, flatten complex types to just their
  //! symbols (env fn) \throws cell_access_exception_c if the cell can not
  //! access data as its
  //!         listed type
  std::string to_string(bool quote_strings = false,
                        bool flatten_complex = false);

  //! \brief Get the string data as a reference
  //! \throws cell_access_exception_c if the cell is not a string type
  std::string &as_string();

  //! \brief Get the symbol value
  //! \throws cell_access_exception_c if the cell is not a symbol type
  std::string &as_symbol();

  //! \brief Get a copy of the cell value
  //! \throws cell_access_exception_c if the cell is not a list type
  list_info_s to_list_info();

  //! \brief Get a reference to the cell value
  //! \throws cell_access_exception_c if the cell is not a list type
  list_info_s &as_list_info();

  //! \brief Get a copy of the cell value
  //! \throws cell_access_exception_c if the cell is not a list type
  cell_list_t to_list();

  //! \brief Get a reference to the cell value
  //! \throws cell_access_exception_c if the cell is not a list type
  cell_list_t &as_list();

  //! \brief Get a copy of the cell value
  //! \throws cell_access_exception_c if the cell is not an aberrant type
  aberrant_cell_if *as_aberrant();

  //! \brief Get a reference of the cell value
  //! \throws cell_access_exception_c if the cell is not a function type
  function_info_s &as_function_info();

  //! \brief Get a reference of the cell value
  //! \throws cell_access_exception_c if the cell is not an environment type
  environment_info_s &as_environment_info();

  // \brief Get a reference of the cell value
  // \throws cell_access_exception_c if the cell is not a dict type
  cell_dict_t &as_dict();

  //! \brief Check if a cell is a numeric type
  inline bool is_numeric() const {
    return type == cell_type_e::INTEGER || type == cell_type_e::DOUBLE;
  }
};
} // namespace nibi
