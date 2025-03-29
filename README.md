# Simple Shell

A lightweight shell implementation written in C++ that supports common shell features like command execution, piping, redirection, and background processes.

## Features

- Command execution with arguments
- Input/output redirection using `<` and `>`
- Command piping using `|` to connect multiple commands
- Background process execution using `&`
- Built-in commands:
  - `cd` - Change directory (supports relative paths, absolute paths, and `~` for home directory)
  - `exit` - Exit the shell
  
### Prerequisites

- C++ compiler with C++11 support
- Unix/Linux environment with standard libraries
- CMake (version 3.0 or higher) for building

### Building

```bash
# Clone the repository
cd simple-shell

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make
```

### Running

```bash
./simple-shell
```

## Usage Examples

The shell supports common Unix/Linux commands and shell features:

```bash
# Basic command execution
ls -la

# I/O Redirection
ls -l > file_list.txt
cat < file_list.txt

# Command Piping
ls -l | grep .cpp | sort

# Multiple Pipes
cat /etc/passwd | grep root | cut -d: -f1,6

# Background Processes
sleep 10 &

# Directory Navigation
cd ~
cd /usr/local
cd ..
```

### Architecture

The shell follows a modular design pattern with components for:

- Command parsing and tokenization
- Expression evaluation
- Process management
- I/O redirection
- Pipeline creation and management

### Key Components

- **Expression** struct: Represents a full command with its parts, redirection info, and background status
- **Command** struct: Represents individual commands within an expression
- **Process Pipeline**: Connects multiple commands using pipes, with proper file descriptor management
- **I/O Redirection**: Handles file input/output using system calls

## Limitations

- No command history
- No tab completion
- No job control beyond basic background processes
- No shell scripting capabilities
