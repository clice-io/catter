module;

#include "modules/prelude.h"

export module catter.command:response_file;

export namespace catter {

/// How a program splits the text of a response file into arguments.
enum class Quoting : std::uint8_t {
    /// GCC's and clang's: whitespace ends an argument, single and double quotes hold whitespace,
    /// and a backslash takes the character after it as it is.
    GNU,
    /// cl's: whitespace ends an argument and double quotes hold it, `""` in them being one; a
    /// backslash is itself but before a double quote, where each pair is one backslash and one
    /// left over takes the quote as it is.
    Windows,
};

/// Reads the file at `path` into `out`: `kotatsu::fs::read_file`, or what a test reads instead.
using ReadFile =
    kotatsu::function_ref<std::error_code(kotatsu::buffer_ref out, std::string_view path)>;

/// A command's argv with the response files it names read in.
struct Expansion {
    /// argv, each `@file` replaced by the arguments the file holds, in turn read in; one that
    /// cannot be read stays as it is, as the program then takes it.
    std::vector<std::string> argv;

    /// For each element of `argv`, the element of the command's argv it is, or names the
    /// response file it is read from.
    std::vector<std::uint32_t> origins;

    /// The response files read, as named, nested ones too, in the order they are read.
    std::vector<std::string> files;

    /// The elements of `argv` that name a response file that cannot be read.
    std::vector<std::uint32_t> unread;
};

/// `argv`, argv[0] included, with its response files read in and split as `quoting` says. A
/// relative name, in a response file too, is from `directory`, the command's working directory.
/// A file's text is UTF-8 after its byte order mark, or UTF-16 after UTF-16's, as MSVC's tools
/// write it.
Expansion expand_response_files(std::span<const std::string> argv,
                                std::string_view directory,
                                Quoting quoting,
                                ReadFile read);

}  // namespace catter
