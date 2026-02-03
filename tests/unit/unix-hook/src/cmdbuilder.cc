#include <boost/ut.hpp>
#include <filesystem>
#include <string_view>
#include <vector>
#include <span>
#include "command.h"
#include "session.h"

// Assuming this header is available as per previous context
#include "opt-data/catter-proxy/parser.h"

namespace ut = boost::ut;
namespace ct = catter;

ut::suite<"CmdBuilder"> cmdbuilder_test = [] {
    using namespace ut;

    ct::Session session;
    session.proxy_path = "/usr/local/bin/catter-proxy";
    session.self_id = "session-99";

    ct::CmdBuilder builder(session);

    test("proxy_cmd constructs correct arguments") = [&] {
        std::filesystem::path target_path = "/usr/bin/gcc";

        // FIX: Use vector<char*> and const_cast to match ArgvRef (span<char* const>) signature
        // This is safe in tests because proxy_cmd only reads the data.
        std::vector<char*> original_argv = {const_cast<char*>("gcc"),
                                            const_cast<char*>("-c"),
                                            const_cast<char*>("main.c"),
                                            nullptr};

        // Note: The ArgvRef passed here should include the original argv[0]
        auto cmd = builder.proxy_cmd(
            target_path,
            std::span<char* const>{original_argv.data(), original_argv.size() - 1});

        // 1. Verify basic properties
        expect(cmd.path == session.proxy_path);

        // 2. Verify argv[0] convention
        expect(cmd.argv.at(0) == session.proxy_path);

        // 3. Use parse_opt to verify the generated command line logic
        // Need to convert vector<string> to char* const[] for the parser
        std::vector<char*> mock_argv;
        for(const auto& s: cmd.argv)
            mock_argv.push_back(const_cast<char*>(s.c_str()));
        mock_argv.push_back(nullptr);

        auto parse_res = ct::optdata::catter_proxy::parse_opt(mock_argv.data());

        expect(parse_res.has_value());
        if(parse_res.has_value()) {
            expect(parse_res->parent_id == "session-99");
            expect(parse_res->executable == target_path);

            expect(parse_res->raw_argv_or_err.has_value());
            if(parse_res->raw_argv_or_err.has_value()) {
                auto& args = parse_res->raw_argv_or_err.value();
                expect(args.size() == 3);
                expect(args.at(0) == "gcc");
                expect(args.at(2) == "main.c");
            }
        }
    };

    test("error_cmd formats message correctly without separator") = [&] {
        std::filesystem::path target_path = "/usr/bin/invalid";

        // FIX: Use vector<char*> and const_cast
        std::vector<char*> original_argv = {const_cast<char*>("invalid"),
                                            const_cast<char*>("--help"),
                                            nullptr};
        const char* error_msg = "File not found";

        auto cmd = builder.error_cmd(
            error_msg,
            target_path,
            std::span<char* const>{original_argv.data(), original_argv.size() - 1});

        // Verify structure: proxy -p ID --exec PATH "Error Message"
        // error_cmd should NOT contain "--"
        bool found_separator = false;
        for(const auto& arg: cmd.argv) {
            if(arg == "--")
                found_separator = true;
        }
        expect(!found_separator);

        // Verify error message is contained in the last argument
        std::string last_arg = cmd.argv.back();
        expect(last_arg.find("Catter Proxy Error: File not found") != std::string::npos);
        expect(last_arg.find("in command: invalid --help") != std::string::npos);

        // Verify parse_opt returns valid result but with error state (due to missing --)
        std::vector<char*> mock_argv;
        for(const auto& s: cmd.argv)
            mock_argv.push_back(const_cast<char*>(s.c_str()));
        mock_argv.push_back(nullptr);

        auto parse_res = ct::optdata::catter_proxy::parse_opt(mock_argv.data());

        // According to parser implementation, if "--" is missing, subsequent strings
        // might be interpreted as an error or invalid input for raw_argv
        expect(parse_res.has_value());
        expect(!parse_res->raw_argv_or_err.has_value());
    };
};
