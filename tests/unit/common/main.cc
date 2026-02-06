#include <string>
#include <string_view>

#include <zest/zest.h>

int main(int argc, char** argv) {
    std::string filter;
    constexpr std::string_view filter_prefix = "--test-filter=";
    for(int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if(arg.starts_with(filter_prefix)) {
            filter.assign(arg.substr(filter_prefix.size()));
            continue;
        }
        if(arg == "--test-filter" && i + 1 < argc) {
            filter.assign(argv[++i]);
            continue;
        }
    }

    return zest::Runner::instance().run_tests(filter);
}
