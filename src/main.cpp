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
#include <fstream>
#include <fcntl.h>

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
  bool in_double_quote = false;
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
    } else if (in_double_quote) {
      if (c == '\\') {
        if (i + 1 < input.size()) {
          char next = input[i + 1];
          if (next == '"' || next == '\\' || next == '$' || next == '`') {
            current_arg += next;
            has_chars = true;
            ++i;
          } else {
            current_arg += c;
            has_chars = true;
          }
        } else {
          current_arg += c;
          has_chars = true;
        }
      } else if (c == '"') {
        in_double_quote = false;
      } else {
        current_arg += c;
        has_chars = true;
      }
    } else {
      if (c == '\\') {
        if (i + 1 < input.size()) {
          current_arg += input[i + 1];
          has_chars = true;
          ++i;
        }
      } else if (c == '\'') {
        in_single_quote = true;
        has_chars = true;
      } else if (c == '"') {
        in_double_quote = true;
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

struct RedirectionInfo {
  bool has_redirect = false;
  size_t op_start = std::string::npos;
  size_t op_len = 0;
};

RedirectionInfo find_redirection(const std::string& input) {
  std::vector<bool> is_unquoted(input.size(), false);
  bool in_single_quote = false;
  bool in_double_quote = false;

  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (in_single_quote) {
      if (c == '\'') {
        in_single_quote = false;
      }
    } else if (in_double_quote) {
      if (c == '\\') {
        if (i + 1 < input.size()) {
          char next = input[i + 1];
          if (next == '"' || next == '\\' || next == '$' || next == '`') {
            ++i;
          }
        }
      } else if (c == '"') {
        in_double_quote = false;
      }
    } else {
      if (c == '\\') {
        if (i + 1 < input.size()) {
          ++i;
        }
      } else if (c == '\'') {
        in_single_quote = true;
      } else if (c == '"') {
        in_double_quote = true;
      } else {
        is_unquoted[i] = true;
      }
    }
  }

  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '>' && is_unquoted[i]) {
      RedirectionInfo info;
      info.has_redirect = true;
      if (i > 0 && input[i - 1] == '1' && is_unquoted[i - 1]) {
        info.op_start = i - 1;
        info.op_len = 2;
      } else {
        info.op_start = i;
        info.op_len = 1;
      }
      return info;
    }
  }

  return {};
}

struct CoutRedirector {
  std::streambuf* org_buf;
  std::ofstream out_file;
  bool active;

  CoutRedirector(bool active, const std::string& filepath) : active(active), org_buf(nullptr) {
    if (active) {
      out_file.open(filepath, std::ios::out | std::ios::trunc);
      if (out_file.is_open()) {
        org_buf = std::cout.rdbuf();
        std::cout.rdbuf(out_file.rdbuf());
      }
    }
  }

  ~CoutRedirector() {
    if (active && org_buf != nullptr) {
      std::cout.rdbuf(org_buf);
    }
  }
};

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::vector<std::string> built_in_commands = {"exit", "echo", "type", "pwd", "cd"};
  std::string command;

  while (true) {
    std::cout << "$ ";
    if (!std::getline(std::cin, command)) break;
    if (command.empty()) continue;

    bool do_redirect = false;
    std::string redirect_file = "";
    std::vector<std::string> args;

    RedirectionInfo redir = find_redirection(command);
    if (redir.has_redirect) {
      std::string left_part = command.substr(0, redir.op_start);
      std::string right_part = command.substr(redir.op_start + redir.op_len);
      std::vector<std::string> left_args = parse_arguments(left_part);
      std::vector<std::string> right_args = parse_arguments(right_part);
      if (!right_args.empty()) {
        redirect_file = right_args[0];
        do_redirect = true;
        args = left_args;
        for (size_t i = 1; i < right_args.size(); ++i) {
          args.push_back(right_args[i]);
        }
      } else {
        args = parse_arguments(command);
      }
    } else {
      args = parse_arguments(command);
    }

    if (args.empty()) continue;

    if (do_redirect) {
      std::filesystem::path p(redirect_file);
      if (!p.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
      }
    }

    std::string cmd_name = args[0];

    bool is_builtin = false;
    for (const auto& built_in : built_in_commands) {
      if (cmd_name == built_in) {
        is_builtin = true;
        break;
      }
    }

    if (is_builtin) {
      CoutRedirector redirect(do_redirect, redirect_file);
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
          std::cerr << "cd: " << orig_path << ": No such file or directory" << std::endl;
        }
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
          if (do_redirect) {
            int fd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
              dup2(fd, STDOUT_FILENO);
              close(fd);
            } else {
              perror("open");
              exit(1);
            }
          }
          execv(exec_path.c_str(), c_args.data());
          exit(1); // exit if execv fails
        } else if (pid > 0) {
          int status;
          waitpid(pid, &status, 0);
        }
      } else {
        std::cerr << cmd_name << ": command not found" << std::endl;
      }
    }
  }
}