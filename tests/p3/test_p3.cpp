// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ---------------------------------------------------------------- registry

TEST(reg_starts_empty) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("calculator") == nullptr);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);

    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
}

TEST(reg_find_absent_name) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("read_file")));
    REQUIRE(r.add(std::make_unique<NamedTool>("write_file")));
    CHECK(r.find("list_dir") == nullptr);
    CHECK(r.find("") == nullptr);
    CHECK(r.find("read_fil") == nullptr);
    CHECK(r.find("read_file ") == nullptr);
    CHECK(r.size() == 2);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    std::vector<std::string> names;
    for (int i = 0; i < 50; ++i) {
        names.push_back("tool_" + std::to_string(i));
        REQUIRE(r.add(std::make_unique<NamedTool>(names.back())));
    }

    std::vector<int> visits(names.size(), 0);
    int total = 0;
    r.for_each([&](const Tool& tool) {
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (names[i] == tool.name()) ++visits[i];
        }
        ++total;
    });

    CHECK(total == 50);
    for (int count : visits) CHECK(count == 1);
}

TEST(reg_resize_doubles_and_keeps_tools) {
    ToolRegistry r;
    std::size_t buckets = r.bucket_count();
    for (int i = 0; i < 100; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("t" + std::to_string(i))));
        CHECK(r.load_factor() <= 1.0);
        if (r.bucket_count() != buckets) {
            CHECK(r.bucket_count() == buckets * 2);
            buckets = r.bucket_count();
        }
    }
    for (int i = 0; i < 100; ++i) {
        Tool* t = r.find("t" + std::to_string(i));
        REQUIRE(t != nullptr);
        CHECK(t->name() == "t" + std::to_string(i));
    }
}

TEST(reg_hash_uses_every_char_in_order) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    CHECK(ToolRegistry::hash("calculator") != ToolRegistry::hash("calculatoR"));
    CHECK(ToolRegistry::hash("calculator") == ToolRegistry::hash("calculator"));

    std::vector<std::size_t> hashes;
    for (int i = 0; i < 10000; ++i) hashes.push_back(ToolRegistry::hash("tool_" + std::to_string(i)));
    std::sort(hashes.begin(), hashes.end());
    std::size_t distinct = std::unique(hashes.begin(), hashes.end()) - hashes.begin();
    CHECK(distinct >= 9990);
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("tool_0");
    Tool* first_ptr = first.get();
    REQUIRE(r.add(std::move(first)));

    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
        CHECK(r.load_factor() <= 1.0);
    }

    CHECK(r.size() == 10000);
    CHECK(r.find("tool_0") == first_ptr);
    for (int i = 0; i < 10000; ++i) {
        CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    }
    CHECK(r.max_probe_length() <= 64);

    std::printf("    FNV-1a:          bucket_count=%zu load_factor=%.3f max_probe_length=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    }
    std::size_t buckets = r.bucket_count();

    std::vector<int> counts(buckets, 0);
    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);
        std::size_t weak = static_cast<unsigned char>(name[0]) + name.size();
        ++counts[weak & (buckets - 1)];
    }
    std::size_t weak_longest = static_cast<std::size_t>(*std::max_element(counts.begin(), counts.end()));

    std::printf("    s[0]+s.size():   bucket_count=%zu longest chain=%zu\n", buckets, weak_longest);
    std::printf("    FNV-1a:          bucket_count=%zu longest chain=%zu\n", buckets, r.max_probe_length());
    CHECK(weak_longest > 64);
    CHECK(r.max_probe_length() < weak_longest);
}

// --------------------------------------------------------------- workspace

TEST(ws_accepts_safe_paths) {
    TempDir d;
    Workspace ws(d.path());
    struct Row { const char* input; fs::path expected; };
    const Row rows[] = {
        {"notes/todo.txt",    ws.root() / "notes" / "todo.txt"},
        {"./notes//todo.txt", ws.root() / "notes" / "todo.txt"},
        {".",                 ws.root()},
        {"a/../b",            ws.root() / "b"},
        {"a/..",              ws.root()},
        {"new/dir/file.txt",  ws.root() / "new" / "dir" / "file.txt"},
        {"my..notes.txt",     ws.root() / "my..notes.txt"},
    };
    for (const Row& row : rows) {
        fs::path out;
        std::string reason;
        CHECK(ws.resolve(row.input, &out, &reason));
        CHECK_EQ(out, row.expected);
    }
}

TEST(ws_rejects_unsafe_paths) {
    TempDir d;
    Workspace ws(d.path());
    const std::string inside = ws.root().filename().string();
    const std::vector<std::string> inputs = {
        "",
        "/etc/passwd",
        "..",
        "../etc/passwd",
        "a/../../b",
        "notes/../../" + inside + "/notes/todo.txt",
    };
    for (const std::string& input : inputs) {
        fs::path out;
        std::string reason;
        CHECK(!ws.resolve(input, &out, &reason));
        CHECK(!reason.empty());
    }
}

TEST(ws_rejects_nul_byte) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    std::string with_nul = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(with_nul, &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_absolute_path_inside_root_rejected) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve((ws.root() / "notes" / "todo.txt").string(), &out, &reason));
    CHECK(!reason.empty());
}

// -------------------------------------------------------------- calculator

TEST(calc_left_to_right) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}

TEST(calc_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
}

TEST(calc_negative_numbers) {
    double v = 0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
}

TEST(calc_decimals) {
    double v = 0;
    std::string err;
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
}

TEST(calc_division_by_zero_in_parentheses) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_own_expressions) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 * (3 + 4) * (5 - 1) / 7", &v, &err));
    CHECK(v == 8);
    REQUIRE(eval("1.5 + 2.25 * -2", &v, &err));
    CHECK(v == -3);
    REQUIRE(eval("((((6))))", &v, &err));
    CHECK(v == 6);
    REQUIRE(eval("8 / 4 * 2", &v, &err));
    CHECK(v == 4);
}

TEST(calc_very_long_expressions) {
    double v = 0;
    std::string err;

    std::string sum = "1";
    for (int i = 1; i < 20000; ++i) sum += "+1";
    REQUIRE(eval(sum, &v, &err));
    CHECK(v == 20000);

    std::string nested = std::string(5000, '(') + "7" + std::string(5000, ')');
    REQUIRE(eval(nested, &v, &err));
    CHECK(v == 7);
}

TEST(calc_malformed_tokens_do_not_crash) {
    using K = CalcToken::Kind;
    double v = 0;
    std::string err;

    CHECK(!calc_evaluate({}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({CalcToken{K::Plus, 0}}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({CalcToken{K::RParen, 0}}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({CalcToken{K::LParen, 0}, CalcToken{K::Number, 1}}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({CalcToken{K::Number, 1}, CalcToken{K::Number, 2}}, &v, &err));
    CHECK(!err.empty());
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
