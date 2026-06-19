#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while (true) {
    std::cout << "$ ";
    std::string command;
    std::getline(std::cin, command);
    std::string input = command;
    if (input.substr(0, 4) == "exit") {
    break;
    }
    else if (input.substr(0, 5) == "echo ") {
        std::cout << input.substr(5) << std::endl;
    }
    else {
        std::cout << command << ": command not found" << std::endl;
    }


  }
}
