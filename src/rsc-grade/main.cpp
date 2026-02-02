#include <CLI/CLI.hpp>
#include <cpp-subprocess/subprocess.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Options
{
    fs::path roster_file{};
    fs::path submissions_dir{};
};

struct Student
{
    std::string name{};
    std::string github_username{};
};

struct TestResult
{
    std::string test_name{};
    bool passed{};
    std::string message{};
};

struct StudentResult
{
    Student student{};
    std::vector<TestResult> test_results{};
    int total_passed{};
    int total_tests{};
};

auto cli(const int argc, const char **argv) -> Options
{
    auto app = CLI::App{"rsc-grade - Grade student submissions"};
    auto options = Options{};

    app.add_option("roster_file", options.roster_file, "Path to roster.json file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("submissions_dir", options.submissions_dir, "Path to directory containing student submissions")
        ->required()
        ->check(CLI::ExistingDirectory);

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        std::exit(app.exit(e));
    }
    return options;
}

auto parse_roster(const fs::path &roster_file) -> std::vector<Student>
{
    auto roster_data = std::ifstream{roster_file};
    auto json_data = nlohmann::json::parse(roster_data);

    auto students = std::vector<Student>{};
    for (const auto &student_json : json_data)
    {
        students.push_back(
            {.name = student_json.value("name", ""), .github_username = student_json.value("github_username", "")});
    }

    return students;
}

auto find_student_submission(const fs::path &submissions_dir, const std::string &github_username) -> fs::path
{
    for (const auto &entry : fs::directory_iterator(submissions_dir))
    {
        if (entry.is_directory())
        {
            auto dir_name = entry.path().filename().string();
            // Check if the directory name contains the GitHub username
            if (dir_name.find(github_username) != std::string::npos)
            {
                return entry.path();
            }
        }
    }
    throw std::runtime_error(
        std::format("Could not find submission directory for student with GitHub username: {}", github_username));
}

auto build_executable(const fs::path &submission_dir) -> bool
{
    try
    {
        // Try to find and run make
        auto make_path = fs::path{"make"};
        if (fs::exists(submission_dir / "Makefile"))
        {
            auto p = subprocess::Popen{std::vector<std::string>{make_path.string()},
                                       subprocess::cwd{submission_dir.string()}, subprocess::output{subprocess::PIPE},
                                       subprocess::error{subprocess::PIPE}};

            auto result = p.communicate();
            auto retcode = p.retcode();

            if (retcode != 0)
            {
                spdlog::error("Build failed for {}: {}", submission_dir.string(),
                             std::string{result.second.buf.data(), result.second.length});
                return false;
            }
            return true;
        }
        else
        {
            spdlog::error("No Makefile found in {}", submission_dir.string());
            return false;
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Build error for {}: {}", submission_dir.string(), e.what());
        return false;
    }
}

auto run_rsc_test(const fs::path &submission_dir, const fs::path &test_config) -> std::vector<TestResult>
{
    auto results = std::vector<TestResult>{};

    try
    {
        // Run rsc-test with the test configuration (assuming it's on PATH)
        auto p = subprocess::Popen{std::vector<std::string>{"rsc-test", test_config.string()},
                                   subprocess::cwd{submission_dir.string()}, subprocess::output{subprocess::PIPE},
                                   subprocess::error{subprocess::PIPE}};

        auto result = p.communicate();
        auto retcode = p.retcode();

        if (retcode != 0)
        {
            throw std::runtime_error(std::format("rsc-test failed with return code {}: {}", retcode,
                                                 std::string{result.second.buf.data(), result.second.length}));
        }

        // Parse the output to extract test results
        auto output = std::string{result.first.buf.data(), result.first.length};
        auto stderr_output = std::string{result.second.buf.data(), result.second.length};

        // Simple parsing of rsc-test output
        // Look for "PASS" and "FAIL" patterns
        auto pos = size_t{0};
        while (pos < output.size())
        {
            auto test_start = output.find("== Running test: ", pos);
            if (test_start == std::string::npos)
                break;

            auto test_end = output.find(" ==", test_start);
            if (test_end == std::string::npos)
                break;

            auto test_name = output.substr(test_start + 17, test_end - (test_start + 17));

            auto result_pos = output.find("\n", test_end);
            if (result_pos == std::string::npos)
                break;

            auto result_line = output.substr(result_pos + 1);
            auto next_test = result_line.find("== Running test: ");

            auto passed = result_line.find("PASS") != std::string::npos;
            auto message = std::string{};

            if (passed)
            {
                message = "Test passed";
            }
            else
            {
                // Extract failure details
                auto fail_start = result_line.find("FAIL");
                auto fail_end = result_line.find("\n\n", fail_start);
                if (fail_end != std::string::npos)
                {
                    message = result_line.substr(fail_start, fail_end - fail_start);
                }
                else
                {
                    message = result_line.substr(fail_start);
                }
            }

            results.push_back({.test_name = test_name, .passed = passed, .message = message});

            pos = test_start + 1;
        }
    }
    catch (const std::exception &e)
    {
        results.push_back({.test_name = "Test Execution",
                           .passed = false,
                           .message = std::format("Failed to run tests: {}", e.what())});
    }

    return results;
}

auto grade_student(const fs::path &submissions_dir, const Student &student) -> StudentResult
{
    auto student_result = StudentResult{};
    student_result.student = student;

    try
    {
        // Find student's submission directory
        auto submission_dir = find_student_submission(submissions_dir, student.github_username);
        spdlog::info("Grading {} ({})", student.name, student.github_username);

        // Find test configuration (contest.yaml)
        auto test_config = submission_dir / "contest.yaml";
        if (!fs::exists(test_config))
        {
            spdlog::error("No contest.yaml found in {}", submission_dir.string());
            student_result.test_results.push_back(
                {.test_name = "Configuration", .passed = false, .message = "No contest.yaml configuration file found"});
            return student_result;
        }

        // Build the executable
        if (!build_executable(submission_dir))
        {
            student_result.test_results.push_back(
                {.test_name = "Build", .passed = false, .message = "Failed to build executable"});
            return student_result;
        }

        // Run tests using rsc-test
        auto test_results = run_rsc_test(submission_dir, test_config);

        for (const auto &test_result : test_results)
        {
            student_result.test_results.push_back(test_result);

            if (test_result.passed)
            {
                student_result.total_passed++;
            }
            student_result.total_tests++;
        }
    }
    catch (const std::exception &e)
    {
        student_result.test_results.push_back(
            {.test_name = "Grading", .passed = false, .message = std::format("Grading failed: {}", e.what())});
    }

    return student_result;
}

auto run(const Options &options) -> void
{
    // Parse roster
    auto students = parse_roster(options.roster_file);
    spdlog::info("Found {} students in roster", students.size());

    // Grade each student
    auto results = std::vector<StudentResult>{};
    for (const auto &student : students)
    {
        results.push_back(grade_student(options.submissions_dir, student));
    }

    // Print summary
    spdlog::info("\n=== GRADING SUMMARY ===");
    for (const auto &result : results)
    {
        spdlog::info("{}: {}/{} tests passed", result.student.name, result.total_passed, result.total_tests);

        // Print detailed results for failed tests
        for (const auto &test_result : result.test_results)
        {
            if (!test_result.passed)
            {
                spdlog::error("  FAILED: {}", test_result.test_name);
                spdlog::error("    {}", test_result.message);
            }
        }
    }
}

auto main(const int argc, const char **argv) -> int
{
    auto options = cli(argc, argv);
    run(options);
    return 0;
}
