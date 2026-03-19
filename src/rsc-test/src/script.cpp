#include "rsc_test/script.hpp"

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <format>

namespace rsc
{
auto run_validation_script(const std::filesystem::path &script, const rsc::RunResult &result,
                           const std::filesystem::path &workdir) -> std::optional<rsc::Diagnostic>
{
    auto L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, script.c_str()) != LUA_OK)
    {
        auto err = std::string(lua_tostring(L, -1));
        lua_close(L);
        throw std::runtime_error(std::format("Lua script error: {}", err));
    }

    lua_getglobal(L, "validate");

    if (!lua_isfunction(L, -1))
    {
        lua_close(L);
        throw std::runtime_error("Lua script must define function validate(stdout, stderr, workdir)");
    }

    lua_pushlstring(L, result.stdout_text.data(), result.stdout_text.size());
    lua_pushlstring(L, result.stderr_text.data(), result.stderr_text.size());
    lua_pushstring(L, workdir.c_str());

    if (lua_pcall(L, 3, 2, 0) != LUA_OK)
    {
        auto err = std::string(lua_tostring(L, -1));
        lua_close(L);
        throw std::runtime_error(std::format("Lua runtime error: {}", err));
    }

    if (!lua_isboolean(L, -2))
    {
        lua_close(L);
        throw std::runtime_error("validate() must at least return a boolean");
    }

    auto passed = lua_toboolean(L, -2);
    auto diag = std::string{};
    if (!passed)
    {
        if (lua_gettop(L) != 2 || !lua_isstring(L, -1))
        {
            lua_close(L);
            auto diag = rsc::Diagnostic{"Unknown validation error, failure reason not provided"};
            return diag;
        }
        else
        {
            auto diag = rsc::Diagnostic{std::format("Script validation failed:{}", lua_tostring(L, -1))};
            return diag;
        }
    }
    lua_close(L);
    return std::nullopt;
}
} // namespace rsc