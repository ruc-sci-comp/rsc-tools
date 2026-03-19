#include "rsc_test/configuration.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <print>
#include <regex>
#include <stdexcept>

namespace rsc
{
constexpr char SCHEMA[] = {
#embed "schema.json"
};

auto read_configuration(const std::filesystem::path &test_configuration) -> nlohmann::json
{
    auto config = nlohmann::json::parse(std::ifstream{test_configuration});
    auto validator = nlohmann::json_schema::json_validator{};
    auto schema = nlohmann::json::parse(SCHEMA);
    validator.set_root_schema(schema);
    validator.validate(config);
    return config;
}

auto generate_configuration(const std::filesystem::path &configuration_path) -> void
{
    auto schema = nlohmann::json::parse(SCHEMA);

    struct ctx_t
    {
        const nlohmann::json &root;
        std::filesystem::path base;
        std::unordered_map<std::string, nlohmann::json> cache{};
    } ctx{schema, configuration_path.parent_path()};

    auto resolve_refs = [&](const nlohmann::json &s) -> const nlohmann::json & {
        if (!s.contains("$ref"))
        {
            return s;
        }

        auto ref = s["$ref"].get<std::string>();
        auto pos = ref.find('#');

        auto file = ref.substr(0, pos);
        auto frag = pos == std::string::npos ? "" : ref.substr(pos);

        auto resolve_ptr = [](const nlohmann::json &doc, const std::string &p) -> const nlohmann::json & {
            if (p.empty() || p == "#")
            {
                return doc;
            }
            return doc.at(nlohmann::json::json_pointer(p.substr(1)));
        };

        if (file.empty())
        {
            return resolve_ptr(ctx.root, frag);
        }

        auto path = std::filesystem::weakly_canonical(ctx.base / file);
        auto key = path.string();

        if (!ctx.cache.contains(key))
        {
            auto f = std::ifstream{path};
            if (!f)
            {
                throw std::runtime_error("bad ref: " + key);
            }
            ctx.cache[key] = nlohmann::json::parse(f);
        }

        return resolve_ptr(ctx.cache[key], frag);
    };

    auto generator = [&](auto &&self, const nlohmann::json &schema) -> nlohmann::json {
        auto const &s = resolve_refs(schema);

        if (s.contains("allOf"))
        {
            auto merged = nlohmann::json::object();
            for (auto const &sub : s["allOf"])
            {
                auto tmp = self(self, sub);
                if (tmp.is_object())
                    for (auto &[k, v] : tmp.items())
                        merged[k] = v;
            }
            return merged;
        }

        if (s.contains("default"))
        {
            return s["default"];
        }
        if (s.contains("const"))
        {
            return s["const"];
        }
        if (s.contains("enum") && !s["enum"].empty())
        {
            return s["enum"][0];
        }
        if (!s.contains("type"))
        {
            return nullptr;
        }

        auto const &t = s["type"];
        if (t == "object")
        {
            auto obj = nlohmann::json::object();
            if (s.contains("properties"))
                for (auto const &[k, v] : s["properties"].items())
                    obj[k] = self(self, v);

            return obj;
        }
        if (t == "array")
        {
            auto arr = nlohmann::json::array();
            if (s.contains("items"))
                arr.push_back(self(self, s["items"]));
            return arr;
        }
        if (t == "string")
        {
            return "";
        }
        if (t == "integer")
        {
            return 0;
        }
        if (t == "number")
        {
            return 0.0;
        }
        if (t == "boolean")
        {
            return false;
        }

        return nullptr;
    };

    auto result = generator(generator, schema);
    auto validator = nlohmann::json_schema::json_validator{};
    validator.set_root_schema(schema);
    validator.validate(result);

    auto out = std::ofstream{configuration_path};
    if (!out)
    {
        throw std::runtime_error("write failed");
    }
    out << result.dump(4);
}

auto get_tests(const std::regex &filter_pattern, const nlohmann::json::array_t &tests) -> std::set<std::string>
{
    auto test_names = std::set<std::string>{};
    for (const auto &test : tests)
    {
        auto test_name = test.at("name").get<std::string>();
        if (!std::regex_search(test_name, filter_pattern))
        {
            continue;
        }
        if (test_names.contains(test_name))
        {
            throw std::runtime_error(std::format("Test names must be unique, found multiple \"{}\"", test_name));
        }
        test_names.insert(test_name);
    }
    return test_names;
}

auto cli(const int argc, const char **argv) -> Options
{
    auto app = CLI::App{"rsc-test"};
    auto options = Options{};
    app.add_option("test_configuration", options.test_configuration)->required();
    app.add_option("--filter", options.filter_pattern, "Filter test names using regex pattern");
    app.add_flag("--list", options.list_tests, "List test names without running them");
    app.add_flag("--json", options.json_output, "Output results as JSON");
    app.add_flag("--generate", options.generate, "Generate a new test file");
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        std::exit(app.exit(e));
    }
    catch (const std::regex_error &e)
    {
        std::println(std::cerr, "Invalid regex pattern:\n{}", e.what());
        std::exit(1);
    }
    return options;
}
} // namespace rsc