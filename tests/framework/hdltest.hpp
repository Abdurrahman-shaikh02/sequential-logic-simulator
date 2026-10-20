#pragma once

// hdltest: a tiny, dependency-free test framework for hdl_sim.
//
//   TEST(suite, name) { CHECK_EQ(1 + 1, 2); }
//   TEST_XFAIL(suite, name, "why it is expected to fail") { ... }
//
// Assertions:
//   CHECK(cond)           CHECK_EQ(a, b)   CHECK_NE(a, b)
//   CHECK_LT/LE/GT/GE     CHECK_CONTAINS(haystack, needle)
//   CHECK_THROWS(expr)    CHECK_NOTHROW(expr)    FAIL(msg)
// Every CHECK_* has a REQUIRE_* twin that stops the test on failure.
// INFO(a << b) adds context (e.g. the current truth-table row) to every
// failure reported while it is in scope.
//
// The runner (framework/main.cpp) runs every test in its own forked process, so
// global simulator state never leaks between tests and crashes/hangs are reported
// as failures instead of killing the whole run. See `run_tests --help`.

#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace hdltest {

// ---------------------------------------------------------------------------
// Value printing: used to show both sides of a failed comparison
// ---------------------------------------------------------------------------

template <class T, class = void>
struct IsStreamable : std::false_type {};
template <class T>
struct IsStreamable<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<const T &>())>>
    : std::true_type {};

template <class T>
std::string toString(const T &v) {
    std::ostringstream os;
    if constexpr (std::is_same_v<T, bool>)
        os << (v ? "true" : "false");
    else if constexpr (std::is_same_v<T, std::string>)
        os << '"' << v << '"';
    else if constexpr (std::is_array_v<T>)
        os << '"' << v << '"';
    else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char *>)
        os << '"' << (v ? v : "(null)") << '"';
    else if constexpr (IsStreamable<T>::value)
        os << v;
    else if constexpr (std::is_enum_v<T>)
        os << static_cast<long long>(v);
    else if constexpr (std::is_pointer_v<T>)
        os << static_cast<const void *>(v);
    else
        os << "<unprintable " << sizeof(T) << "-byte value>";
    return os.str();
}

template <class T>
std::string toString(const std::vector<T> &v) {
    std::string s = "{";
    for (size_t i = 0; i < v.size(); i++)
        s += (i ? ", " : "") + toString(v[i]);
    return s + "}";
}

// Builds a string from a << chain: (Msg() << "row " << r).str()
class Msg {
    std::ostringstream os;
public:
    template <class T>
    Msg &operator<<(const T &v) {
        if constexpr (std::is_enum_v<T> && !IsStreamable<T>::value)
            os << static_cast<long long>(v);
        else
            os << v;
        return *this;
    }
    std::string str() const { return os.str(); }
};

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

struct TestCase {
    std::string suite, name;
    std::function<void()> fn;
    const char *file;
    int line;
    std::string xfail;   // non-empty: test is expected to fail, this is why

    std::string fullName() const { return suite + "." + name; }
};

inline std::vector<TestCase> &registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const char *suite, const char *name, const char *file, int line,
              std::function<void()> fn, const char *xfail = "") {
        registry().push_back({suite, name, std::move(fn), file, line, xfail});
    }
};

// ---------------------------------------------------------------------------
// Per-test state (one test runs at a time per process)
// ---------------------------------------------------------------------------

struct Failure {
    std::string file;
    int line;
    std::string message;
};

struct State {
    std::vector<Failure> failures;
    std::vector<std::string> info;   // INFO() stack
    long long checks = 0;
};

inline State &state() {
    static State s;
    return s;
}

// Thrown by REQUIRE_* / FAIL to end the current test early
struct AbortTest {};

inline void reportFailure(const char *file, int line, const std::string &msg) {
    std::string full = msg;
    for (auto it = state().info.rbegin(); it != state().info.rend(); ++it)
        full += "\n      with: " + *it;
    state().failures.push_back({file, line, full});
}

class ScopedInfo {
public:
    explicit ScopedInfo(std::string s) { state().info.push_back(std::move(s)); }
    ~ScopedInfo() { state().info.pop_back(); }
    ScopedInfo(const ScopedInfo &) = delete;
    ScopedInfo &operator=(const ScopedInfo &) = delete;
};

// Returns true if the check passed
template <class A, class B, class Op>
bool compare(const char *file, int line, const char *ea, const char *eb, const char *opText,
             const A &a, const B &b, Op op) {
    state().checks++;
    if (op(a, b))
        return true;
    reportFailure(file, line,
                  std::string(ea) + " " + opText + " " + eb +
                      "\n      left:  " + toString(a) + "\n      right: " + toString(b));
    return false;
}

inline bool check(const char *file, int line, const char *expr, bool ok) {
    state().checks++;
    if (!ok)
        reportFailure(file, line, std::string(expr) + " is false");
    return ok;
}

template <class H, class N>
bool contains(const char *file, int line, const char *eh, const char *en, const H &hay, const N &needle) {
    state().checks++;
    std::string h(hay), n(needle);
    if (h.find(n) != std::string::npos)
        return true;
    reportFailure(file, line, std::string(eh) + " does not contain " + en +
                                  "\n      looked for: \"" + n + "\"\n      in:         \"" + h + "\"");
    return false;
}

}  // namespace hdltest

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

#define HDLTEST_CAT2(a, b) a##b
#define HDLTEST_CAT(a, b) HDLTEST_CAT2(a, b)
#define HDLTEST_ID(prefix, suite, name) HDLTEST_CAT(prefix, HDLTEST_CAT(suite, HDLTEST_CAT(__, name)))

#define HDLTEST_DEFINE(suite, name, xfail)                                                    \
    static void HDLTEST_ID(hdltest_fn_, suite, name)();                                       \
    static ::hdltest::Registrar HDLTEST_ID(hdltest_reg_, suite, name)(                        \
        #suite, #name, __FILE__, __LINE__, HDLTEST_ID(hdltest_fn_, suite, name), xfail);      \
    static void HDLTEST_ID(hdltest_fn_, suite, name)()

#define TEST(suite, name) HDLTEST_DEFINE(suite, name, "")

// A test for a known bug or missing feature: it is reported as XFAIL while it
// fails, and turns into a failure (XPASS) once it starts passing, so the marker
// gets removed when the bug is fixed.
#define TEST_XFAIL(suite, name, reason) HDLTEST_DEFINE(suite, name, reason)

#define INFO(...) \
    ::hdltest::ScopedInfo HDLTEST_CAT(hdltest_info_, __LINE__)((::hdltest::Msg() << __VA_ARGS__).str())

#define HDLTEST_ABORT_IF_FALSE(ok) \
    do { if (!(ok)) throw ::hdltest::AbortTest{}; } while (0)

#define HDLTEST_CMP(a, b, op, opText) \
    ::hdltest::compare(__FILE__, __LINE__, #a, #b, opText, (a), (b), \
                       [](const auto &x, const auto &y) { return x op y; })

#define CHECK(cond) ((void)::hdltest::check(__FILE__, __LINE__, #cond, static_cast<bool>(cond)))
#define CHECK_EQ(a, b) ((void)HDLTEST_CMP(a, b, ==, "=="))
#define CHECK_NE(a, b) ((void)HDLTEST_CMP(a, b, !=, "!="))
#define CHECK_LT(a, b) ((void)HDLTEST_CMP(a, b, <, "<"))
#define CHECK_LE(a, b) ((void)HDLTEST_CMP(a, b, <=, "<="))
#define CHECK_GT(a, b) ((void)HDLTEST_CMP(a, b, >, ">"))
#define CHECK_GE(a, b) ((void)HDLTEST_CMP(a, b, >=, ">="))
#define CHECK_CONTAINS(h, n) ((void)::hdltest::contains(__FILE__, __LINE__, #h, #n, (h), (n)))

#define REQUIRE(cond) HDLTEST_ABORT_IF_FALSE(::hdltest::check(__FILE__, __LINE__, #cond, static_cast<bool>(cond)))
#define REQUIRE_EQ(a, b) HDLTEST_ABORT_IF_FALSE(HDLTEST_CMP(a, b, ==, "=="))
#define REQUIRE_NE(a, b) HDLTEST_ABORT_IF_FALSE(HDLTEST_CMP(a, b, !=, "!="))
#define REQUIRE_LT(a, b) HDLTEST_ABORT_IF_FALSE(HDLTEST_CMP(a, b, <, "<"))
#define REQUIRE_LE(a, b) HDLTEST_ABORT_IF_FALSE(HDLTEST_CMP(a, b, <=, "<="))
#define REQUIRE_GT(a, b) HDLTEST_ABORT_IF_FALSE(HDLTEST_CMP(a, b, >, ">"))
#define REQUIRE_GE(a, b) HDLTEST_ABORT_IF_FALSE(HDLTEST_CMP(a, b, >=, ">="))
#define REQUIRE_CONTAINS(h, n) HDLTEST_ABORT_IF_FALSE(::hdltest::contains(__FILE__, __LINE__, #h, #n, (h), (n)))

#define FAIL(...)                                                                                     \
    do {                                                                                              \
        ::hdltest::reportFailure(__FILE__, __LINE__, (::hdltest::Msg() << __VA_ARGS__).str());       \
        throw ::hdltest::AbortTest{};                                                                 \
    } while (0)

#define HDLTEST_THROWS(expr, abort)                                                           \
    do {                                                                                      \
        ::hdltest::state().checks++;                                                          \
        bool hdltest_threw = false;                                                           \
        try { (void)(expr); } catch (...) { hdltest_threw = true; }                           \
        if (!hdltest_threw) {                                                                 \
            ::hdltest::reportFailure(__FILE__, __LINE__, "expected " #expr " to throw");      \
            if (abort) throw ::hdltest::AbortTest{};                                          \
        }                                                                                     \
    } while (0)

#define HDLTEST_NOTHROW(expr, abort)                                                          \
    do {                                                                                      \
        ::hdltest::state().checks++;                                                          \
        try { (void)(expr); } catch (const std::exception &hdltest_e) {                       \
            ::hdltest::reportFailure(__FILE__, __LINE__,                                      \
                std::string("expected " #expr " not to throw, got: ") + hdltest_e.what());    \
            if (abort) throw ::hdltest::AbortTest{};                                          \
        } catch (...) {                                                                       \
            ::hdltest::reportFailure(__FILE__, __LINE__, "expected " #expr " not to throw");  \
            if (abort) throw ::hdltest::AbortTest{};                                          \
        }                                                                                     \
    } while (0)

#define CHECK_THROWS(expr) HDLTEST_THROWS(expr, false)
#define REQUIRE_THROWS(expr) HDLTEST_THROWS(expr, true)
#define CHECK_NOTHROW(expr) HDLTEST_NOTHROW(expr, false)
#define REQUIRE_NOTHROW(expr) HDLTEST_NOTHROW(expr, true)
