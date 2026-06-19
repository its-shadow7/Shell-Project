#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

void typeCommand(std::string input,
                 const std::array<std::string, 10> &built_in_commands) {
  bool found = false;
  for (int i = 0; i < built_in_commands.size(); i++) {
    if (built_in_commands[i] == input.substr(5)) {
      found = true;
      std::cout << input.substr(5) << " is a shell builtin" << std::endl;
      break;
    }
  }
  if (found == false) {
    std::string path = getenv("PATH");
    std::istringstream ss(path);
    std::string directory;
    while (getline(ss, directory, ':')) {
      std::string full_path = directory + "/" + input.substr(5);
      if (!access(full_path.c_str(), X_OK)) {
        std::cout << input.substr(5) << " is " << full_path << std::endl;
        return;
      }
    }
    std::cout << input.substr(5) << ": not found" << std::endl;
  }
  return;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::array<std::string, 10> built_in_commands = {"exit", "echo", "type"};
  std::string command;

  while (true) {
    std::cout << "$ ";
    std::getline(std::cin, command);
    if (command == "exit") {
      break;
    } else if (command.substr(0, 5) == "echo ") {
      std::cout << command.substr(5) << std::endl;
    } else if (command.substr(0, 4) == "type") {
      typeCommand(command, built_in_commands);
    } else {
      std::cout << command << ": command not found" << std::endl;
    }
  }
}