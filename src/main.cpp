#include <array>
#include <cstddef>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

std::string get_executable_path(const std::string& command_name) {
  char* path_env = getenv("PATH");
  if (!path_env) return "";
  std::istringstream ss(path_env);
  std::string directory;
  while (std::getline(ss, directory, ':')) {
    std::string full_path = directory + "/" + command_name;
    if (!access(full_path.c_str(), X_OK)) {
      return full_path;
    }
  }
  return "";
}

void typeCommand(std::string input, const std::array<std::string, 10> &built_in_commands) {
  if (input.length() <= 5) return;
  std::string cmd = input.substr(5);
  bool found = false;
  for (int i = 0; i < built_in_commands.size(); i++) {
    if (built_in_commands[i] == cmd) {
      found = true;
      std::cout << cmd << " is a shell builtin" << std::endl;
      break;
    }
  }
  if (!found) {
    std::string path = get_executable_path(cmd);
    if (!path.empty()) {
      std::cout << cmd << " is " << path << std::endl;
    } else {
      std::cout << cmd << ": not found" << std::endl;
    }
  }
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::array<std::string, 10> built_in_commands = {"exit", "echo", "type", "pwd", "cd"};
  std::string command;

  while (true) {
    std::cout << "$ ";
    if (!std::getline(std::cin, command)) break;
    if (command.empty()) continue;

    if (command == "exit") {
      break;
    } else if (command.substr(0, 5) == "echo ") {
      std::cout << command.substr(5) << std::endl;
    } else if (command.substr(0, 5) == "type ") {
      typeCommand(command, built_in_commands);
    } else if (command == "pwd") {
      std::cout << std::filesystem::current_path().string() << std::endl;
    } else if (command.substr(0, 3) == "cd ") {
      std::string path = command.substr(3);
      std::error_code ec;
      std::filesystem::current_path(path, ec);
      if (ec) {
        std::cout << "cd: " << path << ": No such file or directory" << std::endl;
      }
    } else if (command == "cd") {
      char* home = getenv("HOME");
      if (home) {
        std::error_code ec;
        std::filesystem::current_path(home, ec);
      }
    } else {
      std::string cmd_name = command.substr(0, command.find(' '));
      std::string exec_path = get_executable_path(cmd_name);
      
      if (!exec_path.empty()) {
        std::vector<std::string> args;
        std::istringstream iss(command);
        std::string arg;
        while (iss >> arg) {
          args.push_back(arg);
        }
        
        std::vector<char*> c_args;
        for (auto& a : args) {
          c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);
        
        pid_t pid = fork();
        if (pid == 0) {
          // child process
          execv(exec_path.c_str(), c_args.data());
          exit(1); // exit if execv fails
        } else if (pid > 0) {
          
          int status;
          waitpid(pid, &status, 0);
        }
      } else {
        std::cout << command << ": command not found" << std::endl;
      }
    }
  }
}