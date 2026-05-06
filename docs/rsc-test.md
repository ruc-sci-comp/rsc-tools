# rsc-test Tool Documentation

## Overview

The `rsc-test` tool is a comprehensive automated testing framework for command-line programs. It executes programs with specified inputs and validates their outputs against expected results defined in JSON configuration files. The tool supports testing scenarios including command-line arguments, environment variables, stdin input, and file system state validation.

## Build Instructions

### Prerequisites

- C++20 compatible compiler
- CMake 3.20 or later
- Conan package manager (for dependency management)

### Dependencies

The tool requires the following libraries:
- **CLI11**: Command-line interface parsing
- **nlohmann_json**: JSON processing and manipulation
- **spdlog**: Logging framework
- **cpp-subprocess**: Cross-platform subprocess execution

### Build Process

1. **Clone the repository:**
   ```bash
   git clone <repository-url>
   cd rsc-tools
   ```

2. **Build using the provided Makefile:**
   ```bash
   make
   ```

The build will automatically install the executable to your system path.

## Usage

### Basic Syntax

```bash
rsc-test <test-configuration-file> [options]
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `--filter <pattern>` | Filter test names using a regex pattern |
| `--list` | List test names without running them |
| `--help` | Show help message |

### Examples

1. **Run all tests from a configuration file:**
   ```bash
   rsc-test tests/config.json
   ```

2. **List all available tests:**
   ```bash
   rsc-test tests/config.json --list
   ```

3. **Run only tests matching a pattern:**
   ```bash
   rsc-test tests/config.json --filter "test.*"
   ```

## JSON Configuration Structure

### Root Structure

```json
{
    "tests": [
        {
            "name": "Test Name",
            "executable": "path/to/executable",
            "input": { ... },
            "output": { ... },
            "resources": [ ... ]
        }
    ]
}
```

### Test Object Properties

#### Required Properties

- **`name`** (string): Human-readable name for the test
- **`executable`** (string): Path to the executable to test (relative to test file location)

#### Optional Properties

- **`input`** (object): Input configuration for the test
- **`output`** (object): Expected output validation rules
- **`resources`** (array): Files/directories to copy for test execution

### Input Configuration

The `input` object specifies how to run the test program:

```json
{
    "input": {
        "argv": ["arg1", "arg2"],
        "env": {
            "VAR1": "value1",
            "VAR2": "value2"
        },
        "stdin": [
            "line 1 input",
            "line 2 input"
        ]
    }
}
```

#### Input Properties

- **`argv`** (array): Command-line arguments to pass to the executable
- **`env`** (object): Environment variables to set for the test
- **`stdin`** (array): Lines of input to send to the program's stdin

### Output Validation

The `output` object defines expected results:

```json
{
    "output": {
        "returncode": 0,
        "stdout": {
            "text": "Expected output",
            "exact": false,
            "empty": false
        },
        "stderr": {
            "text": "Expected error output",
            "exact": true
        },
        "files": [
            {
                "test_file": "output.txt",
                "text": "expected content"
            }
        ]
    }
}
```

#### Output Properties

- **`returncode`** (integer): Expected exit code (default: 0)
- **`stdout`** (object): Expected stdout output validation
- **`stderr`** (object): Expected stderr output validation
- **`files`** (array): File content checks

#### Stream Validation Options

For `stdout`, `stderr`, and `files` objects:

- **`text`** (string): Expected output text
- **`from_file`** (string): Path to a file containing the expected output
- **`exact`** (boolean): Whether to perform exact matching (default: false)
  - When false, `\r` is stripped and trailing whitespace is trimmed
- **`empty`** (boolean): Whether the output should be empty

#### File Validation

Each file object in the `files` array:

- **`test_file`** (string): Path to check (relative to test working directory)
- Plus any one of the stream validation options above to specify expected content

### Resources Configuration

The `resources` array specifies files to copy for test execution:

```json
{
    "resources": [
        "path/to/resource/file.txt",
        {
            "src": "source/path",
            "dst": "destination/path"
        }
    ]
}
```

#### Resource Formats

- **String format**: Copy file/directory to same location in working directory
- **Object format**: Copy from `src` to `dst` (relative to working directory)

## Example Configuration

```json
{
    "tests": [
        {
            "name": "Basic arithmetic test",
            "executable": "./calculator",
            "input": {
                "argv": ["add", "5", "3"],
                "stdin": [],
                "env": {
                    "CALC_MODE": "decimal"
                }
            },
            "output": {
                "returncode": 0,
                "stdout": {
                    "text": "Result: 8",
                    "exact": true
                },
                "stderr": {
                    "empty": true
                },
                "files": [
                    {
                        "test_file": "calc.log",
                        "text": "Result: 8"
                    }
                ]
            },
            "resources": [
                "test-data/input.txt",
                {
                    "src": "templates/config.json",
                    "dst": "config.json"
                }
            ]
        }
    ]
}
```

## Working Directory and Execution

### Temporary Working Directory

Each test runs in a temporary directory (`/tmp/rsc-test` on Unix systems). The tool:

1. Creates a clean temporary directory
2. Copies specified resources
3. Creates symlinks to the executable
4. Executes the program with specified inputs
5. Validates outputs against expectations
6. Cleans up the temporary directory

### Executable Path Handling

- **Relative paths**: Resolved relative to the test configuration file location
- **Absolute paths**: Used as-is
- **Symlinks**: Created in the working directory to maintain original executable name

## Output Formats

### Output Format

```
Running test: Basic arithmetic test
PASS

Running test: File processing test
FAIL
  return code mismatch:
      expected: 0
      received: 1
  stdout mismatch at line 1:
      expected: "Processing complete"
      received: "Error: File not found"
```
