// Temporary compatibility fallback.
// Replace this file with official Catch2 amalgamated header by running:
//   powershell -ExecutionPolicy Bypass -File scripts/fetch_catch2.ps1
#pragma once

#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Catch {

struct TestCase
{
    const char* Name;
    const char* Tags;
    void (*Fn)();
};

inline std::vector<TestCase>& Registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct AutoReg
{
    AutoReg(const char* name, const char* tags, void (*fn)())
    {
        Registry().push_back({name, tags, fn});
    }
};

class Approx
{
public:
    explicit Approx(double value)
        : m_Value(value)
    {
    }

    Approx margin(double marginValue) const
    {
        Approx copy(*this);
        copy.m_Margin = std::fabs(marginValue);
        return copy;
    }

    bool Compare(double value) const
    {
        return std::fabs(value - m_Value) <= m_Margin;
    }

private:
    double m_Value = 0.0;
    double m_Margin = 1e-12;

    friend bool operator==(double lhs, const Approx& rhs)
    {
        return rhs.Compare(lhs);
    }

    friend bool operator==(const Approx& lhs, double rhs)
    {
        return lhs.Compare(rhs);
    }
};

inline std::string BuildFailureMessage(const char* expr, const char* file, int line)
{
    std::ostringstream oss;
    oss << file << ":" << line << " REQUIRE failed: " << expr;
    return oss.str();
}

class Session
{
public:
    int run(int argc, char* argv[])
    {
        std::string filter;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (!arg.empty() && arg[0] != '-')
                filter = arg;
        }

        int failed = 0;
        int executed = 0;

        for (const TestCase& t : Registry())
        {
            if (!filter.empty())
            {
                const std::string name = t.Name ? t.Name : "";
                const std::string tags = t.Tags ? t.Tags : "";
                if (name.find(filter) == std::string::npos && tags.find(filter) == std::string::npos)
                    continue;
            }

            ++executed;
            try
            {
                t.Fn();
                std::cout << "[PASS] " << (t.Name ? t.Name : "<unnamed>") << "\n";
            }
            catch (const std::exception& ex)
            {
                ++failed;
                std::cout << "[FAIL] " << (t.Name ? t.Name : "<unnamed>") << "\n";
                std::cout << "       " << ex.what() << "\n";
            }
            catch (...)
            {
                ++failed;
                std::cout << "[FAIL] " << (t.Name ? t.Name : "<unnamed>") << "\n";
                std::cout << "       unknown exception\n";
            }
        }

        std::cout << "Executed: " << executed << ", Failed: " << failed << "\n";
        return failed;
    }
};

} // namespace Catch

#define CATCH_INTERNAL_CAT_IMPL(a, b) a##b
#define CATCH_INTERNAL_CAT(a, b) CATCH_INTERNAL_CAT_IMPL(a, b)

#define TEST_CASE(name, tags) \
    static void CATCH_INTERNAL_CAT(catch_test_fn_, __LINE__)(); \
    static ::Catch::AutoReg CATCH_INTERNAL_CAT(catch_test_reg_, __LINE__)(name, tags, &CATCH_INTERNAL_CAT(catch_test_fn_, __LINE__)); \
    static void CATCH_INTERNAL_CAT(catch_test_fn_, __LINE__)()

#define REQUIRE(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error(::Catch::BuildFailureMessage(#expr, __FILE__, __LINE__)); \
        } \
    } while (0)
