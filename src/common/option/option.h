#pragma once
#include "parsed_arg.h"
#include "opt_specifier.h"
#include "opt_table.h"
#include <cassert>
#include <optional>
#include <string_view>
#include <utility>

namespace catter::opt {

/// ArgStringList - A list of arguments that can be passed to an Option, eg. splitted argv
using ArgStringList = std::span<std::string_view>;

/// Base flags for all options. Custom flags may be added after.
enum DriverFlag {
    HelpHidden = (1 << 0),
    RenderAsInput = (1 << 1),
    RenderJoined = (1 << 2),
    RenderSeparate = (1 << 3)
};

enum DriverVisibility {
    DefaultVis = (1 << 0),
};

/// Option - Abstract representation for a single form of driver
/// argument.
///
/// An Option class represents a form of option that the driver
/// takes, for example how many arguments the option has and how
/// they can be provided. Individual option instances store
/// additional information about what group the option is a member
/// of (if any), if the option is an alias, and a number of
/// flags. At runtime the driver parses the command line into
/// concrete Arg instances, each of which corresponds to a
/// particular Option instance.
class Option : public OptionEnum {
protected:
    const OptTable::Info* info;
    const OptTable* owner;

public:
    Option(const OptTable::Info* info, const OptTable* owner);

    bool valid() const {
        return this->info != nullptr;
    }

    unsigned id() const {
        assert(this->info && "Must have a valid info!");
        return this->info->id;
    }

    OptionClass kind() const {
        assert(this->info && "Must have a valid info!");
        return OptionClass(this->info->kind);
    }

    /// Get the name of this option without any prefix.
    std::string_view name() const {
        assert(this->info && "Must have a valid info!");
        assert(this->owner && "Must have a valid owner!");
        return this->owner->option_name(this->info->id);
    }

    const Option group() const {
        assert(this->info && "Must have a valid info!");
        assert(this->owner && "Must have a valid owner!");
        return this->owner->option(this->info->group_id);
    }

    const Option alias() const {
        assert(this->info && "Must have a valid info!");
        assert(this->owner && "Must have a valid owner!");
        return this->owner->option(this->info->alias_id);
    }

    /// Get the alias arguments as a \0 separated list.
    /// E.g. ["foo", "bar"] would be returned as "foo\0bar\0".
    const char* alias_args() const {
        assert(this->info && "Must have a valid info!");
        assert((!this->info->alias_args || this->info->alias_args[0] != 0) &&
               "AliasArgs should be either 0 or non-empty.");

        return this->info->alias_args;
    }

    /// Get the default prefix for this option.
    std::string_view prefix() const {
        assert(this->info && "Must have a valid info!");
        assert(this->owner && "Must have a valid owner!");
        return this->owner->option_prefix(this->info->id);
    }

    /// Get the name of this option with the default prefix.
    std::string_view prefixed_name() const {
        assert(this->info && "Must have a valid info!");
        assert(this->owner && "Must have a valid owner!");
        return this->owner->option_prefixed_name(this->info->id);
    }

    /// Get the help text for this option.
    std::string_view help_text() const {
        assert(this->info && "Must have a valid info!");
        return this->info->help_text;
    }

    /// Get the meta-variable list for this option.
    std::string_view meta_var() const {
        assert(this->info && "Must have a valid info!");
        return this->info->meta_var;
    }

    unsigned num_args() const {
        return this->info->param;
    }

    bool has_no_opt_as_input() const {
        return this->info->flags & RenderAsInput;
    }

    RenderStyleKind render_style() const {
        if(this->info->flags & RenderJoined)
            return RenderJoinedStyle;
        if(this->info->flags & RenderSeparate)
            return RenderSeparateStyle;
        switch(this->kind()) {
            case GroupClass:
            case InputClass:
            case UnknownClass: return RenderValuesStyle;
            case JoinedClass:
            case JoinedAndSeparateClass: return RenderJoinedStyle;
            case CommaJoinedClass: return RenderCommaJoinedStyle;
            case FlagClass:
            case ValuesClass:
            case SeparateClass:
            case MultiArgClass:
            case JoinedOrSeparateClass:
            case RemainingArgsClass:
            case RemainingArgsJoinedClass: return RenderSeparateStyle;
        }
        std::unreachable();
    }

    /// Test if this option has the flag val.
    bool has_flag(unsigned val) const {
        return this->info->flags & val;
    }

    /// Test if this option has the visibility flag Val.
    bool has_visibility_flag(unsigned val) const {
        return this->info->visibility & val;
    }

    /// getUnaliasedOption - Return the final option this option
    /// aliases (itself, if the option has no alias).
    const Option unaliased_option() const {
        const Option als = this->alias();
        if(als.valid())
            return als.unaliased_option();
        return *this;
    }

    /// getRenderName - Return the name to use when rendering this
    /// option.
    std::string_view render_name() const {
        return this->unaliased_option().name();
    }

    /// matches - Predicate for whether this option is part of the
    /// given option (which may be a group).
    ///
    /// Note that matches against options which are an alias should never be
    /// done -- aliases do not participate in matching and so such a query will
    /// always be false.
    bool matches(OptSpecifier id) const;

    //   bool isRegisteredSC(std::string_view SubCommand) const {
    //     assert(Info && "Must have a valid info!");
    //     assert(Owner && "Must have a valid owner!");
    //     return Owner->isValidForSubCommand(Info, SubCommand);
    //   }

    using ArgList = std::span<std::string>;

    /// Potentially accept the current argument, returning a new Arg instance,
    /// or 0 if the option does not accept this argument (or the argument is
    /// missing values).
    ///
    /// If the option accepts the current argument, accept() sets
    /// Index to the position where argument parsing should resume
    /// (even if the argument is missing values).
    ///
    /// \p CurArg The argument to be matched. It may be shorter than the
    /// underlying storage to represent a Joined argument.
    /// \p GroupedShortOption If true, we are handling the fallback case of
    /// parsing a prefix of the current argument as a short option.
    std::optional<ParsedArgument> accept(const ArgList& args,
                                         std::string_view cur_arg,
                                         bool grouped_short_option,
                                         unsigned& index) const;

private:
    std::optional<ParsedArgument> accept_internal(const ArgList& args,
                                                  std::string_view spelling,
                                                  unsigned& index) const;

public:
    void print(std::ostream& o, bool add_new_line) const;
};
}  // namespace catter::opt
