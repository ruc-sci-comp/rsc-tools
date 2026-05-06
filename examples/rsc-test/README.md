# rsc-test Examples

This directory contains comprehensive examples demonstrating the features of the `rsc-test` tool.

## Prerequisites

First, compile the test program:

```bash
cd examples/rsc-test
g++ -std=c++23 -o simple_program simple_program.cpp
```

## Example Overview

### 01_basic_stdout_test.json
Tests basic stdout output with command-line arguments.

**Features demonstrated:**
- Executable execution with arguments
- Exact stdout matching
- Return code verification

**Run:**
```bash
rsc-test 01_basic_stdout_test.json
```

### 02_stdin_test.json
Tests stdin input to the program.

**Features demonstrated:**
- Providing input via stdin
- Multiple lines of input
- Combined stdout and stdin testing

**Run:**
```bash
rsc-test 02_stdin_test.json
```

### 03_environment_test.json
Tests environment variable passing.

**Features demonstrated:**
- Setting environment variables
- Environment variable access in program
- Exact output matching

**Run:**
```bash
rsc-test 03_environment_test.json
```

### 04_file_creation_test.json
Tests file creation and content checking.

**Features demonstrated:**
- File content verification
- Program file creation
- Working directory isolation

**Run:**
```bash
rsc-test 04_file_creation_test.json
```

### 05_stderr_error_test.json
Tests stderr output and error return codes.

**Features demonstrated:**
- Stderr output verification
- Non-zero return codes
- Error condition testing

**Run:**
```bash
rsc-test 05_stderr_error_test.json
```

### 06_resources_test.json
Tests resource copying to the test working directory.

**Features demonstrated:**
- Resource file copying
- File existence in test environment
- Test isolation

**Run:**
```bash
rsc-test 06_resources_test.json
```

### 07_filter_test.json
Tests the `--filter` option for running specific tests.

**Features demonstrated:**
- Multiple test cases
- Regex filtering
- Test name pattern matching

**Run:**
```bash
# List all tests
rsc-test 07_filter_test.json --list

# Run only tests matching "Filter"
rsc-test 07_filter_test.json --filter "Filter.*"

# Run only tests matching "Another"
rsc-test 07_filter_test.json --filter "Another"
```

## Test Program

The `simple_program.cpp` is a test program that demonstrates various behaviors:

- **Arguments**: Prints command-line arguments
- **Stdin**: Reads and echoes input
- **Environment**: Checks for `TEST_ENV_VAR`
- **File creation**: Creates `output.txt`
- **Error handling**: Returns error code 1 when first argument is "error"

## rsc-test Features

### Basic Test Structure

```json
{
  "tests": [
    {
      "name": "Test name",
      "executable": "./program",
      "input": {
        "argv": ["arg1", "arg2"],
        "stdin": ["input line 1", "input line 2"],
        "env": {
          "VAR_NAME": "value"
        }
      },
      "output": {
        "stdout": {
          "text": "expected output",
          "exact": true
        },
        "stderr": {
          "text": "expected error output",
          "exact": true
        },
        "returncode": 0,
        "files": [
          {
            "test_file": "filename.txt",
            "text": "expected content"
          }
        ]
      },
      "resources": [
        "file1.txt",
        "file2.txt"
      ]
    }
  ]
}
```

### Command Line Options

- `--list`: List test names without running them
- `--filter <pattern>`: Filter tests using regex pattern

### Resource Handling

Resources can be specified in two formats:

**New simplified format:**
```json
"resources": ["file1.txt", "file2.txt"]
```

**Old format (still supported):**
```json
"resources": [
  {"src": "file1.txt", "dst": "file1.txt"},
  {"src": "file2.txt", "dst": "file2.txt"}
]
```

## Testing Tips

1. **Test isolation**: Each test runs in a separate temporary directory
2. **Resource copying**: Use resources to provide input files to tests
3. **Environment variables**: Set up test-specific environment
4. **Error conditions**: Test both success and failure cases
5. **File operations**: Verify file creation and content
6. **Output matching**: Use exact matching for precise verification
7. **Filtering**: Use regex patterns to run specific test subsets

## Example Commands

```bash
# Run all tests in a configuration
rsc-test 01_basic_stdout_test.json

# List available tests
rsc-test 07_filter_test.json --list

# Run filtered tests
rsc-test 07_filter_test.json --filter "Filter.*"