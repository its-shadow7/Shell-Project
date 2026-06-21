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

std::vector<std::string> parse_arguments(const std::string& input) {
  std::vector<std::string> args;
  std::string current_arg = "";
  bool in_single_quote = false;
  bool has_chars = false;

  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (in_single_quote) {
      if (c == '\'') {
        in_single_quote = false;
      } else {
        current_arg += c;
        has_chars = true;
      }
    } else {
      if (c == '\'') {
        in_single_quote = true;
        has_chars = true;
      } else if (c == ' ' || c == '\t') {
        if (has_chars) {
          args.push_back(current_arg);
          current_arg = "";
          has_chars = false;
        }
      } else {
        current_arg += c;
        has_chars = true;
      }
    }
  }
  if (has_chars) {
    args.push_back(current_arg);
  }
  return args;
}

void typeCommand(const std::string& cmd, const std::vector<std::string> &built_in_commands) {
  bool found = false;
  for (size_t i = 0; i < built_in_commands.size(); i++) {
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
  std::vector<std::string> built_in_commands = {"exit", "echo", "type", "pwd", "cd"};
  std::string command;

  while (true) {
    std::cout << "$ ";
    if (!std::getline(std::cin, command)) break;
    if (command.empty()) continue;

    std::vector<std::string> args = parse_arguments(command);
    if (args.empty()) continue;

    std::string cmd_name = args[0];

    if (cmd_name == "exit") {
      break;
    } else if (cmd_name == "echo") {
      for (size_t i = 1; i < args.size(); ++i) {
        std::cout << args[i] << (i + 1 < args.size() ? " " : "");
      }
      std::cout << std::endl;
    } else if (cmd_name == "type") {
      if (args.size() > 1) {
        typeCommand(args[1], built_in_commands);
      }
    } else if (cmd_name == "pwd") {
      std::cout << std::filesystem::current_path().string() << std::endl;
    } else if (cmd_name == "cd") {
      std::string orig_path = "";
      if (args.size() > 1) {
        orig_path = args[1];
      }
      std::string path = orig_path;
      if (path.empty() || path == "~") {
        char* home = getenv("HOME");
        if (home) {
          path = home;
        }
      } else if (path.rfind("~/", 0) == 0) {
        char* home = getenv("HOME");
        if (home) {
          path = std::string(home) + path.substr(1);
        }
      }
      std::error_code ec;
      std::filesystem::current_path(path, ec);
      if (ec) {
        std::cout << "cd: " << orig_path << ": No such file or directory" << std::endl;
      }
    } else {
      std::string exec_path = get_executable_path(cmd_name);
      if (!exec_path.empty()) {
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
        std::cout << cmd_name << ": command not found" << std::endl;
      }
    }
  }
}