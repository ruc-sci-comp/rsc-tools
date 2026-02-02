#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <print>

auto main(int argc, char* argv[]) -> int {
    // Test program that demonstrates various behaviors
    
    // Print command line arguments
    std::print("Arguments: ");
    for (auto i = 0; i < argc; ++i) {
        std::print("{}", argv[i]);
        if (i < argc - 1) std::print(" ");
    }
    std::print("\n");
    
    // Read from stdin and echo (with timeout simulation)
    auto input = std::string{};
    if (std::getline(std::cin, input)) {
        if (!input.empty()) {
            std::print("Input: {}\n", input);
        }
    }
    
    // Check environment variable
    const auto* env_val = std::getenv("TEST_ENV_VAR");
    if (env_val) {
        std::print("Env: {}\n", env_val);
    }
    
    // Create a test file
    auto file = std::ofstream{"output.txt"};
    if (file.is_open()) {
        file << "test content" << std::endl;
        file.close();
    }
    
    // Return code based on first argument
    if (argc > 1 && std::string{argv[1]} == "error") {
        std::print(std::cerr, "Error occurred\n");
        return 1;
    }
    
    return 0;
}