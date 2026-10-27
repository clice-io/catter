module;

#if defined(__linux__) && defined(__x86_64__) && defined(NDEBUG)
#include "linux-x64/deps.fragment.h"
#elif defined(__linux__) && defined(__x86_64__) && !defined(NDEBUG)
#include "linux-x64-debug/deps.fragment.h"
#elif defined(__APPLE__) && defined(__aarch64__) && defined(NDEBUG)
#include "macos-arm64/deps.fragment.h"
#elif defined(__APPLE__) && defined(__aarch64__) && !defined(NDEBUG)
#include "macos-arm64-debug/deps.fragment.h"
#elif defined(_WIN32) && defined(__x86_64__) && defined(NDEBUG)
#include "windows-x64/deps.fragment.h"
#elif defined(_WIN32) && defined(__x86_64__) && !defined(NDEBUG)
#include "windows-x64-debug/deps.fragment.h"
#elif defined(_WIN32) && defined(__aarch64__) && defined(NDEBUG)
#include "windows-arm64/deps.fragment.h"
#else
#error "no configuration merged matches this compilation"
#endif

export module deps;

template <class T> using unnamed_enum = T;

export using ::operator""_padded;
export using ::size_t;
export using ::uv_loop_s;
export using ::uv_loop_t;

export namespace cpptrace {
using ::cpptrace::can_get_safe_object_frame;
using ::cpptrace::can_signal_safe_unwind;
using ::cpptrace::clear_all_jit_objects;
using ::cpptrace::domain_error;
using ::cpptrace::exception;
using ::cpptrace::exception_with_message;
using ::cpptrace::frame_ptr;
using ::cpptrace::generate_object_trace;
using ::cpptrace::generate_raw_trace;
using ::cpptrace::generate_trace;
using ::cpptrace::get_safe_object_frame;
using ::cpptrace::invalid_argument;
using ::cpptrace::lazy_exception;
using ::cpptrace::length_error;
using ::cpptrace::logic_error;
using ::cpptrace::nested_exception;
using ::cpptrace::nullable;
using ::cpptrace::object_frame;
using ::cpptrace::object_trace;
using ::cpptrace::out_of_range;
using ::cpptrace::overflow_error;
using ::cpptrace::range_error;
using ::cpptrace::raw_trace;
using ::cpptrace::register_jit_object;
using ::cpptrace::rethrow_and_wrap_if_needed;
using ::cpptrace::runtime_error;
using ::cpptrace::safe_generate_raw_trace;
using ::cpptrace::safe_object_frame;
using ::cpptrace::stacktrace;
using ::cpptrace::stacktrace_frame;
using ::cpptrace::system_error;
using ::cpptrace::underflow_error;
using ::cpptrace::unregister_jit_object;
}

export namespace cpptrace::detail {
using ::cpptrace::detail::get_raw_trace_and_absorb;
using ::cpptrace::detail::lazy_trace_holder;
}

export namespace kota {
using ::kota::UTF8Sequence;
using ::kota::acceptor;
using ::kota::aggregate_op;
using ::kota::any_word;
using ::kota::async_node;
using ::kota::async_visitor;
using ::kota::bad_outcome_access;
using ::kota::basic_buffer_ref;
using ::kota::bind;
using ::kota::bind_ref;
using ::kota::buffer_ref;
using ::kota::bytes_below;
using ::kota::bytes_equal;
using ::kota::callable_args_count_v;
using ::kota::callable_args_t;
using ::kota::callable_return_t;
using ::kota::callable_traits;
using ::kota::cancel;
using ::kota::cancellation;
using ::kota::cancellation_callback;
using ::kota::cancellation_source;
using ::kota::cancellation_token;
using ::kota::check;
using ::kota::class_type_t;
using ::kota::co_invoke;
using ::kota::compare_exact;
using ::kota::condition_variable;
using ::kota::console;
using ::kota::decode_utf8;
using ::kota::dependent_false;
using ::kota::dump_dot;
using ::kota::endpoint;
using ::kota::eq_comparable_with;
using ::kota::error;
using ::kota::event;
using ::kota::event_loop;
using ::kota::fail;
using ::kota::first_byte_index;
using ::kota::format_kind;
using ::kota::from_utf16;
using ::kota::function;
using ::kota::function_args_count_v;
using ::kota::function_args_t;
using ::kota::function_ref;
using ::kota::function_return_t;
using ::kota::function_traits;
using ::kota::ge_comparable_with;
using ::kota::gt_comparable_with;
using ::kota::guess_handle;
using ::kota::handle_type;
using ::kota::has_byte;
using ::kota::has_non_ascii;
using ::kota::hybrid_vector;
using ::kota::idle;
using ::kota::io_op;
using ::kota::is_expected_v;
using ::kota::is_function_pointer_v;
using ::kota::is_functor_v;
using ::kota::is_map_value_v;
using ::kota::is_mem_fn_of;
using ::kota::is_one_of;
using ::kota::is_optional_v;
using ::kota::is_outcome_v;
using ::kota::is_specialization_of;
using ::kota::is_utf8;
using ::kota::le_comparable_with;
using ::kota::load_word;
using ::kota::lt_comparable_with;
using ::kota::map_entry_key_t;
using ::kota::map_entry_mapped_t;
using ::kota::map_range;
using ::kota::mem_fn;
using ::kota::member_traits;
using ::kota::member_type_t;
using ::kota::mutex;
using ::kota::narrow_int;
using ::kota::or_fail;
using ::kota::ordered_associative_range;
using ::kota::ordered_map_range;
using ::kota::ordered_set_range;
using ::kota::outcome;
using ::kota::outcome_cancel;
using ::kota::outcome_cancel_t;
using ::kota::outcome_error;
using ::kota::outcome_error_t;
using ::kota::outcome_ok_tag;
using ::kota::outcome_value;
using ::kota::pipe;
using ::kota::prepare;
using ::kota::process;
using ::kota::queue;
using ::kota::range_format;
using ::kota::range_of_kind;
using ::kota::relay;
using ::kota::replace_invalid_utf8;
using ::kota::result;
using ::kota::run;
using ::kota::semaphore;
using ::kota::sequence_range;
using ::kota::set_range;
using ::kota::signal;
using ::kota::sleep;
using ::kota::small_string;
using ::kota::small_vector;
using ::kota::small_wstring;
using ::kota::stream;
using ::kota::string_buffer;
using ::kota::string_list_ref;
using ::kota::string_ref;
using ::kota::sync_primitive;
using ::kota::task;
using ::kota::task_frame;
using ::kota::task_group;
using ::kota::tcp;
using ::kota::timer;
using ::kota::to_utf16;
using ::kota::tuple_any_of_v;
using ::kota::tuple_count_of_v;
using ::kota::tuple_find;
using ::kota::tuple_find_spec;
using ::kota::tuple_find_spec_t;
using ::kota::tuple_find_t;
using ::kota::tuple_has_spec_v;
using ::kota::tuple_has_v;
using ::kota::tuple_push_front;
using ::kota::tuple_push_front_t;
using ::kota::type_list;
using ::kota::type_list_cat;
using ::kota::type_list_cat_t;
using ::kota::type_list_concat;
using ::kota::type_list_concat_t;
using ::kota::type_list_contains;
using ::kota::type_list_contains_v;
using ::kota::type_list_element;
using ::kota::type_list_element_t;
using ::kota::type_list_filter;
using ::kota::type_list_filter_t;
using ::kota::type_list_prepend;
using ::kota::type_list_prepend_t;
using ::kota::type_list_size;
using ::kota::type_list_size_v;
using ::kota::type_list_to_union;
using ::kota::type_list_to_union_t;
using ::kota::type_list_unique;
using ::kota::type_list_unique_impl;
using ::kota::type_list_unique_t;
using ::kota::udp;
using ::kota::unordered_associative_range;
using ::kota::unordered_map_range;
using ::kota::unordered_set_range;
using ::kota::vector;
using ::kota::wait_node;
using ::kota::watcher;
using ::kota::when_all;
using ::kota::when_any;
using ::kota::with_task_group;
using ::kota::with_timeout;
using ::kota::with_token;
using ::kota::yield;
using ::kota::yield_awaiter;
}

export namespace kota::codec {
using ::kota::codec::FieldName;
using ::kota::codec::MapKeyReader;
using ::kota::codec::MapKeyWriter;
using ::kota::codec::RawValue;
using ::kota::codec::StringKeySink;
using ::kota::codec::UnknownFields;
using ::kota::codec::apply_enum_rename;
using ::kota::codec::char_from_utf8;
using ::kota::codec::char_to_utf8;
using ::kota::codec::decode_struct_fields;
using ::kota::codec::decode_value;
using ::kota::codec::decode_variant;
using ::kota::codec::default_config;
using ::kota::codec::deserialize_visit;
using ::kota::codec::encode_struct_fields;
using ::kota::codec::encode_value;
using ::kota::codec::enum_repr;
using ::kota::codec::invalid_char_message;
using ::kota::codec::invalid_utf8;
using ::kota::codec::is_human_readable;
using ::kota::codec::layout_computed;
using ::kota::codec::nan_repr;
using ::kota::codec::rich_error;
using ::kota::codec::scoped_context;
using ::kota::codec::serialize_visit;
using ::kota::codec::writes_every_field;
using ::kota::codec::writes_utf8;
}

export namespace kota::codec::bincode {
using ::kota::codec::bincode::MapAccess;
using ::kota::codec::bincode::Reader;
using ::kota::codec::bincode::SeqAccess;
using ::kota::codec::bincode::Writer;
using ::kota::codec::bincode::format;
using ::kota::codec::bincode::from_bytes;
using ::kota::codec::bincode::to_bytes;
}

export namespace kota::codec::bincode::detail {
using ::kota::codec::bincode::detail::LengthMarker;
using ::kota::codec::bincode::detail::invalid_length;
using ::kota::codec::bincode::detail::number_out_of_range;
using ::kota::codec::bincode::detail::trailing_bytes;
using ::kota::codec::bincode::detail::type_mismatch;
using ::kota::codec::bincode::detail::unexpected_eof;
}

export namespace kota::codec::debug {
using ::kota::codec::debug::Formatter;
using ::kota::codec::debug::MapWriter;
using ::kota::codec::debug::SeqWriter;
using ::kota::codec::debug::StructWriter;
using ::kota::codec::debug::TupleWriter;
using ::kota::codec::debug::ValueWriter;
using ::kota::codec::debug::to_string;
}

export namespace kota::codec::detail {
using ::kota::codec::detail::FieldAt;
using ::kota::codec::detail::assert_human_readable_allowed;
using ::kota::codec::detail::assert_internal_tag_fits;
using ::kota::codec::detail::check_required_fields;
using ::kota::codec::detail::construct_and_visit;
using ::kota::codec::detail::data_driven;
using ::kota::codec::detail::decode_adjacently_tagged;
using ::kota::codec::detail::decode_enum_name;
using ::kota::codec::detail::decode_externally_tagged;
using ::kota::codec::detail::decode_field_value;
using ::kota::codec::detail::decode_internally_tagged;
using ::kota::codec::detail::decode_one_field;
using ::kota::codec::detail::decode_step;
using ::kota::codec::detail::decode_untagged_variant;
using ::kota::codec::detail::decode_with_attrs;
using ::kota::codec::detail::empty_config_base;
using ::kota::codec::detail::encode_one_field;
using ::kota::codec::detail::encode_tagged_variant;
using ::kota::codec::detail::encode_text;
using ::kota::codec::detail::encode_untagged_variant;
using ::kota::codec::detail::encode_with_attrs;
using ::kota::codec::detail::ensure_allocated;
using ::kota::codec::detail::fail_unknown_tag;
using ::kota::codec::detail::fail_unnamed_enum;
using ::kota::codec::detail::fail_unusable_tag;
using ::kota::codec::detail::find_field_slot;
using ::kota::codec::detail::find_tag_index;
using ::kota::codec::detail::has_field_named;
using ::kota::codec::detail::has_native_variant;
using ::kota::codec::detail::has_peek_kind;
using ::kota::codec::detail::has_try_read;
using ::kota::codec::detail::human_readable_allowed;
using ::kota::codec::detail::kind_compatible;
using ::kota::codec::detail::kind_compatible_impl;
using ::kota::codec::detail::match_field;
using ::kota::codec::detail::nested_alternative_pass;
using ::kota::codec::detail::no_tag;
using ::kota::codec::detail::pass_unknown_field;
using ::kota::codec::detail::peek_tag_index;
using ::kota::codec::detail::prepend_step;
using ::kota::codec::detail::probes_as_untagged_variant;
using ::kota::codec::detail::repr_decode;
using ::kota::codec::detail::repr_encode;
using ::kota::codec::detail::required_mask;
using ::kota::codec::detail::reserve_for;
using ::kota::codec::detail::run_decode;
using ::kota::codec::detail::run_encode;
using ::kota::codec::detail::skipped;
using ::kota::codec::detail::slot_required;
using ::kota::codec::detail::taggable;
using ::kota::codec::detail::trace_path;
using ::kota::codec::detail::try_read_callable;
using ::kota::codec::detail::untagged_variant_pass;
using ::kota::codec::detail::visit_variant_callable;
using ::kota::codec::detail::with_index;
}

export namespace kota::codec::dyn {
using ::kota::codec::dyn::Array;
using ::kota::codec::dyn::Cursor;
using ::kota::codec::dyn::MapWriter;
using ::kota::codec::dyn::Object;
using ::kota::codec::dyn::SeqWriter;
using ::kota::codec::dyn::StructWriter;
using ::kota::codec::dyn::Value;
using ::kota::codec::dyn::ValueKind;
using ::kota::codec::dyn::ValueReader;
using ::kota::codec::dyn::ValueWriter;
using ::kota::codec::dyn::from_dyn;
using ::kota::codec::dyn::to_dyn;
}

export namespace kota::codec::dyn::detail {
using ::kota::codec::dyn::detail::fail_type;
using ::kota::codec::dyn::detail::kind_name;
}

export namespace kota::codec::json {
using ::kota::codec::json::KeySink;
using ::kota::codec::json::MapWriter;
using ::kota::codec::json::Reader;
using ::kota::codec::json::SeqWriter;
using ::kota::codec::json::Source;
using ::kota::codec::json::StringBuilder;
using ::kota::codec::json::StructWriter;
using ::kota::codec::json::ValueWriter;
using ::kota::codec::json::enum_rename_fn;
using ::kota::codec::json::format;
using ::kota::codec::json::from_padded_string;
using ::kota::codec::json::from_string;
using ::kota::codec::json::padded_string;
using ::kota::codec::json::padded_string_view;
using ::kota::codec::json::prettify;
using ::kota::codec::json::schema;
using ::kota::codec::json::schema_options;
using ::kota::codec::json::schema_string;
using ::kota::codec::json::success;
using ::kota::codec::json::to_string;
}

export namespace kota::codec::json::detail {
using ::kota::codec::json::detail::DefaultAnnotator;
using ::kota::codec::json::detail::FreshDefaults;
using ::kota::codec::json::detail::ParsedDocument;
using ::kota::codec::json::detail::SchemaEmitter;
using ::kota::codec::json::detail::alternative_marker;
using ::kota::codec::json::detail::collect_fresh;
using ::kota::codec::json::detail::collect_fresh_alternatives;
using ::kota::codec::json::detail::collect_fresh_slots;
using ::kota::codec::json::detail::copy_bytes;
using ::kota::codec::json::detail::count_lines;
using ::kota::codec::json::detail::decoder_reads_directly;
using ::kota::codec::json::detail::default_document_config;
using ::kota::codec::json::detail::doc_iter_ptr;
using ::kota::codec::json::detail::doc_iter_tag;
using ::kota::codec::json::detail::escape_to;
using ::kota::codec::json::detail::escaped_bytes;
using ::kota::codec::json::detail::escaped_size;
using ::kota::codec::json::detail::is_control;
using ::kota::codec::json::detail::member_key;
using ::kota::codec::json::detail::needs_escape;
using ::kota::codec::json::detail::options_of;
using ::kota::codec::json::detail::schema_config;
using ::kota::codec::json::detail::short_escape;
using ::kota::codec::json::detail::simdjson_error;
using ::kota::codec::json::detail::steal;
using ::kota::codec::json::detail::stringify;
using ::kota::codec::json::detail::val_iter_ptr;
using ::kota::codec::json::detail::val_iter_tag;
}

export namespace kota::codec::json::ondemand {
using ::kota::codec::json::ondemand::Document;
using ::kota::codec::json::ondemand::NumberType;
using ::kota::codec::json::ondemand::Parser;
using ::kota::codec::json::ondemand::Type;
using ::kota::codec::json::ondemand::Value;
}

export namespace kota::comptime {
using ::kota::comptime::ComptimeMemoryResource;
using ::kota::comptime::ComptimeVector;
using ::kota::comptime::Record;
using ::kota::comptime::counting_flag;
}

export namespace kota::deco {
namespace backend = ::kota::option;
namespace refl = ::kota::meta;
using ::kota::deco::ParsedArgOwning;
}

export namespace kota::deco::cli {
using ::kota::deco::cli::AfterStep;
using ::kota::deco::cli::Command;
using ::kota::deco::cli::Invocation;
using ::kota::deco::cli::ParseError;
using ::kota::deco::cli::ParsedResult;
using ::kota::deco::cli::SubCommandError;
using ::kota::deco::cli::SubCommandMatch;
using ::kota::deco::cli::SubCommander;
using ::kota::deco::cli::check_valid;
using ::kota::deco::cli::command;
using ::kota::deco::cli::invoke;
using ::kota::deco::cli::parse;
using ::kota::deco::cli::parse_error_exit_code;
using ::kota::deco::cli::parse_with_callback;
using ::kota::deco::cli::runtime_callable_t;
using ::kota::deco::cli::write_usage_for;
}

export namespace kota::deco::cli::detail {
using ::kota::deco::cli::detail::access_member_path;
using ::kota::deco::cli::detail::exit_code_of;
using ::kota::deco::cli::detail::help_requested;
using ::kota::deco::cli::detail::input_args_text;
using ::kota::deco::cli::detail::make_usage_document;
using ::kota::deco::cli::detail::member_object_pointer_traits;
using ::kota::deco::cli::detail::member_path_result;
using ::kota::deco::cli::detail::member_path_result_t;
using ::kota::deco::cli::detail::member_path_step;
using ::kota::deco::cli::detail::resolve_alias_forward;
using ::kota::deco::cli::detail::resolve_static_alias_forward;
using ::kota::deco::cli::detail::run_parse_session;
}

export namespace kota::deco::cli::text {
using ::kota::deco::cli::text::CompatibleRenderer;
using ::kota::deco::cli::text::CompatibleRendererConfig;
using ::kota::deco::cli::text::Diagnostic;
using ::kota::deco::cli::text::ModernRenderer;
using ::kota::deco::cli::text::ModernRendererConfig;
using ::kota::deco::cli::text::PositionStyle;
using ::kota::deco::cli::text::Renderer;
using ::kota::deco::cli::text::SubCommandDocument;
using ::kota::deco::cli::text::SubCommandEntry;
using ::kota::deco::cli::text::SubCommandStyle;
using ::kota::deco::cli::text::TextStyle;
using ::kota::deco::cli::text::UsageDocument;
using ::kota::deco::cli::text::UsageEntry;
using ::kota::deco::cli::text::UsageGroup;
using ::kota::deco::cli::text::UsageStyle;
using ::kota::deco::cli::text::clear_default_renderer;
using ::kota::deco::cli::text::default_renderer;
using ::kota::deco::cli::text::diagnostic_at;
using ::kota::deco::cli::text::diagnostic_message;
using ::kota::deco::cli::text::explicit_default_renderer;
using ::kota::deco::cli::text::looks_like_rendered_diagnostic;
using ::kota::deco::cli::text::render_diagnostic;
using ::kota::deco::cli::text::render_subcommands;
using ::kota::deco::cli::text::render_usage;
using ::kota::deco::cli::text::render_usage_entry;
using ::kota::deco::cli::text::resolve_renderer;
using ::kota::deco::cli::text::set_default_renderer;
}

export namespace kota::deco::config {
using ::kota::deco::config::BuiltInRenderConfig;
using ::kota::deco::config::CompatibleRendererConfig;
using ::kota::deco::config::Config;
using ::kota::deco::config::ConfigOverride;
using ::kota::deco::config::EnumMetaVarConfig;
using ::kota::deco::config::ModernRendererConfig;
using ::kota::deco::config::PositionStyle;
using ::kota::deco::config::SubCommandStyle;
using ::kota::deco::config::TextStyle;
using ::kota::deco::config::UsageStyle;
using ::kota::deco::config::get;
using ::kota::deco::config::merge;
using ::kota::deco::config::set;
using ::kota::deco::config::set_enum_meta_var;
using ::kota::deco::config::set_render;
}

export namespace kota::deco::decl {
using ::kota::deco::decl::AliasFields;
using ::kota::deco::decl::AliasForwardField;
using ::kota::deco::decl::AliasForwardFn;
using ::kota::deco::decl::AliasForwardFnWithContext;
using ::kota::deco::decl::AliasForwardResult;
using ::kota::deco::decl::ArgNum;
using ::kota::deco::decl::Category;
using ::kota::deco::decl::CategoryRef;
using ::kota::deco::decl::CommaJoinedAliasFields;
using ::kota::deco::decl::CommaJoinedFields;
using ::kota::deco::decl::CommonOptionFields;
using ::kota::deco::decl::ConfigFields;
using ::kota::deco::decl::ConfigOverrideField;
using ::kota::deco::decl::DecoFields;
using ::kota::deco::decl::DecoOption;
using ::kota::deco::decl::DecoOptionBase;
using ::kota::deco::decl::DecoType;
using ::kota::deco::decl::ErasedParseCallback;
using ::kota::deco::decl::FlagAliasFields;
using ::kota::deco::decl::FlagFields;
using ::kota::deco::decl::FlagOption;
using ::kota::deco::decl::HelpOption;
using ::kota::deco::decl::InputFields;
using ::kota::deco::decl::InputOption;
using ::kota::deco::decl::IntoContext;
using ::kota::deco::decl::KVAliasFields;
using ::kota::deco::decl::KVFields;
using ::kota::deco::decl::KVOption;
using ::kota::deco::decl::KVStyle;
using ::kota::deco::decl::MetaVarField;
using ::kota::deco::decl::MultiAliasFields;
using ::kota::deco::decl::MultiFields;
using ::kota::deco::decl::NamedOptionFields;
using ::kota::deco::decl::OptionCallbackField;
using ::kota::deco::decl::OptionDeclaration;
using ::kota::deco::decl::PackFields;
using ::kota::deco::decl::ParseControl;
using ::kota::deco::decl::ParseStep;
using ::kota::deco::decl::ScalarOption;
using ::kota::deco::decl::SubCommand;
using ::kota::deco::decl::VectorOption;
using ::kota::deco::decl::default_category;
using ::kota::deco::decl::help_names;
using ::kota::deco::decl::input_arg_num;
using ::kota::deco::decl::invalid_input_arg_num;
using ::kota::deco::decl::is_alias_placeholder_name;
using enum ::kota::deco::decl::KVStyle;
}

export namespace kota::deco::decl::detail {
using ::kota::deco::decl::detail::append_vector;
using ::kota::deco::decl::detail::assign_scalar;
using ::kota::deco::decl::detail::assign_vector;
using ::kota::deco::decl::detail::format_invalid_enum_value;
using ::kota::deco::decl::detail::generated_option_name;
using ::kota::deco::decl::detail::has_kv_style;
using ::kota::deco::decl::detail::iequals_ascii;
using ::kota::deco::decl::detail::is_joined_kv_name;
using ::kota::deco::decl::detail::parse_primitive_scalar;
using ::kota::deco::decl::detail::takes_value_after_equals;
}

export namespace kota::deco::desc {
using ::kota::deco::desc::from_deco_option;
}

export namespace kota::deco::desc::detail {
using ::kota::deco::desc::detail::active_config;
using ::kota::deco::desc::detail::base_meta_name;
using ::kota::deco::desc::detail::category_desc;
using ::kota::deco::desc::detail::comma_joined_alias;
using ::kota::deco::desc::detail::enum_meta_var_token;
using ::kota::deco::desc::detail::has_help_text;
using ::kota::deco::desc::detail::inferred_enum_meta_var_type;
using ::kota::deco::desc::detail::inferred_enum_meta_var_type_t;
using ::kota::deco::desc::detail::inferred_meta_var_token;
using ::kota::deco::desc::detail::is_repeatable;
using ::kota::deco::desc::detail::join_aliases;
using ::kota::deco::desc::detail::join_strings;
using ::kota::deco::desc::detail::kv_usage_text;
using ::kota::deco::desc::detail::meta_var_token;
using ::kota::deco::desc::detail::named_aliases;
using ::kota::deco::desc::detail::placeholder_name;
using ::kota::deco::desc::detail::repeated_meta_vars;
using ::kota::deco::desc::detail::usage_entry;
using ::kota::deco::desc::detail::usage_text;
using ::kota::deco::desc::detail::usage_text_for_field;
}

export namespace kota::deco::detail {
using ::kota::deco::detail::DecoStructConsumer;
using ::kota::deco::detail::LLVMOptGenerator;
using ::kota::deco::detail::SizedGenerator;
using ::kota::deco::detail::StrPool;
using ::kota::deco::detail::build_record;
using ::kota::deco::detail::generator_of;
using ::kota::deco::detail::prefixes_of;
using ::kota::deco::detail::same_option_name;
}

export namespace kota::deco::ser {
using ::kota::deco::ser::Serializer;
using ::kota::deco::ser::to_argv;
}

export namespace kota::deco::trait {
using ::kota::deco::trait::BaseResultTy;
using ::kota::deco::trait::BorrowedStringResultType;
using ::kota::deco::trait::CustomStringResultTy;
using ::kota::deco::trait::CustomStringResultTyWithContext;
using ::kota::deco::trait::CustomStringVectorResultTy;
using ::kota::deco::trait::CustomStringVectorResultTyWithContext;
using ::kota::deco::trait::FlagResultType;
using ::kota::deco::trait::OptionalResultType;
using ::kota::deco::trait::OptionalTrait;
using ::kota::deco::trait::OwnedStringResultType;
using ::kota::deco::trait::PrimitiveScalarResultType;
using ::kota::deco::trait::PrimitiveVectorResultType;
using ::kota::deco::trait::ScalarResultType;
using ::kota::deco::trait::StringResultType;
using ::kota::deco::trait::ValueResultType;
using ::kota::deco::trait::VectorResultType;
}

export namespace kota::deco::ty {
using ::kota::deco::ty::alias_ty_of;
using ::kota::deco::ty::base_ty;
using ::kota::deco::ty::cfg_ty_of;
using ::kota::deco::ty::deco_option_like;
using ::kota::deco::ty::dyn_cast;
using ::kota::deco::ty::field_ty_of;
using ::kota::deco::ty::is_alias_field;
using ::kota::deco::ty::is_config_field;
using ::kota::deco::ty::is_deco_field;
using ::kota::deco::ty::is_deco_field_or_option;
using ::kota::deco::ty::is_decoed;
using ::kota::deco::ty::is_option_field;
}

export namespace kota::deco::util {
using ::kota::deco::util::argvify;
}

export namespace kota::detail {
using ::kota::detail::BasicGuard;
using ::kota::detail::CancellationNode;
using ::kota::detail::CancellationState;
using ::kota::detail::ErasedCallable;
using ::kota::detail::ScopedAwaiter;
using ::kota::detail::aggregate_result_t;
using ::kota::detail::append_sequence_element;
using ::kota::detail::async_range;
using ::kota::detail::await_probe_handle;
using ::kota::detail::await_probe_promise;
using ::kota::detail::await_result_t;
using ::kota::detail::awaitable;
using ::kota::detail::awaiter;
using ::kota::detail::awaiter_t;
using ::kota::detail::call_result_t;
using ::kota::detail::cancel_await;
using ::kota::detail::decode_sequence;
using ::kota::detail::default_buffer_size;
using ::kota::detail::destroy_handle;
using ::kota::detail::expire;
using ::kota::detail::fail_await;
using ::kota::detail::failing;
using ::kota::detail::from_utf16;
using ::kota::detail::get_awaiter;
using ::kota::detail::group_error;
using ::kota::detail::inline_buffer;
using ::kota::detail::insert_map_entry;
using ::kota::detail::is_group_body_v;
using ::kota::detail::is_plain_task_v;
using ::kota::detail::is_small_vector_v;
using ::kota::detail::is_task_v;
using ::kota::detail::keep_non_void;
using ::kota::detail::loop_access;
using ::kota::detail::map_entry_like;
using ::kota::detail::map_entry_pair_like;
using ::kota::detail::map_entry_tuple_like;
using ::kota::detail::map_entry_types;
using ::kota::detail::map_insertable;
using ::kota::detail::merged_channel_t;
using ::kota::detail::normalize;
using ::kota::detail::normalized_range_task_t;
using ::kota::detail::normalized_task;
using ::kota::detail::normalized_task_t;
using ::kota::detail::or_fail_await;
using ::kota::detail::or_fail_result;
using ::kota::detail::or_fail_task;
using ::kota::detail::or_fail_task_await;
using ::kota::detail::pack_any_v;
using ::kota::detail::pack_count_v;
using ::kota::detail::pack_find;
using ::kota::detail::pack_find_spec;
using ::kota::detail::promise_result;
using ::kota::detail::range_tasks;
using ::kota::detail::run_on_pool;
using ::kota::detail::run_until;
using ::kota::detail::sequence_insertable;
using ::kota::detail::small_vector_compatible_range;
using ::kota::detail::small_vector_size_type;
using ::kota::detail::success_t;
using ::kota::detail::synth_three_way;
using ::kota::detail::task_access;
using ::kota::detail::task_promise;
using ::kota::detail::task_return_object;
using ::kota::detail::to_utf16;
using ::kota::detail::unique_handle;
using ::kota::detail::when_children;
using ::kota::detail::when_op;
}

export namespace kota::fs {
using ::kota::fs::DirEntry;
using ::kota::fs::DirStream;
using ::kota::fs::File;
using ::kota::fs::FileID;
using ::kota::fs::FileTimes;
using ::kota::fs::FileType;
using ::kota::fs::OpenOptions;
using ::kota::fs::SeekFrom;
using ::kota::fs::Status;
using ::kota::fs::Volume;
using ::kota::fs::absolute;
using ::kota::fs::canonical;
using ::kota::fs::create_directories;
using ::kota::fs::create_directory;
using ::kota::fs::create_directory_symlink;
using ::kota::fs::create_hard_link;
using ::kota::fs::create_symlink;
using ::kota::fs::create_temp_directory;
using ::kota::fs::create_temp_file;
using ::kota::fs::current_dir;
using ::kota::fs::equivalent;
using ::kota::fs::exists;
using ::kota::fs::is_directory;
using ::kota::fs::is_file;
using ::kota::fs::is_symlink;
using ::kota::fs::read_dir;
using ::kota::fs::read_file;
using ::kota::fs::read_symlink;
using ::kota::fs::remove;
using ::kota::fs::remove_all;
using ::kota::fs::rename;
using ::kota::fs::set_permissions;
using ::kota::fs::set_times;
using ::kota::fs::status;
using ::kota::fs::symlink_status;
using ::kota::fs::volume;
using ::kota::fs::weakly_canonical;
using ::kota::fs::write_file;
using ::kota::fs::write_file_atomic;
}

export namespace kota::fs::async {
using ::kota::fs::async::access;
using ::kota::fs::async::chmod;
using ::kota::fs::async::chown;
using ::kota::fs::async::close;
using ::kota::fs::async::copyfile;
using ::kota::fs::async::copyfile_options;
using ::kota::fs::async::dir_handle;
using ::kota::fs::async::dirent;
using ::kota::fs::async::fchmod;
using ::kota::fs::async::fchown;
using ::kota::fs::async::fdatasync;
using ::kota::fs::async::file_stats;
using ::kota::fs::async::file_time;
using ::kota::fs::async::fs_stats;
using ::kota::fs::async::fstat;
using ::kota::fs::async::fsync;
using ::kota::fs::async::ftruncate;
using ::kota::fs::async::futime;
using ::kota::fs::async::lchown;
using ::kota::fs::async::link;
using ::kota::fs::async::lstat;
using ::kota::fs::async::lutime;
using ::kota::fs::async::mkdir;
using ::kota::fs::async::mkdtemp;
using ::kota::fs::async::mkstemp;
using ::kota::fs::async::mkstemp_result;
using ::kota::fs::async::open;
using ::kota::fs::async::opendir;
using ::kota::fs::async::read;
using ::kota::fs::async::readdir;
using ::kota::fs::async::readlink;
using ::kota::fs::async::realpath;
using ::kota::fs::async::rename;
using ::kota::fs::async::rmdir;
using ::kota::fs::async::scandir;
using ::kota::fs::async::sendfile;
using ::kota::fs::async::stat;
using ::kota::fs::async::statfs;
using ::kota::fs::async::symlink;
using ::kota::fs::async::unlink;
using ::kota::fs::async::utime;
using ::kota::fs::async::write;
}

export namespace kota::fs::detail {
using ::kota::fs::detail::OpenDirectory;
}

export namespace kota::fs::path {
using ::kota::fs::path::DotDot;
using ::kota::fs::path::Prefix;
using ::kota::fs::path::Root;
using ::kota::fs::path::Style;
using ::kota::fs::path::ancestors;
using ::kota::fs::path::ancestors_view;
using ::kota::fs::path::components;
using ::kota::fs::path::components_view;
using ::kota::fs::path::extension;
using ::kota::fs::path::filename;
using ::kota::fs::path::has_root;
using ::kota::fs::path::is_absolute;
using ::kota::fs::path::is_separator;
using ::kota::fs::path::is_within;
using ::kota::fs::path::native_style;
using ::kota::fs::path::normalize;
using ::kota::fs::path::parent;
using ::kota::fs::path::pop;
using ::kota::fs::path::push;
using ::kota::fs::path::relative;
using ::kota::fs::path::root;
using ::kota::fs::path::set_extension;
using ::kota::fs::path::set_file_name;
using ::kota::fs::path::stem;
using ::kota::fs::path::strip_prefix;
using ::kota::fs::path::to_backslash;
using ::kota::fs::path::to_slash;
}

export namespace kota::ipc {
using ::kota::ipc::BincodeCodec;
using ::kota::ipc::BincodePeer;
using ::kota::ipc::Error;
using ::kota::ipc::FrameParser;
using ::kota::ipc::IncomingErrorResponse;
using ::kota::ipc::IncomingMessage;
using ::kota::ipc::IncomingNotification;
using ::kota::ipc::IncomingParseError;
using ::kota::ipc::IncomingRequest;
using ::kota::ipc::IncomingResponse;
using ::kota::ipc::JSONCodec;
using ::kota::ipc::JSONPeer;
using ::kota::ipc::LogCallback;
using ::kota::ipc::LogLevel;
using ::kota::ipc::MessageHead;
using ::kota::ipc::PayloadSlice;
using ::kota::ipc::Peer;
using ::kota::ipc::ReadError;
using ::kota::ipc::RequestResult;
using ::kota::ipc::Result;
using ::kota::ipc::StreamTransport;
using ::kota::ipc::Transport;
using ::kota::ipc::UnansweredRequest;
using ::kota::ipc::basic_request_context;
using ::kota::ipc::frame_header;
using ::kota::ipc::lsp_config;
using ::kota::ipc::request_options;
}

export namespace kota::ipc::detail {
using ::kota::ipc::detail::LoggedID;
using ::kota::ipc::detail::callback_args_t;
using ::kota::ipc::detail::callback_param_t;
using ::kota::ipc::detail::callback_return_t;
using ::kota::ipc::detail::has_notification_traits_v;
using ::kota::ipc::detail::has_request_traits_v;
using ::kota::ipc::detail::message_prefix;
using ::kota::ipc::detail::notification_prefix;
using ::kota::ipc::detail::request_prefix;
using ::kota::ipc::detail::success_prefix;
using ::kota::ipc::detail::takes_no_params_v;
using ::kota::ipc::detail::validate_notification_callback_signature;
using ::kota::ipc::detail::validate_request_callback_signature;
}

export namespace kota::ipc::protocol {
using ::kota::ipc::protocol::CancelRequestParams;
using ::kota::ipc::protocol::Error;
using ::kota::ipc::protocol::ErrorCode;
using ::kota::ipc::protocol::NotificationTraits;
using ::kota::ipc::protocol::RequestID;
using ::kota::ipc::protocol::RequestTraits;
using ::kota::ipc::protocol::boolean;
using ::kota::ipc::protocol::decimal;
using ::kota::ipc::protocol::integer;
using ::kota::ipc::protocol::null;
using ::kota::ipc::protocol::string;
using ::kota::ipc::protocol::uinteger;
}

export namespace kota::mem {
using ::kota::mem::AllocationGuard;
using ::kota::mem::ConstructedRange;
using ::kota::mem::allocate;
using ::kota::mem::construct_each;
using ::kota::mem::deallocate;
using ::kota::mem::default_construct;
using ::kota::mem::is_bitwise_constructible_v;
using ::kota::mem::move_range;
using ::kota::mem::pointer_in_range;
using ::kota::mem::range_length;
using ::kota::mem::uninitialized_copy;
using ::kota::mem::uninitialized_default_construct;
using ::kota::mem::uninitialized_fill;
using ::kota::mem::uninitialized_move;
using ::kota::mem::uninitialized_value_construct;
}

export namespace kota::mem::detail {
using ::kota::mem::detail::construct_from_each;
using ::kota::mem::detail::copy_bytes;
}

export namespace kota::meta {
using ::kota::meta::annotate;
using ::kota::meta::annotated_type;
using ::kota::meta::annotated_underlying_t;
using ::kota::meta::annotated_value;
using ::kota::meta::annotation;
using ::kota::meta::apply_rename_policy;
using ::kota::meta::array_type_info;
using ::kota::meta::bool_like;
using ::kota::meta::bytes_like;
using ::kota::meta::char_like;
using ::kota::meta::compare_numbers;
using ::kota::meta::declared_repr_t;
using ::kota::meta::default_config;
using ::kota::meta::dynamic;
using ::kota::meta::empty_field_spec;
using ::kota::meta::empty_struct_spec;
using ::kota::meta::enum_name;
using ::kota::meta::enum_strings;
using ::kota::meta::enum_type;
using ::kota::meta::enum_type_info;
using ::kota::meta::enum_value;
using ::kota::meta::eq;
using ::kota::meta::eq_t;
using ::kota::meta::evaluate_skip_predicate;
using ::kota::meta::evaluate_skip_when;
using ::kota::meta::field;
using ::kota::meta::field_addr_of;
using ::kota::meta::field_count;
using ::kota::meta::field_info;
using ::kota::meta::field_name;
using ::kota::meta::field_names;
using ::kota::meta::field_of;
using ::kota::meta::field_offset;
using ::kota::meta::field_refs;
using ::kota::meta::field_slot;
using ::kota::meta::field_spec;
using ::kota::meta::field_spec_of;
using ::kota::meta::field_type;
using ::kota::meta::floating_like;
using ::kota::meta::for_each;
using ::kota::meta::format_config;
using ::kota::meta::format_of_t;
using ::kota::meta::ge;
using ::kota::meta::ge_t;
using ::kota::meta::gt;
using ::kota::meta::gt_t;
using ::kota::meta::has_repr;
using ::kota::meta::inherit_type;
using ::kota::meta::inherit_use_type;
using ::kota::meta::int_like;
using ::kota::meta::is_behavior_attr_v;
using ::kota::meta::is_behavior_provider;
using ::kota::meta::is_floating_kind;
using ::kota::meta::is_integer_kind;
using ::kota::meta::is_object_kind;
using ::kota::meta::is_pair_v;
using ::kota::meta::is_sequence_kind;
using ::kota::meta::is_signed_integer_kind;
using ::kota::meta::is_spec_attr;
using ::kota::meta::is_struct_spec_attr;
using ::kota::meta::is_tuple_v;
using ::kota::meta::is_unsigned_integer_kind;
using ::kota::meta::kind_of;
using ::kota::meta::le;
using ::kota::meta::le_t;
using ::kota::meta::lt;
using ::kota::meta::lt_t;
using ::kota::meta::make_annotation;
using ::kota::meta::make_spec;
using ::kota::meta::make_struct_spec;
using ::kota::meta::map_enum_to_string;
using ::kota::meta::map_string_to_enum;
using ::kota::meta::map_type_info;
using ::kota::meta::member_name;
using ::kota::meta::merged_config_t;
using ::kota::meta::name_list;
using ::kota::meta::ne;
using ::kota::meta::ne_t;
using ::kota::meta::node_config_t;
using ::kota::meta::null_like;
using ::kota::meta::operator==;
using ::kota::meta::optional_type_info;
using ::kota::meta::pointer_name;
using ::kota::meta::reflectable_class;
using ::kota::meta::reflection;
using ::kota::meta::repr;
using ::kota::meta::repr_for;
using ::kota::meta::resolve_tag_names;
using ::kota::meta::resolved_config_t;
using ::kota::meta::resolved_repr_t;
using ::kota::meta::resolves_to_tagged_variant;
using ::kota::meta::schema_number;
using ::kota::meta::schema_opaque;
using ::kota::meta::skip_when;
using ::kota::meta::spec_of;
using ::kota::meta::spec_result;
using ::kota::meta::str_like;
using ::kota::meta::struct_spec;
using ::kota::meta::struct_spec_of;
using ::kota::meta::struct_spec_tag;
using ::kota::meta::struct_type_info;
using ::kota::meta::tag_mode;
using ::kota::meta::tuple_like;
using ::kota::meta::tuple_type_info;
using ::kota::meta::type_info;
using ::kota::meta::type_info_fn;
using ::kota::meta::type_info_of;
using ::kota::meta::type_kind;
using ::kota::meta::type_name;
using ::kota::meta::type_tag;
using ::kota::meta::uint_like;
using ::kota::meta::variant_type_info;
using ::kota::meta::virtual_schema;
using ::kota::meta::wrap_type;
}

export namespace kota::meta::attrs {
using ::kota::meta::attrs::spec;
using ::kota::meta::attrs::struct_spec;
}

export namespace kota::meta::behavior {
using ::kota::meta::behavior::as;
using ::kota::meta::behavior::enum_string;
using ::kota::meta::behavior::skip_if;
using ::kota::meta::behavior::with;
}

export namespace kota::meta::detail {
using ::kota::meta::detail::annotated_field;
using ::kota::meta::detail::any;
using ::kota::meta::detail::any_component;
using ::kota::meta::detail::apply_rename_cx;
using ::kota::meta::detail::as_variant;
using ::kota::meta::detail::build_fields;
using ::kota::meta::detail::build_slots_from_seq;
using ::kota::meta::detail::build_slots_t;
using ::kota::meta::detail::built_fields_t;
using ::kota::meta::detail::compare_eq;
using ::kota::meta::detail::compare_ge;
using ::kota::meta::detail::compare_gt;
using ::kota::meta::detail::compare_le;
using ::kota::meta::detail::compare_lt;
using ::kota::meta::detail::compare_map_entry_eq;
using ::kota::meta::detail::compare_map_entry_lt;
using ::kota::meta::detail::compare_map_eq;
using ::kota::meta::detail::compare_map_eq_unordered;
using ::kota::meta::detail::compare_map_lt;
using ::kota::meta::detail::compare_ne;
using ::kota::meta::detail::compare_range_eq;
using ::kota::meta::detail::compare_range_lt;
using ::kota::meta::detail::compare_sequence_eq;
using ::kota::meta::detail::compare_sequence_lt;
using ::kota::meta::detail::compare_set_eq_unordered;
using ::kota::meta::detail::component_attr;
using ::kota::meta::detail::component_kinds_unique;
using ::kota::meta::detail::config_defaults_fields;
using ::kota::meta::detail::config_denies_unknown;
using ::kota::meta::detail::count_valid;
using ::kota::meta::detail::declared_repr_impl;
using ::kota::meta::detail::defaulted_layer;
using ::kota::meta::detail::deny_layer;
using ::kota::meta::detail::effective_field_count;
using ::kota::meta::detail::effective_field_rename;
using ::kota::meta::detail::effective_field_rename_t;
using ::kota::meta::detail::enum_info_node;
using ::kota::meta::detail::enum_probe;
using ::kota::meta::detail::enum_probe_name;
using ::kota::meta::detail::enum_probe_value;
using ::kota::meta::detail::eq_op;
using ::kota::meta::detail::ext;
using ::kota::meta::detail::extract_identifier;
using ::kota::meta::detail::field_attr_flags;
using ::kota::meta::detail::field_count;
using ::kota::meta::detail::fill_field;
using ::kota::meta::detail::filter_runtime_attrs;
using ::kota::meta::detail::filter_runtime_attrs_t;
using ::kota::meta::detail::finalize_struct_spec;
using ::kota::meta::detail::find_last_top_level_scope;
using ::kota::meta::detail::findable_in;
using ::kota::meta::detail::format_of_impl;
using ::kota::meta::detail::ge_op;
using ::kota::meta::detail::gt_op;
using ::kota::meta::detail::integer_like;
using ::kota::meta::detail::integer_value;
using ::kota::meta::detail::is_runtime_spec_attr_v;
using ::kota::meta::detail::is_runtime_struct_spec_attr_v;
using ::kota::meta::detail::le_op;
using ::kota::meta::detail::lt_op;
using ::kota::meta::detail::make_candidate_names;
using ::kota::meta::detail::make_candidate_valid;
using ::kota::meta::detail::make_candidate_values;
using ::kota::meta::detail::make_field_info;
using ::kota::meta::detail::make_scan_values;
using ::kota::meta::detail::map_entries_contained_in;
using ::kota::meta::detail::merge_config_impl;
using ::kota::meta::detail::merged_config;
using ::kota::meta::detail::op_comparable_with;
using ::kota::meta::detail::range_ref_t;
using ::kota::meta::detail::range_value_op_comparable;
using ::kota::meta::detail::reflectable_pair;
using ::kota::meta::detail::rename_layer;
using ::kota::meta::detail::rename_with;
using ::kota::meta::detail::renamed_name_static;
using ::kota::meta::detail::repr_for_impl;
using ::kota::meta::detail::resolve_field_name;
using ::kota::meta::detail::resolve_node;
using ::kota::meta::detail::resolve_repr;
using ::kota::meta::detail::resolved_repr;
using ::kota::meta::detail::scan_values;
using ::kota::meta::detail::set_entries_contained_in;
using ::kota::meta::detail::single_field_count;
using ::kota::meta::detail::single_field_slots;
using ::kota::meta::detail::standard_integer;
using ::kota::meta::detail::takeover_for_range;
using ::kota::meta::detail::takeover_range_eq;
using ::kota::meta::detail::takeover_range_ge;
using ::kota::meta::detail::takeover_range_gt;
using ::kota::meta::detail::takeover_range_le;
using ::kota::meta::detail::takeover_range_lt;
using ::kota::meta::detail::test;
using ::kota::meta::detail::tuple_gettable;
using ::kota::meta::detail::tuple_gettable_impl;
using ::kota::meta::detail::tuple_info_node;
using ::kota::meta::detail::type_instance;
using ::kota::meta::detail::type_instance_impl;
using ::kota::meta::detail::uninitialized;
using ::kota::meta::detail::unqualify_type_name;
using ::kota::meta::detail::unwrap_annotated;
using ::kota::meta::detail::unwrap_wrapper_value;
using ::kota::meta::detail::validate_attrs;
using ::kota::meta::detail::validate_spec;
using ::kota::meta::detail::variant_info_node;
using ::kota::meta::detail::variant_like;
using ::kota::meta::detail::variant_of;
using ::kota::meta::detail::variant_of_t;
using ::kota::meta::detail::variant_pair;
using ::kota::meta::detail::wrapper;
}

export namespace kota::meta::dsl {
using ::kota::meta::dsl::alias;
using ::kota::meta::dsl::as;
using ::kota::meta::dsl::aspect;
using ::kota::meta::dsl::choices;
using ::kota::meta::dsl::content;
using ::kota::meta::dsl::defaulted;
using ::kota::meta::dsl::defaulted_fields;
using ::kota::meta::dsl::deny_unknown_fields;
using ::kota::meta::dsl::description;
using ::kota::meta::dsl::enum_string;
using ::kota::meta::dsl::flatten;
using ::kota::meta::dsl::idx;
using ::kota::meta::dsl::is_type_component_v;
using ::kota::meta::dsl::maximum;
using ::kota::meta::dsl::minimum;
using ::kota::meta::dsl::name_list_proxy;
using ::kota::meta::dsl::number_proxy;
using ::kota::meta::dsl::rename;
using ::kota::meta::dsl::rename_all;
using ::kota::meta::dsl::schema_default;
using ::kota::meta::dsl::skip;
using ::kota::meta::dsl::skip_if;
using ::kota::meta::dsl::skip_if_proxy;
using ::kota::meta::dsl::spec_component;
using ::kota::meta::dsl::tag;
using ::kota::meta::dsl::tag_names;
using ::kota::meta::dsl::tagged;
using ::kota::meta::dsl::tagged_component;
using ::kota::meta::dsl::tagged_proxy;
using ::kota::meta::dsl::type;
using ::kota::meta::dsl::type_component;
using ::kota::meta::dsl::type_proxy;
using ::kota::meta::dsl::value_component;
using ::kota::meta::dsl::value_proxy;
using ::kota::meta::dsl::with;
}

export namespace kota::meta::pred {
using ::kota::meta::pred::default_value;
using ::kota::meta::pred::empty;
using ::kota::meta::pred::optional_none;
}

export namespace kota::naming {
using ::kota::naming::Casing;
using ::kota::naming::is_alnum;
using ::kota::naming::is_alpha;
using ::kota::naming::is_digit;
using ::kota::naming::is_lower;
using ::kota::naming::is_non_ascii;
using ::kota::naming::is_upper;
using ::kota::naming::normalize_identifier;
using ::kota::naming::normalize_to_lower_snake;
using ::kota::naming::rename_policy_t;
using ::kota::naming::snake_to_camel;
using ::kota::naming::snake_to_upper;
using ::kota::naming::to_lower;
using ::kota::naming::to_upper;
}

export namespace kota::naming::detail {
using ::kota::naming::detail::casing_policy;
}

export namespace kota::naming::rename_policy {
using ::kota::naming::rename_policy::identity;
using ::kota::naming::rename_policy::lower_camel;
using ::kota::naming::rename_policy::lower_snake;
using ::kota::naming::rename_policy::upper_camel;
using ::kota::naming::rename_policy::upper_snake;
}

export namespace kota::option {
using ::kota::option::ArgsRef;
using ::kota::option::DriverFlag;
using ::kota::option::DriverVisibility;
using ::kota::option::Kind;
using ::kota::option::OptTable;
using ::kota::option::Option;
using ::kota::option::OptionRef;
using ::kota::option::ParseError;
using ::kota::option::ParseIter;
using ::kota::option::ParseOptions;
using ::kota::option::ParsedArg;
using ::kota::option::RenderStyle;
using ::kota::option::pfx_all;
using ::kota::option::pfx_dash;
using ::kota::option::pfx_dash_double;
using ::kota::option::pfx_double;
using ::kota::option::pfx_none;
using ::kota::option::pfx_slash_dash;
using enum ::kota::option::DriverFlag;
using enum ::kota::option::DriverVisibility;
}

export namespace kota::option::detail {
using ::kota::option::detail::all_prefixes;
using ::kota::option::detail::dash_double_prefixes;
using ::kota::option::detail::dash_prefixes;
using ::kota::option::detail::double_prefixes;
using ::kota::option::detail::slash_dash_prefixes;
}

export namespace kota::sys {
using ::kota::sys::Child;
using ::kota::sys::CreationOptions;
using ::kota::sys::EnvChange;
using ::kota::sys::EnvVar;
using ::kota::sys::ExitStatus;
using ::kota::sys::ProgramSearch;
using ::kota::sys::SpawnOptions;
using ::kota::sys::Stdio;
using ::kota::sys::arguments;
using ::kota::sys::cache_dir;
using ::kota::sys::capture;
using ::kota::sys::config_dir;
using ::kota::sys::data_dir;
using ::kota::sys::env_name_equal;
using ::kota::sys::environment;
using ::kota::sys::executable_path;
using ::kota::sys::expand_tilde;
using ::kota::sys::find_program;
using ::kota::sys::get_env;
using ::kota::sys::home_dir;
using ::kota::sys::join_paths;
using ::kota::sys::parent_pid;
using ::kota::sys::pid;
using ::kota::sys::process_alive;
using ::kota::sys::self_executable;
using ::kota::sys::set_env;
using ::kota::sys::set_process_name;
using ::kota::sys::set_process_title;
using ::kota::sys::signal_name;
using ::kota::sys::spawn;
using ::kota::sys::split_paths;
using ::kota::sys::split_paths_view;
using ::kota::sys::started_path;
using ::kota::sys::temp_dir;
using ::kota::sys::unset_env;
}

export namespace kota::zest {
using ::kota::zest::CaseRegistrar;
using ::kota::zest::Context;
using ::kota::zest::FatalHook;
using ::kota::zest::Match;
using ::kota::zest::Options;
using ::kota::zest::Runner;
using ::kota::zest::TestAttrs;
using ::kota::zest::TestCase;
using ::kota::zest::TestState;
using ::kota::zest::TestSuite;
using ::kota::zest::TestSuiteDef;
using ::kota::zest::contains;
using ::kota::zest::current_test_state;
using ::kota::zest::describe_exception;
using ::kota::zest::ends_with;
using ::kota::zest::failure;
using ::kota::zest::merge_attrs;
using ::kota::zest::pretty_dump;
using ::kota::zest::print_trace;
using ::kota::zest::run_cli;
using ::kota::zest::run_tests;
using ::kota::zest::skip;
using ::kota::zest::snapshot;
using ::kota::zest::snapshot_glob;
using ::kota::zest::starts_with;
using ::kota::zest::strip_test_prefix;
using ::kota::zest::throws;
using ::kota::zest::trace_exception;
using ::kota::zest::type_eq;
}

export namespace kota::zest::detail {
using ::kota::zest::detail::Captured;
using ::kota::zest::detail::Comparison;
using ::kota::zest::detail::Decomposer;
using ::kota::zest::detail::Operand;
using ::kota::zest::detail::Relation;
using ::kota::zest::detail::ReportLine;
using ::kota::zest::detail::as_pattern;
using ::kota::zest::detail::as_text;
using ::kota::zest::detail::check;
using ::kota::zest::detail::check_fatal;
using ::kota::zest::detail::check_snapshot;
using ::kota::zest::detail::cleanup_unused_snapshots;
using ::kota::zest::detail::compares_as_text;
using ::kota::zest::detail::fail_fatally;
using ::kota::zest::detail::is_c_string;
using ::kota::zest::detail::keep_registered;
using ::kota::zest::detail::reject_bitwise;
using ::kota::zest::detail::reject_chain;
using ::kota::zest::detail::reject_logic;
using ::kota::zest::detail::reject_shift;
using ::kota::zest::detail::relate;
using ::kota::zest::detail::report_failure;
using ::kota::zest::detail::reset_snapshot_context;
using ::kota::zest::detail::set_snapshot_dir;
using ::kota::zest::detail::set_update_snapshots;
using ::kota::zest::detail::snapshot_text;
}

export namespace simdjson {
using ::simdjson::ErrorValues;
using ::simdjson::builtin_implementation;
using ::simdjson::custom_deserializable;
using ::simdjson::deserializable;
using ::simdjson::deserialize;
using ::simdjson::deserialize_tag;
using ::simdjson::error_code;
using ::simdjson::error_message;
using ::simdjson::get_active_implementation;
using ::simdjson::get_available_implementations;
using ::simdjson::get_next_key_and_json_path;
using ::simdjson::has_custom_serialization;
using ::simdjson::implementation;
using ::simdjson::is_builtin_deserializable;
using ::simdjson::is_builtin_deserializable_v;
using ::simdjson::is_fatal;
using ::simdjson::is_streaming;
using ::simdjson::json_path_to_pointer_conversion;
using ::simdjson::minify;
using ::simdjson::nothrow_custom_deserializable;
using ::simdjson::nothrow_deserializable;
using ::simdjson::nothrow_tag_invocable;
using ::simdjson::operator<<;
using ::simdjson::pad;
using ::simdjson::pad_with_reserve;
using ::simdjson::padded_string;
using ::simdjson::padded_string_view;
using ::simdjson::prettify;
using ::simdjson::require_custom_serialization;
using ::simdjson::serialize;
using ::simdjson::serialize_tag;
using ::simdjson::simdjson_error;
using ::simdjson::simdjson_result;
using ::simdjson::stage1_mode;
using ::simdjson::tag_invocable;
using ::simdjson::tag_invoke;
using ::simdjson::to_json;
using ::simdjson::to_json_string;
using ::simdjson::to_string;
using ::simdjson::trim;
using ::simdjson::validate_utf8;
using enum ::simdjson::error_code;
using enum unnamed_enum<decltype(::simdjson::SIMDJSON_VERSION_MAJOR)>;
}

export namespace simdjson::concepts {
using ::simdjson::concepts::appendable_containers;
using ::simdjson::concepts::constructible_from_string_view;
using ::simdjson::concepts::container_but_not_string;
using ::simdjson::concepts::emplace_one;
using ::simdjson::concepts::indexable_container;
using ::simdjson::concepts::indexable_container_v;
using ::simdjson::concepts::is_pair;
using ::simdjson::concepts::optional_type;
using ::simdjson::concepts::returns_reference;
using ::simdjson::concepts::smart_pointer;
using ::simdjson::concepts::string_like;
using ::simdjson::concepts::string_view_keyed_map;
using ::simdjson::concepts::string_view_like;
}

export namespace simdjson::concepts::details {
using ::simdjson::concepts::details::supports_add;
using ::simdjson::concepts::details::supports_append;
using ::simdjson::concepts::details::supports_emplace;
using ::simdjson::concepts::details::supports_emplace_back;
using ::simdjson::concepts::details::supports_insert;
using ::simdjson::concepts::details::supports_op_append;
using ::simdjson::concepts::details::supports_push;
using ::simdjson::concepts::details::supports_push_back;
}

export namespace simdjson::constevalutil {
using ::simdjson::constevalutil::consteval_to_quoted_escaped;
using ::simdjson::constevalutil::fixed_string;
using ::simdjson::constevalutil::string_constant;
}

export namespace simdjson::convert::internal {
using ::simdjson::convert::internal::auto_parser;
using ::simdjson::convert::internal::to_adaptor;
}

export namespace simdjson::dom {
using ::simdjson::dom::array;
using ::simdjson::dom::document;
using ::simdjson::dom::document_stream;
using ::simdjson::dom::element;
using ::simdjson::dom::element_type;
using ::simdjson::dom::is_pointer_well_formed;
using ::simdjson::dom::key_value_pair;
using ::simdjson::dom::object;
using ::simdjson::dom::operator<<;
using ::simdjson::dom::parser;
}

export namespace simdjson::internal {
using ::simdjson::internal::BitsSetTable256mul2;
using ::simdjson::internal::allocate_padded_buffer;
using ::simdjson::internal::atomic_ptr;
using ::simdjson::internal::available_implementation_list;
using ::simdjson::internal::base_formatter;
using ::simdjson::internal::digit_to_val32;
using ::simdjson::internal::dom_parser_implementation;
using ::simdjson::internal::error_code_info;
using ::simdjson::internal::error_codes;
using ::simdjson::internal::escape_json_string;
using ::simdjson::internal::from_chars;
using ::simdjson::internal::instruction_set;
using ::simdjson::internal::mini_formatter;
using ::simdjson::internal::operator<<;
using ::simdjson::internal::power_of_five_128;
using ::simdjson::internal::power_of_ten;
using ::simdjson::internal::pretty_formatter;
using ::simdjson::internal::pshufb_combine_table;
using ::simdjson::internal::simdjson_result_base;
using ::simdjson::internal::string_builder;
using ::simdjson::internal::structural_or_whitespace;
using ::simdjson::internal::structural_or_whitespace_negated;
using ::simdjson::internal::tape_ref;
using ::simdjson::internal::tape_type;
using ::simdjson::internal::thintable_epi8;
using ::simdjson::internal::to_chars;
using ::simdjson::internal::value128;
using enum ::simdjson::internal::instruction_set;
}

export namespace spdlog {
namespace fmt_lib = ::std;
using ::spdlog::apply_all;
using ::spdlog::apply_logger_env_levels;
using ::spdlog::color_mode;
using ::spdlog::create;
using ::spdlog::critical;
using ::spdlog::debug;
using ::spdlog::default_factory;
using ::spdlog::default_logger;
using ::spdlog::default_logger_raw;
using ::spdlog::disable_backtrace;
using ::spdlog::drop;
using ::spdlog::drop_all;
using ::spdlog::dump_backtrace;
using ::spdlog::enable_backtrace;
using ::spdlog::err_handler;
using ::spdlog::error;
using ::spdlog::file_event_handlers;
using ::spdlog::filename_t;
using ::spdlog::flush_every;
using ::spdlog::flush_on;
using ::spdlog::format_string_t;
using ::spdlog::formatter;
using ::spdlog::get;
using ::spdlog::get_level;
using ::spdlog::info;
using ::spdlog::initialize_logger;
using ::spdlog::is_convertible_to_any_format_string;
using ::spdlog::is_convertible_to_basic_format_string;
using ::spdlog::level_t;
using ::spdlog::log;
using ::spdlog::log_clock;
using ::spdlog::logger;
using ::spdlog::memory_buf_t;
using ::spdlog::pattern_time_type;
using ::spdlog::register_logger;
using ::spdlog::register_or_replace;
using ::spdlog::set_automatic_registration;
using ::spdlog::set_default_logger;
using ::spdlog::set_error_handler;
using ::spdlog::set_formatter;
using ::spdlog::set_level;
using ::spdlog::set_pattern;
using ::spdlog::should_log;
using ::spdlog::shutdown;
using ::spdlog::sink_ptr;
using ::spdlog::sinks_init_list;
using ::spdlog::source_loc;
using ::spdlog::spdlog_ex;
using ::spdlog::string_view_t;
using ::spdlog::swap;
using ::spdlog::synchronous_factory;
using ::spdlog::throw_spdlog_ex;
using ::spdlog::trace;
using ::spdlog::warn;
}

export namespace spdlog::details {
using ::spdlog::details::backtracer;
using ::spdlog::details::circular_q;
using ::spdlog::details::conditional_static_cast;
using ::spdlog::details::enable_if_t;
using ::spdlog::details::log_msg;
using ::spdlog::details::log_msg_buffer;
using ::spdlog::details::make_unique;
using ::spdlog::details::null_atomic_int;
using ::spdlog::details::null_mutex;
using ::spdlog::details::periodic_worker;
using ::spdlog::details::registry;
using ::spdlog::details::thread_pool;
using ::spdlog::details::to_string_view;
}

export namespace spdlog::level {
using ::spdlog::level::from_str;
using ::spdlog::level::level_enum;
using ::spdlog::level::to_short_c_str;
using ::spdlog::level::to_string_view;
using enum ::spdlog::level::level_enum;
}

export namespace spdlog::sinks {
using ::spdlog::sinks::sink;
}

#if (defined(__linux__) && defined(__x86_64__) && defined(NDEBUG)) || (defined(__linux__) && defined(__x86_64__) && !defined(NDEBUG)) || (defined(__APPLE__) && defined(__aarch64__) && defined(NDEBUG)) || (defined(_WIN32) && defined(__x86_64__) && defined(NDEBUG)) || (defined(_WIN32) && defined(__x86_64__) && !defined(NDEBUG)) || (defined(_WIN32) && defined(__aarch64__) && defined(NDEBUG))
export namespace simdjson::dom {
using ::simdjson::dom::stage1_worker;
}
#endif

#if (defined(__linux__) && defined(__x86_64__) && defined(NDEBUG)) || (defined(__linux__) && defined(__x86_64__) && !defined(NDEBUG)) || (defined(_WIN32) && defined(__x86_64__) && defined(NDEBUG)) || (defined(_WIN32) && defined(__x86_64__) && !defined(NDEBUG))
export namespace simdjson {
namespace builder = ::simdjson::fallback::builder;
namespace builtin = ::simdjson::fallback;
namespace ondemand = ::simdjson::fallback::ondemand;
}

export namespace simdjson::fallback {
using ::simdjson::fallback::dom_parser_implementation;
using ::simdjson::fallback::implementation;
using ::simdjson::fallback::implementation_simdjson_result_base;
using ::simdjson::fallback::number_type;
using ::simdjson::fallback::open_container;
using ::simdjson::fallback::operator<<;
}

export namespace simdjson::fallback::builder {
using ::simdjson::fallback::builder::escape_json_char;
using ::simdjson::fallback::builder::fast_needs_escaping;
using ::simdjson::fallback::builder::find_next_json_quotable_character;
using ::simdjson::fallback::builder::simple_needs_escaping;
using ::simdjson::fallback::builder::string_builder;
using ::simdjson::fallback::builder::write_string_escaped;
}

export namespace simdjson::fallback::builder::internal {
using ::simdjson::fallback::builder::internal::digit_count;
using ::simdjson::fallback::builder::internal::fast_digit_count_32;
using ::simdjson::fallback::builder::internal::fast_digit_count_64;
using ::simdjson::fallback::builder::internal::int_log2;
}

export namespace simdjson::fallback::numberparsing {
using ::simdjson::fallback::numberparsing::full_multiplication;
using ::simdjson::fallback::numberparsing::parse_number;
using ::simdjson::fallback::numberparsing::write_float;
}

export namespace simdjson::fallback::ondemand {
using ::simdjson::fallback::ondemand::array;
using ::simdjson::fallback::ondemand::array_iterator;
using ::simdjson::fallback::ondemand::depth_t;
using ::simdjson::fallback::ondemand::document;
using ::simdjson::fallback::ondemand::document_reference;
using ::simdjson::fallback::ondemand::document_stream;
using ::simdjson::fallback::ondemand::field;
using ::simdjson::fallback::ondemand::is_pointer_well_formed;
using ::simdjson::fallback::ondemand::json_iterator;
using ::simdjson::fallback::ondemand::json_type;
using ::simdjson::fallback::ondemand::number;
using ::simdjson::fallback::ondemand::number_type;
using ::simdjson::fallback::ondemand::object;
using ::simdjson::fallback::ondemand::object_iterator;
using ::simdjson::fallback::ondemand::operator!=;
using ::simdjson::fallback::ondemand::operator<<;
using ::simdjson::fallback::ondemand::operator==;
using ::simdjson::fallback::ondemand::parser;
using ::simdjson::fallback::ondemand::raw_json_string;
using ::simdjson::fallback::ondemand::release_parser;
using ::simdjson::fallback::ondemand::stage1_worker;
using ::simdjson::fallback::ondemand::token_iterator;
using ::simdjson::fallback::ondemand::token_position;
using ::simdjson::fallback::ondemand::value;
using ::simdjson::fallback::ondemand::value_iterator;
}

export namespace simdjson::fallback::ondemand::logger {
using ::simdjson::fallback::ondemand::logger::log_level;
}
#endif

#if (defined(__APPLE__) && defined(__aarch64__) && defined(NDEBUG)) || (defined(__APPLE__) && defined(__aarch64__) && !defined(NDEBUG)) || (defined(_WIN32) && defined(__aarch64__) && defined(NDEBUG))
export namespace simdjson {
namespace builder = ::simdjson::arm64::builder;
namespace builtin = ::simdjson::arm64;
namespace ondemand = ::simdjson::arm64::ondemand;
}

export namespace simdjson::arm64 {
using ::simdjson::arm64::dom_parser_implementation;
using ::simdjson::arm64::implementation;
using ::simdjson::arm64::implementation_simdjson_result_base;
using ::simdjson::arm64::number_type;
using ::simdjson::arm64::open_container;
using ::simdjson::arm64::operator<<;
}

export namespace simdjson::arm64::builder {
using ::simdjson::arm64::builder::escape_json_char;
using ::simdjson::arm64::builder::fast_needs_escaping;
using ::simdjson::arm64::builder::find_next_json_quotable_character;
using ::simdjson::arm64::builder::simple_needs_escaping;
using ::simdjson::arm64::builder::string_builder;
using ::simdjson::arm64::builder::write_string_escaped;
}

export namespace simdjson::arm64::builder::internal {
using ::simdjson::arm64::builder::internal::digit_count;
using ::simdjson::arm64::builder::internal::fast_digit_count_32;
using ::simdjson::arm64::builder::internal::fast_digit_count_64;
using ::simdjson::arm64::builder::internal::int_log2;
}

export namespace simdjson::arm64::numberparsing {
using ::simdjson::arm64::numberparsing::full_multiplication;
using ::simdjson::arm64::numberparsing::parse_number;
using ::simdjson::arm64::numberparsing::write_float;
}

export namespace simdjson::arm64::ondemand {
using ::simdjson::arm64::ondemand::array;
using ::simdjson::arm64::ondemand::array_iterator;
using ::simdjson::arm64::ondemand::depth_t;
using ::simdjson::arm64::ondemand::document;
using ::simdjson::arm64::ondemand::document_reference;
using ::simdjson::arm64::ondemand::document_stream;
using ::simdjson::arm64::ondemand::field;
using ::simdjson::arm64::ondemand::is_pointer_well_formed;
using ::simdjson::arm64::ondemand::json_iterator;
using ::simdjson::arm64::ondemand::json_type;
using ::simdjson::arm64::ondemand::number;
using ::simdjson::arm64::ondemand::number_type;
using ::simdjson::arm64::ondemand::object;
using ::simdjson::arm64::ondemand::object_iterator;
using ::simdjson::arm64::ondemand::operator!=;
using ::simdjson::arm64::ondemand::operator<<;
using ::simdjson::arm64::ondemand::operator==;
using ::simdjson::arm64::ondemand::parser;
using ::simdjson::arm64::ondemand::raw_json_string;
using ::simdjson::arm64::ondemand::release_parser;
using ::simdjson::arm64::ondemand::token_iterator;
using ::simdjson::arm64::ondemand::token_position;
using ::simdjson::arm64::ondemand::value;
using ::simdjson::arm64::ondemand::value_iterator;
}

export namespace simdjson::arm64::ondemand::logger {
using ::simdjson::arm64::ondemand::logger::log_level;
}
#endif

#if (defined(_WIN32) && defined(__x86_64__) && defined(NDEBUG)) || (defined(_WIN32) && defined(__x86_64__) && !defined(NDEBUG)) || (defined(_WIN32) && defined(__aarch64__) && defined(NDEBUG))
export namespace kota::sys::windows {
using ::kota::sys::windows::CommandLineRules;
using ::kota::sys::windows::create_process_directories;
using ::kota::sys::windows::find_create_process_program;
using ::kota::sys::windows::join_command_line;
using ::kota::sys::windows::ntstatus_name;
using ::kota::sys::windows::quote_argument;
using ::kota::sys::windows::split_command_line;
using ::kota::sys::windows::split_command_line_view;
}
#endif

#if (defined(__APPLE__) && defined(__aarch64__) && defined(NDEBUG)) || (defined(_WIN32) && defined(__aarch64__) && defined(NDEBUG))
export namespace simdjson::arm64::ondemand {
using ::simdjson::arm64::ondemand::stage1_worker;
}
#endif
