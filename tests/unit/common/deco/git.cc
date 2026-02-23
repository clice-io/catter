#include "deco/macro.h"
#include "deco/runtime.h"
#include <zest/zest.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
struct GitCommitOpt {
    DecoFlag(names = {"-a", "--all"}; help = "Stage all modified/deleted files";
             required = false;) all;

    DecoKV(names = {"-m", "--message"}; meta_var = "MSG";
           help = "Use the given message as the commit message";
           required = true;)<std::string> message;
};

struct GitCloneOpt {
    DecoInput(meta_var = "REPO"; help = "Repository URL"; required = true;)<std::string> repo;

    DecoKV(names = {"-b", "--branch"}; meta_var = "BRANCH";
           help = "Checkout BRANCH instead of HEAD";
           required = false;)<std::string> branch;
};

struct GitTagOpt {
    struct Cate {
        constexpr static deco::decl::Category mode_category{
            .exclusive = false,
            .required = true,
            .name = "mode",
            .description = "tag operation mode",
        };
    };

    DecoFlag(names = {"-l", "--list"}; help = "List tags"; required = false;
             category = Cate::mode_category;) list;
};

template <typename... Args>
std::vector<std::string> make_args(Args&&... args) {
    std::vector<std::string> argv;
    argv.reserve(sizeof...(args));
    (argv.emplace_back(std::forward<Args>(args)), ...);
    return argv;
}
}  // namespace

TEST_SUITE(deco_git){TEST_CASE(usage_lists_git_style_subcommands){
    deco::cli::Dispatcher<GitCommitOpt> commit_dispatcher("git commit [OPTIONS]");
commit_dispatcher.dispatch([](GitCommitOpt) {});

deco::cli::Dispatcher<GitCloneOpt> clone_dispatcher("git clone [OPTIONS]");
clone_dispatcher.dispatch([](GitCloneOpt) {});

deco::cli::SubCommander git("git [--version] [--help] <command> [<args>]",
                            "A fast, scalable, distributed version control system");
git.add(
       deco::decl::SubCommand{
           .name = "commit",
           .description = "Record changes to the repository",
       },
       commit_dispatcher)
    .add(
        deco::decl::SubCommand{
            .name = "clone",
            .description = "Clone a repository into a new directory",
        },
        clone_dispatcher);

std::stringstream ss;
git.usage(ss);
const auto usage = ss.str();
EXPECT_TRUE(usage.starts_with("A fast, scalable, distributed version control system"));
EXPECT_TRUE(usage.contains("Subcommands:"));
EXPECT_TRUE(usage.contains("commit"));
EXPECT_TRUE(usage.contains("clone"));
EXPECT_TRUE(!usage.contains("usage: git [--version] [--help] <command> [<args>]"));
}

TEST_CASE(clone_subcommand_parses_input_and_option) {
    std::string repo;
    std::string branch;
    std::string dispatch_err;
    std::string subcommand_err;

    deco::cli::Dispatcher<GitCloneOpt> clone_dispatcher("git clone [OPTIONS] REPO");
    clone_dispatcher
        .dispatch([&](GitCloneOpt opt) {
            EXPECT_TRUE(opt.repo.value.has_value());
            if(opt.repo.value.has_value()) {
                repo = *opt.repo;
            }
            EXPECT_TRUE(opt.branch.value.has_value());
            if(opt.branch.value.has_value()) {
                branch = *opt.branch;
            }
        })
        .when_err([&](auto err) { dispatch_err = err.message; });

    deco::cli::SubCommander git("git [--version] [--help] <command> [<args>]");
    git.add(
           deco::decl::SubCommand{
               .name = "clone",
               .description = "Clone a repository into a new directory",
           },
           clone_dispatcher)
        .when_err([&](auto err) { subcommand_err = err.message; });

    auto args = make_args("clone", "https://example.com/demo.git", "-b", "main");
    git(args);

    EXPECT_TRUE(dispatch_err.empty());
    EXPECT_TRUE(subcommand_err.empty());
    EXPECT_TRUE(repo == "https://example.com/demo.git");
    EXPECT_TRUE(branch == "main");
}

TEST_CASE(commit_subcommand_reports_required_option_error) {
    std::string dispatch_err;
    std::string subcommand_err;

    deco::cli::Dispatcher<GitCommitOpt> commit_dispatcher("git commit [OPTIONS]");
    commit_dispatcher.dispatch([](GitCommitOpt) {}).when_err([&](auto err) {
        dispatch_err = err.message;
    });

    deco::cli::SubCommander git("git [--version] [--help] <command> [<args>]");
    git.add(
           deco::decl::SubCommand{
               .name = "commit",
               .description = "Record changes to the repository",
           },
           commit_dispatcher)
        .when_err([&](auto err) { subcommand_err = err.message; });

    auto args = make_args("commit", "-a");
    git(args);

    EXPECT_TRUE(subcommand_err.empty());
    EXPECT_TRUE(dispatch_err.contains("required option -m|--message <MSG> is missing"));
}

TEST_CASE(unknown_subcommand_reports_error) {
    std::string subcommand_err;

    deco::cli::Dispatcher<GitCommitOpt> commit_dispatcher("git commit [OPTIONS]");
    commit_dispatcher.dispatch([](GitCommitOpt) {});

    deco::cli::SubCommander git("git [--version] [--help] <command> [<args>]");
    git.add(
           deco::decl::SubCommand{
               .name = "commit",
               .description = "Record changes to the repository",
           },
           commit_dispatcher)
        .when_err([&](auto err) { subcommand_err = err.message; });

    auto args = make_args("cherry-pick");
    git(args);

    EXPECT_TRUE(subcommand_err.contains("unknown subcommand 'cherry-pick'"));
}

TEST_CASE(required_category_error_is_reported) {
    std::string dispatch_err;

    deco::cli::Dispatcher<GitTagOpt> tag_dispatcher("git tag [OPTIONS]");
    tag_dispatcher.dispatch([](GitTagOpt) {}).when_err([&](auto err) {
        dispatch_err = err.message;
    });

    deco::cli::SubCommander git("git [--version] [--help] <command> [<args>]");
    git.add(
        deco::decl::SubCommand{
            .name = "tag",
            .description = "Create, list, delete or verify a tag object",
        },
        tag_dispatcher);

    auto args = make_args("tag");
    git(args);

    EXPECT_TRUE(dispatch_err.contains("required <mode> (tag operation mode) is missing"));
}
}
;
