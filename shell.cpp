/**
 * Simple shell implementation with piping and redirection capabilities
 * v1.0
 */

// Required libraries
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <list>
#include <optional>

using namespace std;

struct Command {
  vector<string> parts = {};
};

struct Expression {
  vector<Command> commands;
  string inputFromFile;
  string outputToFile;
  bool background = false;
};

// Parses a string to form a vector of arguments. The separator is a space char (' ').
vector<string> split_string(const string& str, char delimiter = ' ') {
  vector<string> retval;
  for (size_t pos = 0; pos < str.length(); ) {
    // look for the next space
    size_t found = str.find(delimiter, pos);
    // if no space was found, this is the last word
    if (found == string::npos) {
      retval.push_back(str.substr(pos));
      break;
    }
    // filter out consequetive spaces
    if (found != pos)
      retval.push_back(str.substr(pos, found-pos));
    pos = found+1;
  }
  return retval;
}

// wrapper around the C execvp so it can be called with C++ strings
int execvp(const vector<string>& args) {
  // build argument list
  const char** c_args = new const char*[args.size()+1];
  for (size_t i = 0; i < args.size(); ++i) {
    c_args[i] = args[i].c_str();
  }
  c_args[args.size()] = nullptr;
  // replace current process with new process as specified
  int rc = ::execvp(c_args[0], const_cast<char**>(c_args));
  // if we got this far, there must be an error
  int error = errno;
  // in case of failure, clean up memory
  delete[] c_args;
  errno = error;
  return rc;
}

// Executes a command with arguments. In case of failure, returns error code.
int execute_command(const Command& cmd) {
  auto& parts = cmd.parts;
  if (parts.size() == 0)
    return EINVAL;

  // execute external commands
  int retval = execvp(parts);
  return retval ? errno : 0;
}

void display_prompt() {
  char buffer[512];
  char* dir = getcwd(buffer, sizeof(buffer));
  if (dir) {
    cout << "\e[32m" << dir << "\e[39m"; // Set color to green then reset to default
  }
  cout << "$ ";
  flush(cout);
}

string get_user_input(bool showPrompt) {
  if (showPrompt) {
    display_prompt();
  }
  string retval;
  getline(cin, retval);
  return retval;
}

Expression parse_command_line(string commandLine) {
  Expression expression;
  vector<string> commands = split_string(commandLine, '|');
  for (size_t i = 0; i < commands.size(); ++i) {
    string& line = commands[i];
    vector<string> args = split_string(line, ' ');
    if (i == commands.size() - 1 && args.size() > 1 && args[args.size()-1] == "&") {
      expression.background = true;
      args.resize(args.size()-1);
    }
    if (i == commands.size() - 1 && args.size() > 2 && args[args.size()-2] == ">") {
      expression.outputToFile = args[args.size()-1];
      args.resize(args.size()-2);
    }
    if (i == 0 && args.size() > 2 && args[args.size()-2] == "<") {
      expression.inputFromFile = args[args.size()-1];
      args.resize(args.size()-2);
    }
    expression.commands.push_back({args});
  }
  return expression;
}

// Navigate to home directory
void goto_home_directory() {
    char* homeDirectory = getenv("HOME");
    if (!homeDirectory) {
        cerr << "Error: Home environment variable is not set" << endl;
        return;
    }

    if (chdir(homeDirectory) != 0) {
        perror("Error: Could not change to home directory");
    }
}

// Handle cd command
void handle_change_directory(vector<string> commands) {
    if(commands.size() < 2) {
      goto_home_directory();
    }
    else { 
        if(commands.at(1) == "~")
            goto_home_directory();
        else if(chdir(commands.at(1).c_str()) != 0 ) {
          string error_message = "bash: cd: " + commands.at(1);
          perror(error_message.c_str());
        }
    }
}

// Handle input file redirection
void handle_input_redirection(string inputFile) {
  int fdInput = open(inputFile.c_str(), O_RDONLY);
  if(fdInput == -1) { 
    cerr << "Input file did not open correctly" << endl;
    exit(errno);
  }
  else if(dup2(fdInput, STDIN_FILENO) == -1) {
    cerr << "File descriptor of input file did not copy correctly" << endl;
    exit(errno);
  }
  close(fdInput);
}

// Handle output file redirection
void handle_output_redirection(string outputFile) {
  int fdOutput = open(outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if(fdOutput == -1) {
    cerr << "Output file did not open correctly" << endl;
    exit(errno);
  }
  else if(dup2(fdOutput, STDOUT_FILENO) == -1) {
    cerr << "File descriptor of output file did not copy correctly" << endl;
    exit(errno);
  }
  close(fdOutput);
}

// Handle pipe commands
int process_pipeline(Expression& expression) {
  int fd[2];
  int previousFd = 0;
  int expSize = expression.commands.size();
  
  for(int i=0; i<expSize; i++) {
    if(i != (expSize-1)) {
      if(pipe(fd) < 0) {
        cerr << "Pipe command failed" << endl;
        return errno;
      }
    }

    pid_t pid = fork();

    if(pid < 0) {
      cerr << "Forking of the process failed" << endl;   
      return errno;
    }

    if (pid == 0) { // Child process
      if (previousFd != 0) {
        dup2(previousFd, STDIN_FILENO);
        close(previousFd);
      }
      
      if(i < (expSize-1)) {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
      }

      // Handle input file for first command
      if(i == 0 && !expression.inputFromFile.empty()) {
        handle_input_redirection(expression.inputFromFile);
      }

      // Handle output file for last command
      if(i == (expSize - 1) && !expression.outputToFile.empty()) {
        handle_output_redirection(expression.outputToFile);
      }

      // Check for internal commands
      if (expression.commands.at(i).parts.at(0) == "cd") {
        cerr << "cd cannot be used in a pipe" << endl;
        exit(errno);
      }
      else if (expression.commands.at(i).parts.at(0) == "exit") {
        cerr << "exit cannot be used in a pipe" << endl;
        exit(errno);
      } 
      else if (execute_command(expression.commands.at(i)) != 0) {
        cerr << "Pipe failed! Invalid command" << endl;
        exit(errno);
      }
    }
    else { // Parent process
      if(!expression.background) {
        waitpid(pid, nullptr, 0);
      } 
      
      if(previousFd != 0) {
        close(previousFd);
      }
      
      if(i < (expSize-1)) {
        close(fd[1]);
        previousFd = fd[0];
      }
    }
  }  
  return 0;
}

// Fix grep command quotation issues
void fix_grep_quotes(Expression& expression) {
  for (Command& cmd : expression.commands) {
    if (cmd.parts.size() > 1 && cmd.parts.at(0) == "grep") {
      for (int j = 1; j < cmd.parts.size(); j++) {
        if (cmd.parts.at(j).front() == '\"' && cmd.parts.at(j).back() == '\"') {
          cmd.parts[j] = cmd.parts[j].substr(1, cmd.parts[j].size() - 2);
        }
      }
    }
  }
}

int execute_expression(Expression& expression) {
  // Check for empty expression
  if (expression.commands.size() == 0)
    return EINVAL;
  
  // Fix issue with grep and quotation marks
  fix_grep_quotes(expression);

  // Handle pipe commands
  if (expression.commands.size() > 1) {
    return process_pipeline(expression);
  }
  
  // Handle internal commands
  vector<string> commands = expression.commands.at(0).parts;

  // Handle exit command
  if(commands.at(0) == "exit") {
      exit(0);
  } 
  // Handle cd command
  else if(commands.at(0) == "cd") { 
      handle_change_directory(commands);
  }
  // External commands
  else {
      pid_t pid = fork();

      if(pid < 0) {
        perror("Fork failed");
      }
      else if(pid == 0) { // Child process
        // Handle input file redirection
        if(!expression.inputFromFile.empty()) {
          handle_input_redirection(expression.inputFromFile);
        }

        // Handle output file redirection
        if(!expression.outputToFile.empty()) {
          handle_output_redirection(expression.outputToFile);
        }

        // Close stdin for background processes without input redirection
        if(expression.background && expression.inputFromFile.empty()) {
          close(STDIN_FILENO);
        }

        if (execute_command(expression.commands.at(0)) != 0) {
            cerr << "Command not found, please insert a valid one" << endl;
            exit(errno);
        }
      }
      else { // Parent process
        if (!expression.background) {
            waitpid(pid, nullptr, 0);
        } else {
            cout << "Background process PID: " << pid << endl;
        }
      }
  }

  return 0;
}

int step1(bool showPrompt) {
  pid_t child1 = fork();
  if (child1 == 0) {
    Command cmd = {{string("date")}};
    execute_command(cmd);
    abort();
  }

  pid_t child2 = fork();
  if (child2 == 0) {
    Command cmd = {{string("tail"), string("-c"), string("5")}};
    execute_command(cmd);
    abort();
  }

  waitpid(child1, nullptr, 0);
  waitpid(child2, nullptr, 0);
  return 0;
}

int shell(bool showPrompt) {
  while (cin.good()) {
    string commandLine = get_user_input(showPrompt);
    Expression expression = parse_command_line(commandLine);
    int rc = execute_expression(expression);
    if (rc != 0)
      cerr << strerror(rc) << endl;
  }
  return 0;
}
