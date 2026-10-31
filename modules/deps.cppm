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

export namespace kotatsu {
using ::kotatsu::ScopeExit;
using ::kotatsu::UTF8Sequence;
using ::kotatsu::any_word;
using ::kotatsu::bad_outcome_access;
using ::kotatsu::basic_buffer_ref;
using ::kotatsu::bind;
using ::kotatsu::bind_ref;
using ::kotatsu::buffer_ref;
using ::kotatsu::bytes_below;
using ::kotatsu::bytes_equal;
using ::kotatsu::callable_args_count_v;
using ::kotatsu::callable_args_t;
using ::kotatsu::callable_return_t;
using ::kotatsu::callable_traits;
using ::kotatsu::class_type_t;
using ::kotatsu::compare_exact;
using ::kotatsu::decode_utf8;
using ::kotatsu::dependent_false;
using ::kotatsu::eq_comparable_with;
using ::kotatsu::first_byte_index;
using ::kotatsu::format_kind;
using ::kotatsu::from_utf16;
using ::kotatsu::function;
using ::kotatsu::function_args_count_v;
using ::kotatsu::function_args_t;
using ::kotatsu::function_ref;
using ::kotatsu::function_return_t;
using ::kotatsu::function_traits;
using ::kotatsu::ge_comparable_with;
using ::kotatsu::gt_comparable_with;
using ::kotatsu::has_byte;
using ::kotatsu::has_non_ascii;
using ::kotatsu::hybrid_vector;
using ::kotatsu::is_expected_v;
using ::kotatsu::is_function_pointer_v;
using ::kotatsu::is_functor_v;
using ::kotatsu::is_map_value_v;
using ::kotatsu::is_mem_fn_of;
using ::kotatsu::is_one_of;
using ::kotatsu::is_optional_v;
using ::kotatsu::is_outcome_v;
using ::kotatsu::is_specialization_of;
using ::kotatsu::is_utf8;
using ::kotatsu::le_comparable_with;
using ::kotatsu::load_word;
using ::kotatsu::lt_comparable_with;
using ::kotatsu::map_entry_key_t;
using ::kotatsu::map_entry_mapped_t;
using ::kotatsu::map_range;
using ::kotatsu::mem_fn;
using ::kotatsu::member_traits;
using ::kotatsu::member_type_t;
using ::kotatsu::narrow_int;
using ::kotatsu::ordered_associative_range;
using ::kotatsu::ordered_map_range;
using ::kotatsu::ordered_set_range;
using ::kotatsu::outcome;
using ::kotatsu::outcome_cancel;
using ::kotatsu::outcome_cancel_t;
using ::kotatsu::outcome_error;
using ::kotatsu::outcome_error_t;
using ::kotatsu::outcome_ok_tag;
using ::kotatsu::outcome_value;
using ::kotatsu::range_format;
using ::kotatsu::range_of_kind;
using ::kotatsu::replace_invalid_utf8;
using ::kotatsu::sequence_range;
using ::kotatsu::set_range;
using ::kotatsu::small_string;
using ::kotatsu::small_vector;
using ::kotatsu::small_wstring;
using ::kotatsu::string_buffer;
using ::kotatsu::string_list_ref;
using ::kotatsu::string_ref;
using ::kotatsu::to_utf16;
using ::kotatsu::tuple_any_of_v;
using ::kotatsu::tuple_count_of_v;
using ::kotatsu::tuple_find;
using ::kotatsu::tuple_find_spec;
using ::kotatsu::tuple_find_spec_t;
using ::kotatsu::tuple_find_t;
using ::kotatsu::tuple_has_spec_v;
using ::kotatsu::tuple_has_v;
using ::kotatsu::tuple_push_front;
using ::kotatsu::tuple_push_front_t;
using ::kotatsu::type_list;
using ::kotatsu::type_list_cat;
using ::kotatsu::type_list_cat_t;
using ::kotatsu::type_list_concat;
using ::kotatsu::type_list_concat_t;
using ::kotatsu::type_list_contains;
using ::kotatsu::type_list_contains_v;
using ::kotatsu::type_list_element;
using ::kotatsu::type_list_element_t;
using ::kotatsu::type_list_filter;
using ::kotatsu::type_list_filter_t;
using ::kotatsu::type_list_prepend;
using ::kotatsu::type_list_prepend_t;
using ::kotatsu::type_list_size;
using ::kotatsu::type_list_size_v;
using ::kotatsu::type_list_to_union;
using ::kotatsu::type_list_to_union_t;
using ::kotatsu::type_list_unique;
using ::kotatsu::type_list_unique_impl;
using ::kotatsu::type_list_unique_t;
using ::kotatsu::unordered_associative_range;
using ::kotatsu::unordered_map_range;
using ::kotatsu::unordered_set_range;
using ::kotatsu::vector;
}

export namespace kotatsu::async {
using ::kotatsu::async::acceptor;
using ::kotatsu::async::aggregate_op;
using ::kotatsu::async::async_node;
using ::kotatsu::async::async_visitor;
using ::kotatsu::async::cancel;
using ::kotatsu::async::cancellation;
using ::kotatsu::async::cancellation_callback;
using ::kotatsu::async::cancellation_source;
using ::kotatsu::async::cancellation_token;
using ::kotatsu::async::check;
using ::kotatsu::async::co_invoke;
using ::kotatsu::async::condition_variable;
using ::kotatsu::async::console;
using ::kotatsu::async::dump_dot;
using ::kotatsu::async::endpoint;
using ::kotatsu::async::error;
using ::kotatsu::async::event;
using ::kotatsu::async::event_loop;
using ::kotatsu::async::fail;
using ::kotatsu::async::guess_handle;
using ::kotatsu::async::handle_type;
using ::kotatsu::async::idle;
using ::kotatsu::async::io_op;
using ::kotatsu::async::mutex;
using ::kotatsu::async::or_fail;
using ::kotatsu::async::pipe;
using ::kotatsu::async::prepare;
using ::kotatsu::async::process;
using ::kotatsu::async::queue;
using ::kotatsu::async::relay;
using ::kotatsu::async::result;
using ::kotatsu::async::run;
using ::kotatsu::async::semaphore;
using ::kotatsu::async::signal;
using ::kotatsu::async::sleep;
using ::kotatsu::async::stream;
using ::kotatsu::async::sync_primitive;
using ::kotatsu::async::task;
using ::kotatsu::async::task_frame;
using ::kotatsu::async::task_group;
using ::kotatsu::async::tcp;
using ::kotatsu::async::timer;
using ::kotatsu::async::udp;
using ::kotatsu::async::wait_node;
using ::kotatsu::async::watcher;
using ::kotatsu::async::when_all;
using ::kotatsu::async::when_any;
using ::kotatsu::async::with_task_group;
using ::kotatsu::async::with_timeout;
using ::kotatsu::async::with_token;
using ::kotatsu::async::yield;
using ::kotatsu::async::yield_awaiter;
}

export namespace kotatsu::async::detail {
using ::kotatsu::async::detail::BasicGuard;
using ::kotatsu::async::detail::CancellationNode;
using ::kotatsu::async::detail::CancellationState;
using ::kotatsu::async::detail::ScopedAwaiter;
using ::kotatsu::async::detail::aggregate_result_t;
using ::kotatsu::async::detail::async_range;
using ::kotatsu::async::detail::await_probe_handle;
using ::kotatsu::async::detail::await_probe_promise;
using ::kotatsu::async::detail::await_result_t;
using ::kotatsu::async::detail::awaitable;
using ::kotatsu::async::detail::awaiter;
using ::kotatsu::async::detail::awaiter_t;
using ::kotatsu::async::detail::call_result_t;
using ::kotatsu::async::detail::cancel_await;
using ::kotatsu::async::detail::destroy_handle;
using ::kotatsu::async::detail::expire;
using ::kotatsu::async::detail::fail_await;
using ::kotatsu::async::detail::failing;
using ::kotatsu::async::detail::get_awaiter;
using ::kotatsu::async::detail::group_error;
using ::kotatsu::async::detail::is_group_body_v;
using ::kotatsu::async::detail::is_plain_task_v;
using ::kotatsu::async::detail::is_task_v;
using ::kotatsu::async::detail::keep_non_void;
using ::kotatsu::async::detail::loop_access;
using ::kotatsu::async::detail::merged_channel_t;
using ::kotatsu::async::detail::normalize;
using ::kotatsu::async::detail::normalized_range_task_t;
using ::kotatsu::async::detail::normalized_task;
using ::kotatsu::async::detail::normalized_task_t;
using ::kotatsu::async::detail::or_fail_await;
using ::kotatsu::async::detail::or_fail_result;
using ::kotatsu::async::detail::or_fail_task;
using ::kotatsu::async::detail::or_fail_task_await;
using ::kotatsu::async::detail::promise_result;
using ::kotatsu::async::detail::range_tasks;
using ::kotatsu::async::detail::run_on_pool;
using ::kotatsu::async::detail::run_until;
using ::kotatsu::async::detail::success_t;
using ::kotatsu::async::detail::task_access;
using ::kotatsu::async::detail::task_promise;
using ::kotatsu::async::detail::task_return_object;
using ::kotatsu::async::detail::unique_handle;
using ::kotatsu::async::detail::when_children;
using ::kotatsu::async::detail::when_op;
}

export namespace kotatsu::async::fs {
using ::kotatsu::async::fs::access;
using ::kotatsu::async::fs::chmod;
using ::kotatsu::async::fs::chown;
using ::kotatsu::async::fs::close;
using ::kotatsu::async::fs::copyfile;
using ::kotatsu::async::fs::copyfile_options;
using ::kotatsu::async::fs::dir_handle;
using ::kotatsu::async::fs::dirent;
using ::kotatsu::async::fs::fchmod;
using ::kotatsu::async::fs::fchown;
using ::kotatsu::async::fs::fdatasync;
using ::kotatsu::async::fs::file_stats;
using ::kotatsu::async::fs::file_time;
using ::kotatsu::async::fs::fs_stats;
using ::kotatsu::async::fs::fstat;
using ::kotatsu::async::fs::fsync;
using ::kotatsu::async::fs::ftruncate;
using ::kotatsu::async::fs::futime;
using ::kotatsu::async::fs::lchown;
using ::kotatsu::async::fs::link;
using ::kotatsu::async::fs::lstat;
using ::kotatsu::async::fs::lutime;
using ::kotatsu::async::fs::mkdir;
using ::kotatsu::async::fs::mkdtemp;
using ::kotatsu::async::fs::mkstemp;
using ::kotatsu::async::fs::mkstemp_result;
using ::kotatsu::async::fs::open;
using ::kotatsu::async::fs::opendir;
using ::kotatsu::async::fs::read;
using ::kotatsu::async::fs::readdir;
using ::kotatsu::async::fs::readlink;
using ::kotatsu::async::fs::realpath;
using ::kotatsu::async::fs::rename;
using ::kotatsu::async::fs::rmdir;
using ::kotatsu::async::fs::scandir;
using ::kotatsu::async::fs::sendfile;
using ::kotatsu::async::fs::stat;
using ::kotatsu::async::fs::statfs;
using ::kotatsu::async::fs::symlink;
using ::kotatsu::async::fs::unlink;
using ::kotatsu::async::fs::utime;
using ::kotatsu::async::fs::write;
}

export namespace kotatsu::codec {
using ::kotatsu::codec::FieldName;
using ::kotatsu::codec::MapKeyReader;
using ::kotatsu::codec::MapKeyWriter;
using ::kotatsu::codec::RawValue;
using ::kotatsu::codec::StringKeySink;
using ::kotatsu::codec::UnknownFields;
using ::kotatsu::codec::apply_enum_rename;
using ::kotatsu::codec::char_from_utf8;
using ::kotatsu::codec::char_to_utf8;
using ::kotatsu::codec::decode_struct_fields;
using ::kotatsu::codec::decode_value;
using ::kotatsu::codec::decode_variant;
using ::kotatsu::codec::default_config;
using ::kotatsu::codec::deserialize_visit;
using ::kotatsu::codec::encode_struct_fields;
using ::kotatsu::codec::encode_value;
using ::kotatsu::codec::enum_repr;
using ::kotatsu::codec::invalid_char_message;
using ::kotatsu::codec::invalid_utf8;
using ::kotatsu::codec::is_human_readable;
using ::kotatsu::codec::layout_computed;
using ::kotatsu::codec::nan_repr;
using ::kotatsu::codec::rich_error;
using ::kotatsu::codec::scoped_context;
using ::kotatsu::codec::serialize_visit;
using ::kotatsu::codec::writes_every_field;
using ::kotatsu::codec::writes_utf8;
}

export namespace kotatsu::codec::bincode {
using ::kotatsu::codec::bincode::MapAccess;
using ::kotatsu::codec::bincode::Reader;
using ::kotatsu::codec::bincode::SeqAccess;
using ::kotatsu::codec::bincode::Writer;
using ::kotatsu::codec::bincode::format;
using ::kotatsu::codec::bincode::from_bytes;
using ::kotatsu::codec::bincode::to_bytes;
}

export namespace kotatsu::codec::bincode::detail {
using ::kotatsu::codec::bincode::detail::LengthMarker;
using ::kotatsu::codec::bincode::detail::invalid_length;
using ::kotatsu::codec::bincode::detail::number_out_of_range;
using ::kotatsu::codec::bincode::detail::trailing_bytes;
using ::kotatsu::codec::bincode::detail::type_mismatch;
using ::kotatsu::codec::bincode::detail::unexpected_eof;
}

export namespace kotatsu::codec::debug {
using ::kotatsu::codec::debug::Formatter;
using ::kotatsu::codec::debug::MapWriter;
using ::kotatsu::codec::debug::SeqWriter;
using ::kotatsu::codec::debug::StructWriter;
using ::kotatsu::codec::debug::TupleWriter;
using ::kotatsu::codec::debug::ValueWriter;
using ::kotatsu::codec::debug::to_string;
}

export namespace kotatsu::codec::detail {
using ::kotatsu::codec::detail::FieldAt;
using ::kotatsu::codec::detail::assert_human_readable_allowed;
using ::kotatsu::codec::detail::assert_internal_tag_fits;
using ::kotatsu::codec::detail::check_required_fields;
using ::kotatsu::codec::detail::construct_and_visit;
using ::kotatsu::codec::detail::data_driven;
using ::kotatsu::codec::detail::decode_adjacently_tagged;
using ::kotatsu::codec::detail::decode_enum_name;
using ::kotatsu::codec::detail::decode_externally_tagged;
using ::kotatsu::codec::detail::decode_field_value;
using ::kotatsu::codec::detail::decode_internally_tagged;
using ::kotatsu::codec::detail::decode_one_field;
using ::kotatsu::codec::detail::decode_step;
using ::kotatsu::codec::detail::decode_untagged_variant;
using ::kotatsu::codec::detail::decode_with_attrs;
using ::kotatsu::codec::detail::empty_config_base;
using ::kotatsu::codec::detail::encode_one_field;
using ::kotatsu::codec::detail::encode_tagged_variant;
using ::kotatsu::codec::detail::encode_text;
using ::kotatsu::codec::detail::encode_untagged_variant;
using ::kotatsu::codec::detail::encode_with_attrs;
using ::kotatsu::codec::detail::ensure_allocated;
using ::kotatsu::codec::detail::fail_unknown_tag;
using ::kotatsu::codec::detail::fail_unnamed_enum;
using ::kotatsu::codec::detail::fail_unusable_tag;
using ::kotatsu::codec::detail::find_field_slot;
using ::kotatsu::codec::detail::find_tag_index;
using ::kotatsu::codec::detail::has_field_named;
using ::kotatsu::codec::detail::has_native_variant;
using ::kotatsu::codec::detail::has_peek_kind;
using ::kotatsu::codec::detail::has_try_read;
using ::kotatsu::codec::detail::human_readable_allowed;
using ::kotatsu::codec::detail::kind_compatible;
using ::kotatsu::codec::detail::kind_compatible_impl;
using ::kotatsu::codec::detail::match_field;
using ::kotatsu::codec::detail::nested_alternative_pass;
using ::kotatsu::codec::detail::no_tag;
using ::kotatsu::codec::detail::pass_unknown_field;
using ::kotatsu::codec::detail::peek_tag_index;
using ::kotatsu::codec::detail::prepend_step;
using ::kotatsu::codec::detail::probes_as_untagged_variant;
using ::kotatsu::codec::detail::repr_decode;
using ::kotatsu::codec::detail::repr_encode;
using ::kotatsu::codec::detail::required_mask;
using ::kotatsu::codec::detail::reserve_for;
using ::kotatsu::codec::detail::run_decode;
using ::kotatsu::codec::detail::run_encode;
using ::kotatsu::codec::detail::skipped;
using ::kotatsu::codec::detail::slot_required;
using ::kotatsu::codec::detail::taggable;
using ::kotatsu::codec::detail::trace_path;
using ::kotatsu::codec::detail::try_read_callable;
using ::kotatsu::codec::detail::untagged_variant_pass;
using ::kotatsu::codec::detail::visit_variant_callable;
using ::kotatsu::codec::detail::with_index;
}

export namespace kotatsu::codec::dyn {
using ::kotatsu::codec::dyn::Array;
using ::kotatsu::codec::dyn::Cursor;
using ::kotatsu::codec::dyn::MapWriter;
using ::kotatsu::codec::dyn::Object;
using ::kotatsu::codec::dyn::SeqWriter;
using ::kotatsu::codec::dyn::StructWriter;
using ::kotatsu::codec::dyn::Value;
using ::kotatsu::codec::dyn::ValueKind;
using ::kotatsu::codec::dyn::ValueReader;
using ::kotatsu::codec::dyn::ValueWriter;
using ::kotatsu::codec::dyn::from_dyn;
using ::kotatsu::codec::dyn::to_dyn;
}

export namespace kotatsu::codec::dyn::detail {
using ::kotatsu::codec::dyn::detail::fail_type;
using ::kotatsu::codec::dyn::detail::kind_name;
}

export namespace kotatsu::codec::json {
using ::kotatsu::codec::json::KeySink;
using ::kotatsu::codec::json::MapWriter;
using ::kotatsu::codec::json::Reader;
using ::kotatsu::codec::json::SeqWriter;
using ::kotatsu::codec::json::Source;
using ::kotatsu::codec::json::StringBuilder;
using ::kotatsu::codec::json::StructWriter;
using ::kotatsu::codec::json::ValueWriter;
using ::kotatsu::codec::json::enum_rename_fn;
using ::kotatsu::codec::json::format;
using ::kotatsu::codec::json::from_padded_string;
using ::kotatsu::codec::json::from_string;
using ::kotatsu::codec::json::padded_string;
using ::kotatsu::codec::json::padded_string_view;
using ::kotatsu::codec::json::prettify;
using ::kotatsu::codec::json::schema;
using ::kotatsu::codec::json::schema_options;
using ::kotatsu::codec::json::schema_string;
using ::kotatsu::codec::json::success;
using ::kotatsu::codec::json::to_string;
}

export namespace kotatsu::codec::json::detail {
using ::kotatsu::codec::json::detail::DefaultAnnotator;
using ::kotatsu::codec::json::detail::FreshDefaults;
using ::kotatsu::codec::json::detail::ParsedDocument;
using ::kotatsu::codec::json::detail::SchemaEmitter;
using ::kotatsu::codec::json::detail::alternative_marker;
using ::kotatsu::codec::json::detail::collect_fresh;
using ::kotatsu::codec::json::detail::collect_fresh_alternatives;
using ::kotatsu::codec::json::detail::collect_fresh_slots;
using ::kotatsu::codec::json::detail::copy_bytes;
using ::kotatsu::codec::json::detail::count_lines;
using ::kotatsu::codec::json::detail::decoder_reads_directly;
using ::kotatsu::codec::json::detail::default_document_config;
using ::kotatsu::codec::json::detail::doc_iter_ptr;
using ::kotatsu::codec::json::detail::doc_iter_tag;
using ::kotatsu::codec::json::detail::escape_to;
using ::kotatsu::codec::json::detail::escaped_bytes;
using ::kotatsu::codec::json::detail::escaped_size;
using ::kotatsu::codec::json::detail::is_control;
using ::kotatsu::codec::json::detail::member_key;
using ::kotatsu::codec::json::detail::needs_escape;
using ::kotatsu::codec::json::detail::options_of;
using ::kotatsu::codec::json::detail::schema_config;
using ::kotatsu::codec::json::detail::short_escape;
using ::kotatsu::codec::json::detail::simdjson_error;
using ::kotatsu::codec::json::detail::steal;
using ::kotatsu::codec::json::detail::stringify;
using ::kotatsu::codec::json::detail::val_iter_ptr;
using ::kotatsu::codec::json::detail::val_iter_tag;
}

export namespace kotatsu::codec::json::ondemand {
using ::kotatsu::codec::json::ondemand::Document;
using ::kotatsu::codec::json::ondemand::NumberType;
using ::kotatsu::codec::json::ondemand::Parser;
using ::kotatsu::codec::json::ondemand::Type;
using ::kotatsu::codec::json::ondemand::Value;
}

export namespace kotatsu::comptime {
using ::kotatsu::comptime::ComptimeMemoryResource;
using ::kotatsu::comptime::ComptimeVector;
using ::kotatsu::comptime::Record;
using ::kotatsu::comptime::counting_flag;
}

export namespace kotatsu::deco {
namespace backend = ::kotatsu::option;
namespace refl = ::kotatsu::meta;
using ::kotatsu::deco::ParsedArgOwning;
}

export namespace kotatsu::deco::cli {
using ::kotatsu::deco::cli::AfterStep;
using ::kotatsu::deco::cli::Command;
using ::kotatsu::deco::cli::Invocation;
using ::kotatsu::deco::cli::ParseError;
using ::kotatsu::deco::cli::ParsedResult;
using ::kotatsu::deco::cli::SubCommandError;
using ::kotatsu::deco::cli::SubCommandMatch;
using ::kotatsu::deco::cli::SubCommander;
using ::kotatsu::deco::cli::check_valid;
using ::kotatsu::deco::cli::command;
using ::kotatsu::deco::cli::invoke;
using ::kotatsu::deco::cli::parse;
using ::kotatsu::deco::cli::parse_error_exit_code;
using ::kotatsu::deco::cli::parse_with_callback;
using ::kotatsu::deco::cli::runtime_callable_t;
using ::kotatsu::deco::cli::write_usage_for;
}

export namespace kotatsu::deco::cli::detail {
using ::kotatsu::deco::cli::detail::access_member_path;
using ::kotatsu::deco::cli::detail::exit_code_of;
using ::kotatsu::deco::cli::detail::help_requested;
using ::kotatsu::deco::cli::detail::input_args_text;
using ::kotatsu::deco::cli::detail::make_usage_document;
using ::kotatsu::deco::cli::detail::member_object_pointer_traits;
using ::kotatsu::deco::cli::detail::member_path_result;
using ::kotatsu::deco::cli::detail::member_path_result_t;
using ::kotatsu::deco::cli::detail::member_path_step;
using ::kotatsu::deco::cli::detail::resolve_alias_forward;
using ::kotatsu::deco::cli::detail::resolve_static_alias_forward;
using ::kotatsu::deco::cli::detail::run_parse_session;
}

export namespace kotatsu::deco::cli::text {
using ::kotatsu::deco::cli::text::CompatibleRenderer;
using ::kotatsu::deco::cli::text::CompatibleRendererConfig;
using ::kotatsu::deco::cli::text::Diagnostic;
using ::kotatsu::deco::cli::text::ModernRenderer;
using ::kotatsu::deco::cli::text::ModernRendererConfig;
using ::kotatsu::deco::cli::text::PositionStyle;
using ::kotatsu::deco::cli::text::Renderer;
using ::kotatsu::deco::cli::text::SubCommandDocument;
using ::kotatsu::deco::cli::text::SubCommandEntry;
using ::kotatsu::deco::cli::text::SubCommandStyle;
using ::kotatsu::deco::cli::text::TextStyle;
using ::kotatsu::deco::cli::text::UsageDocument;
using ::kotatsu::deco::cli::text::UsageEntry;
using ::kotatsu::deco::cli::text::UsageGroup;
using ::kotatsu::deco::cli::text::UsageStyle;
using ::kotatsu::deco::cli::text::clear_default_renderer;
using ::kotatsu::deco::cli::text::default_renderer;
using ::kotatsu::deco::cli::text::diagnostic_at;
using ::kotatsu::deco::cli::text::diagnostic_message;
using ::kotatsu::deco::cli::text::explicit_default_renderer;
using ::kotatsu::deco::cli::text::looks_like_rendered_diagnostic;
using ::kotatsu::deco::cli::text::render_diagnostic;
using ::kotatsu::deco::cli::text::render_subcommands;
using ::kotatsu::deco::cli::text::render_usage;
using ::kotatsu::deco::cli::text::render_usage_entry;
using ::kotatsu::deco::cli::text::resolve_renderer;
using ::kotatsu::deco::cli::text::set_default_renderer;
}

export namespace kotatsu::deco::config {
using ::kotatsu::deco::config::BuiltInRenderConfig;
using ::kotatsu::deco::config::CompatibleRendererConfig;
using ::kotatsu::deco::config::Config;
using ::kotatsu::deco::config::ConfigOverride;
using ::kotatsu::deco::config::EnumMetaVarConfig;
using ::kotatsu::deco::config::ModernRendererConfig;
using ::kotatsu::deco::config::PositionStyle;
using ::kotatsu::deco::config::SubCommandStyle;
using ::kotatsu::deco::config::TextStyle;
using ::kotatsu::deco::config::UsageStyle;
using ::kotatsu::deco::config::get;
using ::kotatsu::deco::config::merge;
using ::kotatsu::deco::config::set;
using ::kotatsu::deco::config::set_enum_meta_var;
using ::kotatsu::deco::config::set_render;
}

export namespace kotatsu::deco::decl {
using ::kotatsu::deco::decl::AliasFields;
using ::kotatsu::deco::decl::AliasForwardField;
using ::kotatsu::deco::decl::AliasForwardFn;
using ::kotatsu::deco::decl::AliasForwardFnWithContext;
using ::kotatsu::deco::decl::AliasForwardResult;
using ::kotatsu::deco::decl::ArgNum;
using ::kotatsu::deco::decl::Category;
using ::kotatsu::deco::decl::CategoryRef;
using ::kotatsu::deco::decl::CommaJoinedAliasFields;
using ::kotatsu::deco::decl::CommaJoinedFields;
using ::kotatsu::deco::decl::CommonOptionFields;
using ::kotatsu::deco::decl::ConfigFields;
using ::kotatsu::deco::decl::ConfigOverrideField;
using ::kotatsu::deco::decl::DecoFields;
using ::kotatsu::deco::decl::DecoOption;
using ::kotatsu::deco::decl::DecoOptionBase;
using ::kotatsu::deco::decl::DecoType;
using ::kotatsu::deco::decl::ErasedParseCallback;
using ::kotatsu::deco::decl::FlagAliasFields;
using ::kotatsu::deco::decl::FlagFields;
using ::kotatsu::deco::decl::FlagOption;
using ::kotatsu::deco::decl::HelpOption;
using ::kotatsu::deco::decl::InputFields;
using ::kotatsu::deco::decl::InputOption;
using ::kotatsu::deco::decl::IntoContext;
using ::kotatsu::deco::decl::KVAliasFields;
using ::kotatsu::deco::decl::KVFields;
using ::kotatsu::deco::decl::KVOption;
using ::kotatsu::deco::decl::KVStyle;
using ::kotatsu::deco::decl::MetaVarField;
using ::kotatsu::deco::decl::MultiAliasFields;
using ::kotatsu::deco::decl::MultiFields;
using ::kotatsu::deco::decl::NamedOptionFields;
using ::kotatsu::deco::decl::OptionCallbackField;
using ::kotatsu::deco::decl::OptionDeclaration;
using ::kotatsu::deco::decl::PackFields;
using ::kotatsu::deco::decl::ParseControl;
using ::kotatsu::deco::decl::ParseStep;
using ::kotatsu::deco::decl::ScalarOption;
using ::kotatsu::deco::decl::SubCommand;
using ::kotatsu::deco::decl::VectorOption;
using ::kotatsu::deco::decl::default_category;
using ::kotatsu::deco::decl::help_names;
using ::kotatsu::deco::decl::input_arg_num;
using ::kotatsu::deco::decl::invalid_input_arg_num;
using ::kotatsu::deco::decl::is_alias_placeholder_name;
using enum ::kotatsu::deco::decl::KVStyle;
}

export namespace kotatsu::deco::decl::detail {
using ::kotatsu::deco::decl::detail::append_vector;
using ::kotatsu::deco::decl::detail::assign_scalar;
using ::kotatsu::deco::decl::detail::assign_vector;
using ::kotatsu::deco::decl::detail::format_invalid_enum_value;
using ::kotatsu::deco::decl::detail::generated_option_name;
using ::kotatsu::deco::decl::detail::has_kv_style;
using ::kotatsu::deco::decl::detail::iequals_ascii;
using ::kotatsu::deco::decl::detail::is_joined_kv_name;
using ::kotatsu::deco::decl::detail::parse_primitive_scalar;
using ::kotatsu::deco::decl::detail::takes_value_after_equals;
}

export namespace kotatsu::deco::desc {
using ::kotatsu::deco::desc::from_deco_option;
}

export namespace kotatsu::deco::desc::detail {
using ::kotatsu::deco::desc::detail::active_config;
using ::kotatsu::deco::desc::detail::base_meta_name;
using ::kotatsu::deco::desc::detail::category_desc;
using ::kotatsu::deco::desc::detail::comma_joined_alias;
using ::kotatsu::deco::desc::detail::enum_meta_var_token;
using ::kotatsu::deco::desc::detail::has_help_text;
using ::kotatsu::deco::desc::detail::inferred_enum_meta_var_type;
using ::kotatsu::deco::desc::detail::inferred_enum_meta_var_type_t;
using ::kotatsu::deco::desc::detail::inferred_meta_var_token;
using ::kotatsu::deco::desc::detail::is_repeatable;
using ::kotatsu::deco::desc::detail::join_aliases;
using ::kotatsu::deco::desc::detail::join_strings;
using ::kotatsu::deco::desc::detail::kv_usage_text;
using ::kotatsu::deco::desc::detail::meta_var_token;
using ::kotatsu::deco::desc::detail::named_aliases;
using ::kotatsu::deco::desc::detail::placeholder_name;
using ::kotatsu::deco::desc::detail::repeated_meta_vars;
using ::kotatsu::deco::desc::detail::usage_entry;
using ::kotatsu::deco::desc::detail::usage_text;
using ::kotatsu::deco::desc::detail::usage_text_for_field;
}

export namespace kotatsu::deco::detail {
using ::kotatsu::deco::detail::DecoStructConsumer;
using ::kotatsu::deco::detail::LLVMOptGenerator;
using ::kotatsu::deco::detail::SizedGenerator;
using ::kotatsu::deco::detail::StrPool;
using ::kotatsu::deco::detail::build_record;
using ::kotatsu::deco::detail::generator_of;
using ::kotatsu::deco::detail::prefixes_of;
using ::kotatsu::deco::detail::same_option_name;
}

export namespace kotatsu::deco::ser {
using ::kotatsu::deco::ser::Serializer;
using ::kotatsu::deco::ser::to_argv;
}

export namespace kotatsu::deco::trait {
using ::kotatsu::deco::trait::BaseResultTy;
using ::kotatsu::deco::trait::BorrowedStringResultType;
using ::kotatsu::deco::trait::CustomStringResultTy;
using ::kotatsu::deco::trait::CustomStringResultTyWithContext;
using ::kotatsu::deco::trait::CustomStringVectorResultTy;
using ::kotatsu::deco::trait::CustomStringVectorResultTyWithContext;
using ::kotatsu::deco::trait::FlagResultType;
using ::kotatsu::deco::trait::OptionalResultType;
using ::kotatsu::deco::trait::OptionalTrait;
using ::kotatsu::deco::trait::OwnedStringResultType;
using ::kotatsu::deco::trait::PrimitiveScalarResultType;
using ::kotatsu::deco::trait::PrimitiveVectorResultType;
using ::kotatsu::deco::trait::ScalarResultType;
using ::kotatsu::deco::trait::StringResultType;
using ::kotatsu::deco::trait::ValueResultType;
using ::kotatsu::deco::trait::VectorResultType;
}

export namespace kotatsu::deco::ty {
using ::kotatsu::deco::ty::alias_ty_of;
using ::kotatsu::deco::ty::base_ty;
using ::kotatsu::deco::ty::cfg_ty_of;
using ::kotatsu::deco::ty::deco_option_like;
using ::kotatsu::deco::ty::dyn_cast;
using ::kotatsu::deco::ty::field_ty_of;
using ::kotatsu::deco::ty::is_alias_field;
using ::kotatsu::deco::ty::is_config_field;
using ::kotatsu::deco::ty::is_deco_field;
using ::kotatsu::deco::ty::is_deco_field_or_option;
using ::kotatsu::deco::ty::is_decoed;
using ::kotatsu::deco::ty::is_option_field;
}

export namespace kotatsu::deco::util {
using ::kotatsu::deco::util::argvify;
}

export namespace kotatsu::detail {
using ::kotatsu::detail::ErasedCallable;
using ::kotatsu::detail::append_sequence_element;
using ::kotatsu::detail::decode_sequence;
using ::kotatsu::detail::default_buffer_size;
using ::kotatsu::detail::from_utf16;
using ::kotatsu::detail::inline_buffer;
using ::kotatsu::detail::insert_map_entry;
using ::kotatsu::detail::is_small_vector_v;
using ::kotatsu::detail::map_entry_like;
using ::kotatsu::detail::map_entry_pair_like;
using ::kotatsu::detail::map_entry_tuple_like;
using ::kotatsu::detail::map_entry_types;
using ::kotatsu::detail::map_insertable;
using ::kotatsu::detail::pack_any_v;
using ::kotatsu::detail::pack_count_v;
using ::kotatsu::detail::pack_find;
using ::kotatsu::detail::pack_find_spec;
using ::kotatsu::detail::sequence_insertable;
using ::kotatsu::detail::small_vector_compatible_range;
using ::kotatsu::detail::small_vector_size_type;
using ::kotatsu::detail::synth_three_way;
using ::kotatsu::detail::to_utf16;
}

export namespace kotatsu::fs {
using ::kotatsu::fs::Directory;
using ::kotatsu::fs::File;
using ::kotatsu::fs::FileID;
using ::kotatsu::fs::FileTimes;
using ::kotatsu::fs::FileType;
using ::kotatsu::fs::OpenOptions;
using ::kotatsu::fs::SeekFrom;
using ::kotatsu::fs::Status;
using ::kotatsu::fs::TempDirectory;
using ::kotatsu::fs::TempFile;
using ::kotatsu::fs::Volume;
using ::kotatsu::fs::Walk;
using ::kotatsu::fs::absolute;
using ::kotatsu::fs::canonical;
using ::kotatsu::fs::copy_file;
using ::kotatsu::fs::create_directories;
using ::kotatsu::fs::create_directory;
using ::kotatsu::fs::create_directory_symlink;
using ::kotatsu::fs::create_hard_link;
using ::kotatsu::fs::create_symlink;
using ::kotatsu::fs::create_temp_directory;
using ::kotatsu::fs::create_temp_file;
using ::kotatsu::fs::current_dir;
using ::kotatsu::fs::equivalent;
using ::kotatsu::fs::exists;
using ::kotatsu::fs::follow_symlinks;
using ::kotatsu::fs::is_directory;
using ::kotatsu::fs::is_file;
using ::kotatsu::fs::is_symlink;
using ::kotatsu::fs::read_file;
using ::kotatsu::fs::read_symlink;
using ::kotatsu::fs::remove;
using ::kotatsu::fs::remove_all;
using ::kotatsu::fs::rename;
using ::kotatsu::fs::rename_noreplace;
using ::kotatsu::fs::set_permissions;
using ::kotatsu::fs::set_times;
using ::kotatsu::fs::status;
using ::kotatsu::fs::symlink_status;
using ::kotatsu::fs::try_exists;
using ::kotatsu::fs::volume;
using ::kotatsu::fs::walk;
using ::kotatsu::fs::weakly_canonical;
using ::kotatsu::fs::write_file;
using ::kotatsu::fs::write_file_atomic;
}

export namespace kotatsu::fs::detail {
using ::kotatsu::fs::detail::OpenDirectory;
}

export namespace kotatsu::fs::path {
using ::kotatsu::fs::path::DotDot;
using ::kotatsu::fs::path::Prefix;
using ::kotatsu::fs::path::Root;
using ::kotatsu::fs::path::Style;
using ::kotatsu::fs::path::ancestors;
using ::kotatsu::fs::path::ancestors_view;
using ::kotatsu::fs::path::components;
using ::kotatsu::fs::path::components_view;
using ::kotatsu::fs::path::extension;
using ::kotatsu::fs::path::filename;
using ::kotatsu::fs::path::has_root;
using ::kotatsu::fs::path::is_absolute;
using ::kotatsu::fs::path::is_separator;
using ::kotatsu::fs::path::is_within;
using ::kotatsu::fs::path::lowercase_drive;
using ::kotatsu::fs::path::native_style;
using ::kotatsu::fs::path::normalize;
using ::kotatsu::fs::path::parent;
using ::kotatsu::fs::path::pop;
using ::kotatsu::fs::path::push;
using ::kotatsu::fs::path::relative;
using ::kotatsu::fs::path::resolve;
using ::kotatsu::fs::path::root;
using ::kotatsu::fs::path::set_extension;
using ::kotatsu::fs::path::set_file_name;
using ::kotatsu::fs::path::stem;
using ::kotatsu::fs::path::strip_prefix;
using ::kotatsu::fs::path::to_backslash;
using ::kotatsu::fs::path::to_slash;
}

export namespace kotatsu::fs::path::detail {
using ::kotatsu::fs::path::detail::joins;
}

export namespace kotatsu::ipc {
using ::kotatsu::ipc::BincodeCodec;
using ::kotatsu::ipc::BincodePeer;
using ::kotatsu::ipc::ConversationEnd;
using ::kotatsu::ipc::Error;
using ::kotatsu::ipc::FrameParser;
using ::kotatsu::ipc::IncomingErrorResponse;
using ::kotatsu::ipc::IncomingMessage;
using ::kotatsu::ipc::IncomingNotification;
using ::kotatsu::ipc::IncomingParseError;
using ::kotatsu::ipc::IncomingRequest;
using ::kotatsu::ipc::IncomingResponse;
using ::kotatsu::ipc::JSONCodec;
using ::kotatsu::ipc::JSONPeer;
using ::kotatsu::ipc::LogCallback;
using ::kotatsu::ipc::LogLevel;
using ::kotatsu::ipc::MessageHead;
using ::kotatsu::ipc::PayloadSlice;
using ::kotatsu::ipc::Peer;
using ::kotatsu::ipc::ReadError;
using ::kotatsu::ipc::RequestResult;
using ::kotatsu::ipc::Result;
using ::kotatsu::ipc::StreamTransport;
using ::kotatsu::ipc::Transport;
using ::kotatsu::ipc::UnansweredRequest;
using ::kotatsu::ipc::ViolationPolicy;
using ::kotatsu::ipc::basic_request_context;
using ::kotatsu::ipc::frame_header;
using ::kotatsu::ipc::lsp_config;
using ::kotatsu::ipc::request_options;
}

export namespace kotatsu::ipc::detail {
using ::kotatsu::ipc::detail::LoggedID;
using ::kotatsu::ipc::detail::callback_args_t;
using ::kotatsu::ipc::detail::callback_param_t;
using ::kotatsu::ipc::detail::callback_return_t;
using ::kotatsu::ipc::detail::has_notification_traits_v;
using ::kotatsu::ipc::detail::has_request_traits_v;
using ::kotatsu::ipc::detail::message_prefix;
using ::kotatsu::ipc::detail::notification_prefix;
using ::kotatsu::ipc::detail::request_prefix;
using ::kotatsu::ipc::detail::success_prefix;
using ::kotatsu::ipc::detail::takes_no_params_v;
using ::kotatsu::ipc::detail::validate_notification_callback_signature;
using ::kotatsu::ipc::detail::validate_request_callback_signature;
}

export namespace kotatsu::ipc::protocol {
using ::kotatsu::ipc::protocol::CancelRequestParams;
using ::kotatsu::ipc::protocol::Error;
using ::kotatsu::ipc::protocol::ErrorCode;
using ::kotatsu::ipc::protocol::NotificationTraits;
using ::kotatsu::ipc::protocol::RequestID;
using ::kotatsu::ipc::protocol::RequestTraits;
using ::kotatsu::ipc::protocol::boolean;
using ::kotatsu::ipc::protocol::decimal;
using ::kotatsu::ipc::protocol::integer;
using ::kotatsu::ipc::protocol::null;
using ::kotatsu::ipc::protocol::string;
using ::kotatsu::ipc::protocol::uinteger;
}

export namespace kotatsu::mem {
using ::kotatsu::mem::AllocationGuard;
using ::kotatsu::mem::ConstructedRange;
using ::kotatsu::mem::allocate;
using ::kotatsu::mem::construct_each;
using ::kotatsu::mem::deallocate;
using ::kotatsu::mem::default_construct;
using ::kotatsu::mem::is_bitwise_constructible_v;
using ::kotatsu::mem::move_range;
using ::kotatsu::mem::pointer_in_range;
using ::kotatsu::mem::range_length;
using ::kotatsu::mem::uninitialized_copy;
using ::kotatsu::mem::uninitialized_default_construct;
using ::kotatsu::mem::uninitialized_fill;
using ::kotatsu::mem::uninitialized_move;
using ::kotatsu::mem::uninitialized_value_construct;
}

export namespace kotatsu::mem::detail {
using ::kotatsu::mem::detail::construct_from_each;
using ::kotatsu::mem::detail::copy_bytes;
}

export namespace kotatsu::meta {
using ::kotatsu::meta::annotate;
using ::kotatsu::meta::annotated_type;
using ::kotatsu::meta::annotated_underlying_t;
using ::kotatsu::meta::annotated_value;
using ::kotatsu::meta::annotation;
using ::kotatsu::meta::apply_rename_policy;
using ::kotatsu::meta::array_type_info;
using ::kotatsu::meta::bool_like;
using ::kotatsu::meta::bytes_like;
using ::kotatsu::meta::char_like;
using ::kotatsu::meta::compare_numbers;
using ::kotatsu::meta::declared_repr_t;
using ::kotatsu::meta::default_config;
using ::kotatsu::meta::dynamic;
using ::kotatsu::meta::empty_field_spec;
using ::kotatsu::meta::empty_struct_spec;
using ::kotatsu::meta::enum_name;
using ::kotatsu::meta::enum_strings;
using ::kotatsu::meta::enum_type;
using ::kotatsu::meta::enum_type_info;
using ::kotatsu::meta::enum_value;
using ::kotatsu::meta::eq;
using ::kotatsu::meta::eq_t;
using ::kotatsu::meta::evaluate_skip_predicate;
using ::kotatsu::meta::evaluate_skip_when;
using ::kotatsu::meta::field;
using ::kotatsu::meta::field_addr_of;
using ::kotatsu::meta::field_count;
using ::kotatsu::meta::field_info;
using ::kotatsu::meta::field_name;
using ::kotatsu::meta::field_names;
using ::kotatsu::meta::field_of;
using ::kotatsu::meta::field_offset;
using ::kotatsu::meta::field_refs;
using ::kotatsu::meta::field_slot;
using ::kotatsu::meta::field_spec;
using ::kotatsu::meta::field_spec_of;
using ::kotatsu::meta::field_type;
using ::kotatsu::meta::floating_like;
using ::kotatsu::meta::for_each;
using ::kotatsu::meta::format_config;
using ::kotatsu::meta::format_of_t;
using ::kotatsu::meta::ge;
using ::kotatsu::meta::ge_t;
using ::kotatsu::meta::gt;
using ::kotatsu::meta::gt_t;
using ::kotatsu::meta::has_repr;
using ::kotatsu::meta::inherit_type;
using ::kotatsu::meta::inherit_use_type;
using ::kotatsu::meta::int_like;
using ::kotatsu::meta::is_behavior_attr_v;
using ::kotatsu::meta::is_behavior_provider;
using ::kotatsu::meta::is_floating_kind;
using ::kotatsu::meta::is_integer_kind;
using ::kotatsu::meta::is_object_kind;
using ::kotatsu::meta::is_pair_v;
using ::kotatsu::meta::is_sequence_kind;
using ::kotatsu::meta::is_signed_integer_kind;
using ::kotatsu::meta::is_spec_attr;
using ::kotatsu::meta::is_struct_spec_attr;
using ::kotatsu::meta::is_tuple_v;
using ::kotatsu::meta::is_unsigned_integer_kind;
using ::kotatsu::meta::kind_of;
using ::kotatsu::meta::le;
using ::kotatsu::meta::le_t;
using ::kotatsu::meta::lt;
using ::kotatsu::meta::lt_t;
using ::kotatsu::meta::make_annotation;
using ::kotatsu::meta::make_spec;
using ::kotatsu::meta::make_struct_spec;
using ::kotatsu::meta::map_enum_to_string;
using ::kotatsu::meta::map_string_to_enum;
using ::kotatsu::meta::map_type_info;
using ::kotatsu::meta::member_name;
using ::kotatsu::meta::merged_config_t;
using ::kotatsu::meta::name_list;
using ::kotatsu::meta::ne;
using ::kotatsu::meta::ne_t;
using ::kotatsu::meta::node_config_t;
using ::kotatsu::meta::null_like;
using ::kotatsu::meta::operator==;
using ::kotatsu::meta::optional_type_info;
using ::kotatsu::meta::pointer_name;
using ::kotatsu::meta::reflectable_class;
using ::kotatsu::meta::reflection;
using ::kotatsu::meta::repr;
using ::kotatsu::meta::repr_for;
using ::kotatsu::meta::resolve_tag_names;
using ::kotatsu::meta::resolved_config_t;
using ::kotatsu::meta::resolved_repr_t;
using ::kotatsu::meta::resolves_to_tagged_variant;
using ::kotatsu::meta::schema_number;
using ::kotatsu::meta::schema_opaque;
using ::kotatsu::meta::skip_when;
using ::kotatsu::meta::spec_of;
using ::kotatsu::meta::spec_result;
using ::kotatsu::meta::str_like;
using ::kotatsu::meta::struct_spec;
using ::kotatsu::meta::struct_spec_of;
using ::kotatsu::meta::struct_spec_tag;
using ::kotatsu::meta::struct_type_info;
using ::kotatsu::meta::tag_mode;
using ::kotatsu::meta::tuple_like;
using ::kotatsu::meta::tuple_type_info;
using ::kotatsu::meta::type_info;
using ::kotatsu::meta::type_info_fn;
using ::kotatsu::meta::type_info_of;
using ::kotatsu::meta::type_kind;
using ::kotatsu::meta::type_name;
using ::kotatsu::meta::type_tag;
using ::kotatsu::meta::uint_like;
using ::kotatsu::meta::variant_type_info;
using ::kotatsu::meta::virtual_schema;
using ::kotatsu::meta::wrap_type;
}

export namespace kotatsu::meta::attrs {
using ::kotatsu::meta::attrs::spec;
using ::kotatsu::meta::attrs::struct_spec;
}

export namespace kotatsu::meta::behavior {
using ::kotatsu::meta::behavior::as;
using ::kotatsu::meta::behavior::enum_string;
using ::kotatsu::meta::behavior::skip_if;
using ::kotatsu::meta::behavior::with;
}

export namespace kotatsu::meta::detail {
using ::kotatsu::meta::detail::annotated_field;
using ::kotatsu::meta::detail::any;
using ::kotatsu::meta::detail::any_component;
using ::kotatsu::meta::detail::apply_rename_cx;
using ::kotatsu::meta::detail::as_variant;
using ::kotatsu::meta::detail::build_fields;
using ::kotatsu::meta::detail::build_slots_from_seq;
using ::kotatsu::meta::detail::build_slots_t;
using ::kotatsu::meta::detail::built_fields_t;
using ::kotatsu::meta::detail::compare_eq;
using ::kotatsu::meta::detail::compare_ge;
using ::kotatsu::meta::detail::compare_gt;
using ::kotatsu::meta::detail::compare_le;
using ::kotatsu::meta::detail::compare_lt;
using ::kotatsu::meta::detail::compare_map_entry_eq;
using ::kotatsu::meta::detail::compare_map_entry_lt;
using ::kotatsu::meta::detail::compare_map_eq;
using ::kotatsu::meta::detail::compare_map_eq_unordered;
using ::kotatsu::meta::detail::compare_map_lt;
using ::kotatsu::meta::detail::compare_ne;
using ::kotatsu::meta::detail::compare_range_eq;
using ::kotatsu::meta::detail::compare_range_lt;
using ::kotatsu::meta::detail::compare_sequence_eq;
using ::kotatsu::meta::detail::compare_sequence_lt;
using ::kotatsu::meta::detail::compare_set_eq_unordered;
using ::kotatsu::meta::detail::component_attr;
using ::kotatsu::meta::detail::component_kinds_unique;
using ::kotatsu::meta::detail::config_defaults_fields;
using ::kotatsu::meta::detail::config_denies_unknown;
using ::kotatsu::meta::detail::count_valid;
using ::kotatsu::meta::detail::declared_repr_impl;
using ::kotatsu::meta::detail::defaulted_layer;
using ::kotatsu::meta::detail::deny_layer;
using ::kotatsu::meta::detail::effective_field_count;
using ::kotatsu::meta::detail::effective_field_rename;
using ::kotatsu::meta::detail::effective_field_rename_t;
using ::kotatsu::meta::detail::enum_info_node;
using ::kotatsu::meta::detail::enum_probe;
using ::kotatsu::meta::detail::enum_probe_name;
using ::kotatsu::meta::detail::enum_probe_value;
using ::kotatsu::meta::detail::eq_op;
using ::kotatsu::meta::detail::ext;
using ::kotatsu::meta::detail::extract_identifier;
using ::kotatsu::meta::detail::field_attr_flags;
using ::kotatsu::meta::detail::field_count;
using ::kotatsu::meta::detail::fill_field;
using ::kotatsu::meta::detail::filter_runtime_attrs;
using ::kotatsu::meta::detail::filter_runtime_attrs_t;
using ::kotatsu::meta::detail::finalize_struct_spec;
using ::kotatsu::meta::detail::find_last_top_level_scope;
using ::kotatsu::meta::detail::findable_in;
using ::kotatsu::meta::detail::format_of_impl;
using ::kotatsu::meta::detail::ge_op;
using ::kotatsu::meta::detail::gt_op;
using ::kotatsu::meta::detail::integer_like;
using ::kotatsu::meta::detail::integer_value;
using ::kotatsu::meta::detail::is_runtime_spec_attr_v;
using ::kotatsu::meta::detail::is_runtime_struct_spec_attr_v;
using ::kotatsu::meta::detail::le_op;
using ::kotatsu::meta::detail::lt_op;
using ::kotatsu::meta::detail::make_candidate_names;
using ::kotatsu::meta::detail::make_candidate_valid;
using ::kotatsu::meta::detail::make_candidate_values;
using ::kotatsu::meta::detail::make_field_info;
using ::kotatsu::meta::detail::make_scan_values;
using ::kotatsu::meta::detail::map_entries_contained_in;
using ::kotatsu::meta::detail::merge_config_impl;
using ::kotatsu::meta::detail::merged_config;
using ::kotatsu::meta::detail::op_comparable_with;
using ::kotatsu::meta::detail::range_ref_t;
using ::kotatsu::meta::detail::range_value_op_comparable;
using ::kotatsu::meta::detail::reflectable_pair;
using ::kotatsu::meta::detail::rename_layer;
using ::kotatsu::meta::detail::rename_with;
using ::kotatsu::meta::detail::renamed_name_static;
using ::kotatsu::meta::detail::repr_for_impl;
using ::kotatsu::meta::detail::resolve_field_name;
using ::kotatsu::meta::detail::resolve_node;
using ::kotatsu::meta::detail::resolve_repr;
using ::kotatsu::meta::detail::resolved_repr;
using ::kotatsu::meta::detail::scan_values;
using ::kotatsu::meta::detail::set_entries_contained_in;
using ::kotatsu::meta::detail::single_field_count;
using ::kotatsu::meta::detail::single_field_slots;
using ::kotatsu::meta::detail::standard_integer;
using ::kotatsu::meta::detail::takeover_for_range;
using ::kotatsu::meta::detail::takeover_range_eq;
using ::kotatsu::meta::detail::takeover_range_ge;
using ::kotatsu::meta::detail::takeover_range_gt;
using ::kotatsu::meta::detail::takeover_range_le;
using ::kotatsu::meta::detail::takeover_range_lt;
using ::kotatsu::meta::detail::test;
using ::kotatsu::meta::detail::tuple_gettable;
using ::kotatsu::meta::detail::tuple_gettable_impl;
using ::kotatsu::meta::detail::tuple_info_node;
using ::kotatsu::meta::detail::type_instance;
using ::kotatsu::meta::detail::type_instance_impl;
using ::kotatsu::meta::detail::uninitialized;
using ::kotatsu::meta::detail::unqualify_type_name;
using ::kotatsu::meta::detail::unwrap_annotated;
using ::kotatsu::meta::detail::unwrap_wrapper_value;
using ::kotatsu::meta::detail::validate_attrs;
using ::kotatsu::meta::detail::validate_spec;
using ::kotatsu::meta::detail::variant_info_node;
using ::kotatsu::meta::detail::variant_like;
using ::kotatsu::meta::detail::variant_of;
using ::kotatsu::meta::detail::variant_of_t;
using ::kotatsu::meta::detail::variant_pair;
using ::kotatsu::meta::detail::wrapper;
}

export namespace kotatsu::meta::dsl {
using ::kotatsu::meta::dsl::alias;
using ::kotatsu::meta::dsl::as;
using ::kotatsu::meta::dsl::aspect;
using ::kotatsu::meta::dsl::choices;
using ::kotatsu::meta::dsl::content;
using ::kotatsu::meta::dsl::defaulted;
using ::kotatsu::meta::dsl::defaulted_fields;
using ::kotatsu::meta::dsl::deny_unknown_fields;
using ::kotatsu::meta::dsl::description;
using ::kotatsu::meta::dsl::enum_string;
using ::kotatsu::meta::dsl::flatten;
using ::kotatsu::meta::dsl::idx;
using ::kotatsu::meta::dsl::is_type_component_v;
using ::kotatsu::meta::dsl::maximum;
using ::kotatsu::meta::dsl::minimum;
using ::kotatsu::meta::dsl::name_list_proxy;
using ::kotatsu::meta::dsl::number_proxy;
using ::kotatsu::meta::dsl::rename;
using ::kotatsu::meta::dsl::rename_all;
using ::kotatsu::meta::dsl::schema_default;
using ::kotatsu::meta::dsl::skip;
using ::kotatsu::meta::dsl::skip_if;
using ::kotatsu::meta::dsl::skip_if_proxy;
using ::kotatsu::meta::dsl::spec_component;
using ::kotatsu::meta::dsl::tag;
using ::kotatsu::meta::dsl::tag_names;
using ::kotatsu::meta::dsl::tagged;
using ::kotatsu::meta::dsl::tagged_component;
using ::kotatsu::meta::dsl::tagged_proxy;
using ::kotatsu::meta::dsl::type;
using ::kotatsu::meta::dsl::type_component;
using ::kotatsu::meta::dsl::type_proxy;
using ::kotatsu::meta::dsl::value_component;
using ::kotatsu::meta::dsl::value_proxy;
using ::kotatsu::meta::dsl::with;
}

export namespace kotatsu::meta::pred {
using ::kotatsu::meta::pred::default_value;
using ::kotatsu::meta::pred::empty;
using ::kotatsu::meta::pred::optional_none;
}

export namespace kotatsu::naming {
using ::kotatsu::naming::Casing;
using ::kotatsu::naming::is_alnum;
using ::kotatsu::naming::is_alpha;
using ::kotatsu::naming::is_digit;
using ::kotatsu::naming::is_lower;
using ::kotatsu::naming::is_non_ascii;
using ::kotatsu::naming::is_upper;
using ::kotatsu::naming::normalize_identifier;
using ::kotatsu::naming::normalize_to_lower_snake;
using ::kotatsu::naming::rename_policy_t;
using ::kotatsu::naming::snake_to_camel;
using ::kotatsu::naming::snake_to_upper;
using ::kotatsu::naming::to_lower;
using ::kotatsu::naming::to_upper;
}

export namespace kotatsu::naming::detail {
using ::kotatsu::naming::detail::casing_policy;
}

export namespace kotatsu::naming::rename_policy {
using ::kotatsu::naming::rename_policy::identity;
using ::kotatsu::naming::rename_policy::lower_camel;
using ::kotatsu::naming::rename_policy::lower_snake;
using ::kotatsu::naming::rename_policy::upper_camel;
using ::kotatsu::naming::rename_policy::upper_snake;
}

export namespace kotatsu::option {
using ::kotatsu::option::ArgsRef;
using ::kotatsu::option::DriverFlag;
using ::kotatsu::option::DriverVisibility;
using ::kotatsu::option::Kind;
using ::kotatsu::option::OptTable;
using ::kotatsu::option::Option;
using ::kotatsu::option::OptionRef;
using ::kotatsu::option::ParseError;
using ::kotatsu::option::ParseIter;
using ::kotatsu::option::ParseOptions;
using ::kotatsu::option::ParsedArg;
using ::kotatsu::option::RenderStyle;
using ::kotatsu::option::pfx_all;
using ::kotatsu::option::pfx_dash;
using ::kotatsu::option::pfx_dash_double;
using ::kotatsu::option::pfx_double;
using ::kotatsu::option::pfx_none;
using ::kotatsu::option::pfx_slash_dash;
using enum ::kotatsu::option::DriverFlag;
using enum ::kotatsu::option::DriverVisibility;
}

export namespace kotatsu::option::detail {
using ::kotatsu::option::detail::all_prefixes;
using ::kotatsu::option::detail::dash_double_prefixes;
using ::kotatsu::option::detail::dash_prefixes;
using ::kotatsu::option::detail::double_prefixes;
using ::kotatsu::option::detail::slash_dash_prefixes;
}

export namespace kotatsu::sys {
using ::kotatsu::sys::Child;
using ::kotatsu::sys::CreationOptions;
using ::kotatsu::sys::EnvChange;
using ::kotatsu::sys::EnvVar;
using ::kotatsu::sys::ExitStatus;
using ::kotatsu::sys::ProgramSearch;
using ::kotatsu::sys::SpawnOptions;
using ::kotatsu::sys::Stdio;
using ::kotatsu::sys::arguments;
using ::kotatsu::sys::cache_dir;
using ::kotatsu::sys::capture;
using ::kotatsu::sys::config_dir;
using ::kotatsu::sys::data_dir;
using ::kotatsu::sys::env_name_equal;
using ::kotatsu::sys::environment;
using ::kotatsu::sys::executable_path;
using ::kotatsu::sys::expand_tilde;
using ::kotatsu::sys::find_program;
using ::kotatsu::sys::get_env;
using ::kotatsu::sys::home_dir;
using ::kotatsu::sys::join_paths;
using ::kotatsu::sys::parent_pid;
using ::kotatsu::sys::pid;
using ::kotatsu::sys::process_alive;
using ::kotatsu::sys::self_executable;
using ::kotatsu::sys::set_env;
using ::kotatsu::sys::set_process_name;
using ::kotatsu::sys::set_process_title;
using ::kotatsu::sys::signal_name;
using ::kotatsu::sys::spawn;
using ::kotatsu::sys::split_paths;
using ::kotatsu::sys::split_paths_view;
using ::kotatsu::sys::started_path;
using ::kotatsu::sys::temp_dir;
using ::kotatsu::sys::unset_env;
}

export namespace kotatsu::zest {
using ::kotatsu::zest::CaseRegistrar;
using ::kotatsu::zest::Context;
using ::kotatsu::zest::FatalHook;
using ::kotatsu::zest::Match;
using ::kotatsu::zest::Options;
using ::kotatsu::zest::Runner;
using ::kotatsu::zest::TestAttrs;
using ::kotatsu::zest::TestCase;
using ::kotatsu::zest::TestState;
using ::kotatsu::zest::TestSuite;
using ::kotatsu::zest::TestSuiteDef;
using ::kotatsu::zest::contains;
using ::kotatsu::zest::current_test_state;
using ::kotatsu::zest::describe_exception;
using ::kotatsu::zest::ends_with;
using ::kotatsu::zest::failure;
using ::kotatsu::zest::merge_attrs;
using ::kotatsu::zest::pretty_dump;
using ::kotatsu::zest::print_trace;
using ::kotatsu::zest::run_cli;
using ::kotatsu::zest::run_tests;
using ::kotatsu::zest::skip;
using ::kotatsu::zest::snapshot;
using ::kotatsu::zest::snapshot_glob;
using ::kotatsu::zest::starts_with;
using ::kotatsu::zest::strip_test_prefix;
using ::kotatsu::zest::throws;
using ::kotatsu::zest::trace_exception;
using ::kotatsu::zest::type_eq;
}

export namespace kotatsu::zest::detail {
using ::kotatsu::zest::detail::Captured;
using ::kotatsu::zest::detail::Comparison;
using ::kotatsu::zest::detail::Decomposer;
using ::kotatsu::zest::detail::Operand;
using ::kotatsu::zest::detail::Relation;
using ::kotatsu::zest::detail::ReportLine;
using ::kotatsu::zest::detail::as_pattern;
using ::kotatsu::zest::detail::as_text;
using ::kotatsu::zest::detail::check;
using ::kotatsu::zest::detail::check_fatal;
using ::kotatsu::zest::detail::check_snapshot;
using ::kotatsu::zest::detail::cleanup_unused_snapshots;
using ::kotatsu::zest::detail::compares_as_text;
using ::kotatsu::zest::detail::fail_fatally;
using ::kotatsu::zest::detail::is_c_string;
using ::kotatsu::zest::detail::keep_registered;
using ::kotatsu::zest::detail::reject_bitwise;
using ::kotatsu::zest::detail::reject_chain;
using ::kotatsu::zest::detail::reject_logic;
using ::kotatsu::zest::detail::reject_shift;
using ::kotatsu::zest::detail::relate;
using ::kotatsu::zest::detail::report_failure;
using ::kotatsu::zest::detail::reset_snapshot_context;
using ::kotatsu::zest::detail::set_snapshot_dir;
using ::kotatsu::zest::detail::set_update_snapshots;
using ::kotatsu::zest::detail::snapshot_text;
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
export namespace kotatsu::sys::windows {
using ::kotatsu::sys::windows::CommandLineRules;
using ::kotatsu::sys::windows::create_process_directories;
using ::kotatsu::sys::windows::find_create_process_program;
using ::kotatsu::sys::windows::join_command_line;
using ::kotatsu::sys::windows::ntstatus_name;
using ::kotatsu::sys::windows::quote_argument;
using ::kotatsu::sys::windows::split_command_line;
using ::kotatsu::sys::windows::split_command_line_view;
}
#endif

#if (defined(__APPLE__) && defined(__aarch64__) && defined(NDEBUG)) || (defined(_WIN32) && defined(__aarch64__) && defined(NDEBUG))
export namespace simdjson::arm64::ondemand {
using ::simdjson::arm64::ondemand::stage1_worker;
}
#endif
