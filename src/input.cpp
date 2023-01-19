
#include "input.h"
#include "InputParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <variant>

namespace spright {

std::vector<Sprite> parse_definition(const Settings& settings) {
  auto parser = InputParser(settings);

  if (!settings.input.empty()) {
    auto input = std::stringstream(settings.input);
    parser.parse(input);
  }

  for (const auto& input_file : settings.input_files) {

    if (input_file == "stdin") {
      parser.parse(std::cin);
      continue;
    }

    auto input = std::fstream(input_file, std::ios::in | std::ios::binary);
    if (!input.good())
      throw std::runtime_error("opening file '" + path_to_utf8(input_file) + "' failed");
    try {
      parser.parse(input);
    }
    catch (const std::exception& ex) {
      throw std::runtime_error(
        "'" + path_to_utf8(input_file) + "' " + ex.what());
    }
    input.close();

    if (settings.autocomplete)
      update_textfile(input_file, parser.autocomplete_output());
  }

  return std::move(parser).sprites();
}

} // namespace
