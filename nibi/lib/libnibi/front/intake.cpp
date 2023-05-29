#include "intake.hpp"
#include <regex>

namespace nibi {

static std::regex is_number("[+-]?([0-9]*[.])?[0-9]+");

#define PARSER_ENFORCE_CURRENT_CELL(___message)                                \
  if (!current_list) {                                                         \
    error_cb_(error_c(current_token.get_locator(), ___message));               \
    return nullptr;                                                            \
  }

#define PARSER_ADD_CELL                                                        \
  auto &current_cell_list = current_list->as_list();                           \
  current_cell_list.push_back(cell);                                           \
  return parse(tokens, current_list);

intake_c::intake_c(instruction_processor_if &proc, error_callback_f error_cb,
                   source_manager_c &sm, function_router_t router)
    : processor_(proc), error_cb_(error_cb), sm_(sm), symbol_router_(router) {
  parser_ = std::make_unique<parser_c>(symbol_router_, error_cb_);
}

void intake_c::read(std::string_view source, std::istream &is) {

  std::string line;
  auto source_origin = sm_.get_source(std::string(source));

  bool continue_intake = true;
  while (continue_intake && std::getline(is, line)) {
    tracker_.line_count++;
    continue_intake = process_line(line, source_origin);
  }
  check_for_complete_expression();
}

void intake_c::read_line(std::string_view line,
                         std::shared_ptr<source_origin_c> origin) {
  tracker_.line_count++;
  process_line(line, origin);
  check_for_complete_expression();
}

void intake_c::evaluate(std::string_view data,
                        std::shared_ptr<source_origin_c> origin,
                        locator_ptr location) {
  process_line(data, origin, location);
  check_for_complete_expression();
}

void intake_c::check_for_complete_expression() {
  if (!tracker_.instruction_stack_.empty()) {
    error_cb_(
        error_c(tracker_.instruction_stack_.top(), "Unmatched opening paren"));
  }

  if (!tracker_.data_stack_.empty()) {
    error_cb_(error_c(tracker_.data_stack_.top(), "Unmatched opening bracket"));
  }

  if (!tracker_.access_stack_.empty()) {
    error_cb_(error_c(tracker_.access_stack_.top(), "Unmatched opening brace"));
  }
  if (tokens_.size()) {
    error_cb_(error_c(tokens_.back().get_locator(), "Incomplete expression"));
  }
}

bool intake_c::process_line(std::string_view data,
                            std::shared_ptr<source_origin_c> origin,
                            locator_ptr loc_override) {

  for (std::size_t col = 0; col < data.size(); col++) {
    auto locator =
        (loc_override)
            ? origin->get_locator(tracker_.line_count,
                                  loc_override->get_column() + 1 + col)
            : origin->get_locator(tracker_.line_count, col);
    if (std::isspace(data[col])) {
      continue;
    }
    switch (data[col]) {
    case '#': {
      return true;
    }
    case '(': {
      tracker_.instruction_stack_.push(locator);
      process_token(token_c(locator, token_e::L_PAREN));
      break;
    }
    case ')': {
      if (tracker_.instruction_stack_.empty()) {
        error_cb_(error_c(locator, "Unmatched closing paren"));
        return false;
      }
      tracker_.instruction_stack_.pop();
      process_token(token_c(locator, token_e::R_PAREN));
      break;
    }
    case '[': {
      tracker_.data_stack_.push(locator);
      process_token(token_c(locator, token_e::L_BRACKET));
      break;
    }
    case ']': {
      if (tracker_.data_stack_.empty()) {
        error_cb_(error_c(locator, "Unmatched closing bracket"));
        return false;
      }
      tracker_.data_stack_.pop();
      process_token(token_c(locator, token_e::R_BRACKET));
      break;
    }
    case '{': {
      tracker_.access_stack_.push(locator);
      process_token(token_c(locator, token_e::L_BRACE));
      break;
    }
    case '}': {
      if (tracker_.access_stack_.empty()) {
        error_cb_(error_c(locator, "Unmatched closing brace"));
        return false;
      }
      tracker_.access_stack_.pop();
      process_token(token_c(locator, token_e::R_BRACE));
      break;
    }
    case '"': {
      bool in_str{true};
      std::string value = "\"";
      decltype(col) start = col++;
      while (col < data.size()) {
        if (data[col] == '"') {
          if (col > 0 && data[col - 1] != '\\') {
            value += data[col];
            break;
          }
        }
        value += data[col++];
      }

      if (!value.ends_with('"')) {
        error_cb_(error_c(locator, "Unterminated string"));
        return false;
      }

      // Remove the quotes
      value = value.substr(1, value.size() - 2);

      process_token(token_c(locator, token_e::RAW_STRING, value));
      break;
    }
    default: {

      if (std::isdigit(data[col]) || data[col] == '-') {

        // check for negative number vs subtraction
        if (data[col] == '-' && col + 1 < data.size() &&
            !std::isdigit(data[col + 1])) {
          process_token(token_c(locator, token_e::SYMBOL, "-"));
          break;
        } else if (data[col] == '-' && col == data.size() - 1) {
          process_token(token_c(locator, token_e::SYMBOL, "-"));
          break;
        }

        std::string number;
        number += data[col];
        while (col + 1 < data.size() &&
               (std::isdigit(data[col + 1]) || data[col + 1] == '.')) {
          number += data[col + 1];
          col++;
        }

        if (std::regex_match(number, is_number)) {
          if (number.find('.') != std::string::npos) {
            process_token(token_c(locator, token_e::RAW_FLOAT, number));
          } else {
            process_token(token_c(locator, token_e::RAW_INTEGER, number));
          }
          break;
        } else {
          error_cb_(error_c(locator, "Malformed numerical value"));
          return false;
        }
        break;
      }

      std::string word;
      word += data[col];
      while (col + 1 < data.size() && !std::isspace(data[col + 1]) &&
             data[col + 1] != '(' && data[col + 1] != ')' &&
             data[col + 1] != '[' && data[col + 1] != ']' &&
             data[col + 1] != '}' && data[col + 1] != '}') {
        word += data[col + 1];
        col++;
      }

      process_token(token_c(locator, token_e::SYMBOL, word));
      break;
    }
    }
  }
  return true;
}

void intake_c::process_token(token_c token) {

  tokens_.push_back(token);

  if (!tracker_.instruction_stack_.empty()) {
    return;
  }

  auto instruction = parser_->parse(tokens_);

  if (instruction && instruction->as_list().size()) {
    processor_.instruction_ind(instruction);
  }

  tokens_.clear();
  return;
}

cell_ptr intake_c::parser_c::parse(std::vector<token_c> &tokens) {
  tokens_ = &tokens;
  current_list_.clear();
  index_ = 0;

  cell_ptr list{nullptr};
  list = instruction_list();

  if (!list) {
    error_cb_(error_c(tokens[0].get_locator(), "Invalid instruction list"));
    return nullptr;
  }
  return list;
}

cell_ptr intake_c::parser_c::instruction_list() {
  if (tokens_->empty()) {
    return nullptr;
  }

  if (current_token() != token_e::L_PAREN) {
    return nullptr;
  }

  next();

  cell_list_t list;

  switch (current_token()) {
  case token_e::SYMBOL:
    list.push_back(symbol());
    break;
  case token_e::L_BRACE:
    list.push_back(access_list());
    break;
  case token_e::L_PAREN:
    list.push_back(instruction_list());
    break;
  default:
    error_cb_(error_c(current_location(), "Invalid instruction list"));
    return nullptr;
  }

  auto tracker = current_location();
  while (current_token() != token_e::R_PAREN) {
    cell_ptr next_cell = element();
    if (!next_cell) {
      error_cb_(
          error_c(tracker, "Invalid instruction list - Expected data or list"));
      return nullptr;
    }
    list.push_back(next_cell);
    tracker = current_location();
    if (!has_next()) {
      error_cb_(error_c(tracker,
                        "Invalid instruction - Unexpected end of file/input"));
      return nullptr;
    }
  }

  if (current_token() != token_e::R_PAREN) {
    error_cb_(error_c(current_location(),
                      "Invalid instruction list - Expected closing `)`"));
    return nullptr;
  }

  next();

  return allocate_cell(list_info_s{list_types_e::INSTRUCTION, std::move(list)});
}

cell_ptr intake_c::parser_c::access_list() {
  if (current_token() != token_e::L_BRACE) {
    return nullptr;
  }

  next();

  cell_list_t list;

  auto tracker = current_location();
  while (current_token() != token_e::R_BRACE) {
    cell_ptr next_cell = symbol();
    if (!next_cell) {
      error_cb_(
          error_c(tracker, "Invalid instruction list - Expected data or list"));
      return nullptr;
    }
    list.push_back(next_cell);
    tracker = current_location();
    if (!has_next()) {
      error_cb_(error_c(tracker,
                        "Invalid instruction - Unexpected end of file/input"));
      return nullptr;
    }
  }

  next();

  return allocate_cell(list_info_s{list_types_e::ACCESS, std::move(list)});
}

cell_ptr intake_c::parser_c::data_list() {
  if (current_token() != token_e::L_BRACKET) {
    return nullptr;
  }

  next();

  cell_list_t list;

  auto tracker = current_location();
  while (current_token() != token_e::R_BRACKET) {
    cell_ptr next_cell = element();
    if (!next_cell) {
      error_cb_(
          error_c(tracker, "Invalid instruction list - Expected data or list"));
      return nullptr;
    }
    list.push_back(next_cell);
    tracker = current_location();
    if (!has_next()) {
      error_cb_(error_c(tracker,
                        "Invalid instruction - Unexpected end of file/input"));
      return nullptr;
    }
  }

  next();

  return allocate_cell(list_info_s{list_types_e::DATA, std::move(list)});
}

cell_ptr intake_c::parser_c::list() {

  auto cell = instruction_list();
  if (cell) {
    return cell;
  }

  cell = data_list();
  if (cell) {
    return cell;
  }

  cell = access_list();
  if (cell) {
    return cell;
  }

  return nullptr;
}

cell_ptr intake_c::parser_c::data() {

  auto data = symbol();
  if (data) {
    return data;
  }

  data = number();
  if (data) {
    return data;
  }

  data = string();
  if (data) {
    return data;
  }

  return nullptr;
}

cell_ptr intake_c::parser_c::element() {
  auto element = data();
  if (element) {
    return element;
  }

  element = list();
  if (element) {
    return element;
  }
  return nullptr;
}

cell_ptr intake_c::parser_c::symbol() {

  if (current_token() != token_e::SYMBOL) {
    return nullptr;
  }

  auto symbol_raw = current_data();

  auto router_location = symbol_router_.find(symbol_raw);

  if (router_location == symbol_router_.end()) {
    auto cell = allocate_cell(symbol_s{symbol_raw});
    cell->locator = current_location();

    next();

    return cell;
  }

  auto cell = allocate_cell(router_location->second);
  cell->locator = current_location();

  next();

  return cell;
}

cell_ptr intake_c::parser_c::number() {
  auto num = integer();
  if (num) {
    return num;
  }
  num = real();
  if (num) {
    return num;
  }
  return nullptr;
}

cell_ptr intake_c::parser_c::integer() {

  if (current_token() != token_e::RAW_INTEGER) {
    return nullptr;
  }

  auto stringed_value = current_data();

  int64_t value_actual{0};
  try {
    value_actual = std::stoll(stringed_value);
  } catch (std::exception &e) {
    error_cb_(error_c(current_location(),
                      {"Invalid integer value: " + stringed_value}));
    return nullptr;
  }

  auto cell = allocate_cell((int64_t)value_actual);
  cell->locator = current_location();

  next();

  return cell;
}

cell_ptr intake_c::parser_c::real() {
  if (current_token() != token_e::RAW_FLOAT) {
    return nullptr;
  }

  auto stringed_value = current_data();

  double value_actual{0.00};
  try {
    value_actual = std::stod(stringed_value);
  } catch (std::exception &e) {
    error_cb_(error_c(current_location(),
                      {"Invalid double value: " + stringed_value}));
    return nullptr;
  }

  auto cell = allocate_cell((double)value_actual);
  cell->locator = current_location();

  next();

  return cell;
}

cell_ptr intake_c::parser_c::string() {
  if (current_token() != token_e::RAW_STRING) {
    return nullptr;
  }

  auto cell = allocate_cell(current_data());
  cell->locator = current_location();

  next();

  return cell;
}

} // namespace nibi
