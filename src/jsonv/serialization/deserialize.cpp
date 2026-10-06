/// \file
/// Deserialization of C++ types from JSON values.
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization/deserialize.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/detail.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "../reader_impl.hpp"
#include "describe.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deserializer                                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

deserializer::~deserializer() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deserialization_error::problem                                                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

deserialization_error::problem::problem(jsonv::path path, std::string message, std::exception_ptr cause) noexcept :
        _path(std::move(path)),
        _message(std::move(message)),
        _cause(std::move(cause))
{
    if (_message.empty())
        _message = "Unknown problem";
}

deserialization_error::problem::problem(jsonv::path path, std::string message) noexcept :
        problem(std::move(path), std::move(message), nullptr)
{ }

deserialization_error::problem::problem(jsonv::path path, std::exception_ptr cause) noexcept :
        problem(std::move(path),
                [&]() -> std::string
                {
                    try
                    {
                        std::rethrow_exception(cause);
                    }
                    catch (const std::exception& ex)
                    {
                        return ex.what();
                    }
                    catch (...)
                    {
                        std::ostringstream os;
                        os << "Exception with type " << current_exception_type_name();
                        return std::move(os).str();
                    }
                }(),
                cause
               )
{ }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deserialization_error                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string make_deserialization_error_errmsg(const deserialization_error::problem_list& problems)
{
    std::ostringstream os;

    auto write_problem =
        [&](const deserialization_error::problem& problem)
        {
            // An empty path is not the root of the document but no position at all, so it gets no `#.` either.
            if (!problem.source_name().empty())
            {
                os << " at " << problem.source_name();
                if (!problem.path().empty())
                    os << '#' << problem.path();
            }
            else if (!problem.path().empty())
            {
                os << " at " << problem.path();
            }

            // The separator is unconditional: an empty path used to run "Deserialization error" straight into the
            // message.
            os << ": " << problem.message();
        };

    if (problems.size() == 0U)
    {
        os << "Deserialization error with unspecified problem";
    }
    else if (problems.size() == 1U)
    {
        os << "Deserialization error";
        write_problem(problems[0]);
    }
    else if (problems.size() > 1U)
    {
        os << problems.size() << " deserialization errors:";

        for (const auto& problem : problems)
        {
            os << '\n';
            os << " -";
            write_problem(problem);
        }
    }

    return std::move(os).str();
}

/// Establish what \c deserialization_error::problems documents: there is always at least one \c problem.
///
/// \c path and \c nested_ptr each guard the empty case themselves and hand back a static empty value, which left
/// \c problems -- the one accessor with nothing sensible to fall back on -- returning an empty list against its own
/// documentation. A caller which iterates \c problems to report what went wrong and a caller which reads \c what
/// should not disagree about whether anything did.
static deserialization_error::problem_list& ensure_nonempty(deserialization_error::problem_list& problems)
{
    if (problems.empty())
        problems.emplace_back(jsonv::path(), "Unspecified deserialization error");

    return problems;
}

deserialization_error::deserialization_error(problem_list problems) noexcept :
        // The base is initialised first, so normalising here is also what `_problems` below ends up with.
        std::runtime_error(make_deserialization_error_errmsg(ensure_nonempty(problems))),
        _problems(std::move(problems))
{ }

template <typename... TArgs>
deserialization_error::deserialization_error(std::in_place_t, TArgs&&... args) noexcept :
        deserialization_error(problem_list({ problem(std::forward<TArgs>(args)...) }))
{ }

deserialization_error::deserialization_error(jsonv::path path, std::string message, std::exception_ptr cause) noexcept :
        deserialization_error(std::in_place, std::move(path), std::move(message), std::move(cause))
{ }

deserialization_error::deserialization_error(jsonv::path path, std::string message) noexcept :
        deserialization_error(std::in_place, std::move(path), std::move(message))
{ }

deserialization_error::deserialization_error(jsonv::path path, std::exception_ptr cause) noexcept :
        deserialization_error(std::in_place, std::move(path), std::move(cause))
{ }

deserialization_error::~deserialization_error() noexcept = default;

const path& deserialization_error::path() const noexcept
{
    if (_problems.empty())
    {
        static const jsonv::path empty_path;
        return empty_path;
    }
    else
    {
        return _problems[0].path();
    }
}

const std::string& deserialization_error::source_name() const noexcept
{
    if (_problems.empty())
    {
        static const std::string empty_name;
        return empty_name;
    }
    else
    {
        return _problems[0].source_name();
    }
}

const std::exception_ptr& deserialization_error::nested_ptr() const noexcept
{
    if (_problems.empty())
    {
        static const std::exception_ptr empty_ex;
        return empty_ex;
    }
    else
    {
        return _problems[0].nested_ptr();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deserialize_options                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

deserialize_options::deserialize_options() noexcept = default;

deserialize_options::~deserialize_options() noexcept = default;

deserialize_options deserialize_options::create_default()
{
    return deserialize_options();
}

deserialize_options& deserialize_options::failure_mode(on_error mode)
{
    _failure_mode = mode;
    return *this;
}

deserialize_options& deserialize_options::max_failures(size_type limit)
{
    _max_failures = limit;
    return *this;
}

deserialize_options& deserialize_options::on_duplicate_key(duplicate_key_action action)
{
    _on_duplicate_key = action;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deserialization_context::path_scope                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

deserialization_context::path_scope::path_scope(deserialization_context& context, std::size_t index) noexcept :
        _context(&context),
        _parent(context._innermost),
        _element(std::in_place_type<std::size_t>, index)
{
    context._innermost = this;
}

deserialization_context::path_scope::path_scope(deserialization_context& context, std::string_view key) noexcept :
        _context(&context),
        _parent(context._innermost),
        _element(std::in_place_type<std::string_view>, key)
{
    context._innermost = this;
}

deserialization_context::path_scope::path_scope(deserialization_context& context, path_element elem) :
        _context(&context),
        _parent(context._innermost),
        _element(std::in_place_type<path_element>, std::move(elem))
{
    context._innermost = this;
}

deserialization_context::path_scope::~path_scope() noexcept
{
    // Unlinking rather than restoring a saved copy is the whole point: this runs on the success path of every element
    // of every array and must not allocate or free. It is also why scopes have to be destroyed in reverse order of
    // construction -- which, being stack objects, they are, including when the stack is unwound by an exception.
    _context->_innermost = _parent;
}

void deserialization_context::path_scope::append_to(jsonv::path& out) const
{
    if (_parent)
        _parent->append_to(out);

    if (auto idx = std::get_if<std::size_t>(&_element))
        out += path_element(*idx);
    else if (auto key = std::get_if<std::string_view>(&_element))
        out += path_element(*key);
    else
        out += *std::get_if<path_element>(&_element);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deserialization_context                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

deserialization_context::deserialization_context(jsonv::formats                fmt,
                                                 std::optional<jsonv::version> ver,
                                                 jsonv::path                   p,
                                                 const void*                   userdata,
                                                 deserialize_options           options,
                                                 std::string                   source_name
                                                ) :
        // Neither `formats` nor `deserialize_options` can be moved yet (#286).
        context(std::move(fmt), ver, userdata), // NOLINT(performance-move-const-arg)
        _options(std::move(options)),           // NOLINT(performance-move-const-arg)
        _base_path(std::move(p)),
        _source_name(std::move(source_name))
{ }

deserialization_context::deserialization_context() :
        context()
{ }

deserialization_context::~deserialization_context() noexcept = default;

path deserialization_context::path() const
{
    jsonv::path out(_base_path);
    if (_innermost)
        _innermost->append_to(out);
    return out;
}

std::string_view deserialization_context::encoded_source() const
{
    const auto* scope = _innermost_source;
    if (!scope || scope->_showing != detail::source_scope::shows::value_and_text)
        return std::string_view();
    else if (!scope->_tree)
        return scope->_text;

    if (!scope->_encoded)
        scope->_encoded.emplace(to_string(*scope->_tree));

    return *scope->_encoded;
}

optional<const value&> deserialization_context::source_value() const
{
    using shows = detail::source_scope::shows;

    const auto* scope = _innermost_source;
    if (!scope || scope->_showing == shows::nothing)
        return std::nullopt;
    else if (scope->_tree)
        return scope->_tree;

    if (!scope->_read)
    {
        // Before the walk the reader is still on the value. After it the reader is on the object's `}`, and the
        // bookmark is the way back to its `{` -- which a source with no tape would not have, and could not show.
        if (scope->_showing == shows::value)
            scope->_read.emplace(detail::peek_value(*this, *scope->_from));
        else if (scope->_bookmark)
            scope->_read.emplace(detail::peek_value_at(*this, *scope->_from, *scope->_bookmark));
        else
            return std::nullopt;
    }

    scope->lend_temporary();
    return *scope->_read;
}

/// The reader's path, or an empty one if asking for it fails.
///
/// Building a path decodes the object keys along the way, and a key can be malformed -- an unpaired surrogate throws
/// \c parse_error. Asking is always part of describing some *other* failure, so it must not replace that failure with
/// one of its own, and on an unwind it must not throw at all.
static path safe_current_path(const reader& from) noexcept
{
    try
    {
        if (from.good())
            return from.current_path();
    }
    catch (...) // NOLINT(bugprone-empty-catch): no path is the answer when asking fails
    { }

    return path();
}

/// The structure enclosing wherever \a from is.
///
/// Used when the bridge has already walked the cursor past the value which failed: naming the cursor would blame the
/// next sibling, which is both wrong and -- since it is a value which deserialized perfectly well -- actively
/// misleading. The enclosing structure is still true of the value that failed. Dropping the trailing element is what
/// gets there, except when the cursor has landed on a closing token, where \c reader::current_path already names the
/// structure rather than a member of it.
static path enclosing_path(const reader& from) noexcept
{
    path out = safe_current_path(from);
    if (out.empty())
        return out;

    bool on_closing_token = false;
    try
    {
        if (from.good())
        {
            auto type        = from.current_type();
            on_closing_token = type == ast_node_type::object_end
                            || type == ast_node_type::array_end
                            || type == ast_node_type::document_end;
        }
    }
    catch (...) // NOLINT(bugprone-empty-catch): not knowing means dropping the last element, as for a member
    { }

    if (!on_closing_token)
        out.pop_back();

    return out;
}

path deserialization_context::problem_path(const reader& from) const
{
    // A scope says where the *deserializer* is, which is authoritative over anything derived from the reader.
    if (_innermost || !_base_path.empty())
        return path();
    else
        return safe_current_path(from);
}

path deserialization_context::take_failure_path(const reader& from)
{
    std::optional<jsonv::path> deposited = std::move(_failure_path);
    _failure_path.reset();

    if (_innermost || !_base_path.empty())
    {
        return path();
    }
    else if (deposited)
    {
        // A bridge below has already said where this belongs, because by now the cursor names something else.
        return std::move(*deposited);
    }
    else
    {
        return safe_current_path(from);
    }
}

bool deserialization_context::recover() const noexcept
{
    // The problem is already recorded, so the question is whether there is room for another one.
    return _options.failure_mode() == deserialize_options::on_error::collect_all
        && _problems.size() < _options.max_failures();
}

bool deserialization_context::recover(const deserialization_error& ex)
{
    if (_options.failure_mode() != deserialize_options::on_error::collect_all)
        return false;

    // The problems are still in `ex`, so the question is whether folding them would overrun the budget. Declining
    // leaves them there for the caller to rethrow, which keeps each one recorded exactly once whichever way this
    // goes.
    if (_problems.size() + ex.problems().size() >= _options.max_failures())
        return false;

    for (const auto& p : ex.problems())
        (void) problem(p);

    return true;
}

void deserialization_context::note_value_consumed(const reader& from) noexcept
{
    _consumed_failed_value = &from;

    // The cursor no longer names the value which failed, so where that value was has to be settled now -- the
    // handler which records the problem runs later, by which time the cursor names an unrelated sibling. This is the
    // same deposit `borrowed_subtree` makes for the same reason, and the same approximation: the enclosing structure
    // is still true of the value which failed, where the next sibling is both false and actively misleading.
    //
    // Nothing is computed unless a failure is actually being reported, which is what keeps `reader::current_path`
    // -- a rescan from the start of the document on a text source -- off the success path.
    if (_failure_path || _innermost || !_base_path.empty())
        return;

    try
    {
        _failure_path.emplace(enclosing_path(from));
    }
    catch (...)
    {
        // Naming a position must never cost more than the name.
        try
        {
            _failure_path.emplace();
        }
        catch (...) // NOLINT(bugprone-empty-catch): nothing is left to fall back on
        { }
    }
}

void deserialization_context::skip_failed_value(reader& from) noexcept
{
    // Matched against the reader the note was left for rather than merely taken: an adapter on the bridge may run
    // nested deserializations through readers of its own, and a note left on one of those must not answer for a
    // position in this one.
    if (std::exchange(_consumed_failed_value, nullptr) != &from)
        (void) from.next_value();
}

deserialization_context::problem_list deserialization_context::take_problems_since(problem_list::size_type mark)
{
    using std::begin;
    using std::end;

    if (mark >= _problems.size())
        return problem_list();

    auto first = begin(_problems) + static_cast<problem_list::difference_type>(mark);
    problem_list taken(std::make_move_iterator(first), std::make_move_iterator(end(_problems)));
    _problems.erase(first, end(_problems));
    return taken;
}

std::string_view describe(ast_node_type type)
{
    switch (type)
    {
    case ast_node_type::document_start:   return "start of document";
    case ast_node_type::document_end:     return "end of document";
    case ast_node_type::object_begin:     return "object";
    case ast_node_type::object_end:       return "end of object";
    case ast_node_type::array_begin:      return "array";
    case ast_node_type::array_end:        return "end of array";
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:   return "string";
    case ast_node_type::key_canonical:
    case ast_node_type::key_escaped:      return "object key";
    case ast_node_type::literal_true:     return "true";
    case ast_node_type::literal_false:    return "false";
    case ast_node_type::literal_null:     return "null";
    case ast_node_type::integer:          return "integer";
    case ast_node_type::decimal:          return "decimal";
    case ast_node_type::error:            return "error";
    default:                              return "unknown";
    }
}

std::expected<void, ast_node_type> deserialization_context::expect(reader& from, ast_node_type type)
{
    auto matched = from.expect(type);
    if (matched)
        return {};

    std::ostringstream os;
    os << "Read node of type " << describe(matched.error()) << " when expecting " << describe(type);
    (void) problem(problem_path(from), std::move(os).str());

    // The problem is recorded, but the error channel carries the type actually found rather than the `error`
    // sentinel, so a caller can branch on what was really there.
    return std::unexpected(matched.error());
}

std::expected<void, ast_node_type> deserialization_context::expect(reader&                              from,
                                                                   std::initializer_list<ast_node_type> types
                                                                  )
{
    auto matched = from.expect(types);
    if (matched)
        return {};

    // Several node types share a description -- the two spellings of a string, the two of a key -- and a message
    // reading "one of string, string" would be describing how the source was encoded, which is not the question the
    // caller asked.
    std::vector<std::string_view> wanted;
    for (const auto& type : types)
    {
        auto name = describe(type);
        if (std::find(wanted.begin(), wanted.end(), name) == wanted.end())
            wanted.push_back(name);
    }

    std::ostringstream os;
    os << "Read node of type " << describe(matched.error()) << " when expecting ";
    if (wanted.size() > 1U)
        os << "one of ";

    bool first = true;
    for (const auto& name : wanted)
    {
        if (!std::exchange(first, false))
            os << ", ";
        os << name;
    }
    (void) problem(problem_path(from), std::move(os).str());
    return std::unexpected(matched.error());
}

std::expected<void, ast_node_type>
deserialization_context::deserialize(const std::type_info& type, reader& from, void* into)
{
    // A deposited location lives and dies with this call: whatever leaves one does so while this call is unwinding,
    // and the handlers below are what read it. Clearing on the way in matters too, because `formats::deserialize` is
    // public and reaches the bridge without passing through here -- a caller who catches that exception themselves
    // leaves a location behind which belongs to nothing.
    _failure_path.reset();
    auto discard_deposit = detail::on_scope_exit([this] { _failure_path.reset(); });

    // Cleared on the way in but deliberately *not* on the way out: the note is left by a destructor running as this
    // call unwinds and is read by whoever recovers from the failure, which is after this returns. Bounding it to one
    // deserialization is what keeps a note nobody collects from answering for an unrelated position later.
    _consumed_failed_value = nullptr;

    // An object's hooks are shown it -- before the walk and once it reaches the `}` -- and a hook is free to
    // deserialize something else through this context. Nothing that deserialization runs is part of the object -- and
    // the DSL's adapter is not the only deserializer which might ask -- so it is shown nothing. Part-way through a walk
    // there is nothing showing to hide, so the ordinary path pays for the test and no more.
    std::optional<detail::source_scope> hide_source;
    if (_innermost_source && _innermost_source->_showing != detail::source_scope::shows::nothing)
        hide_source.emplace(*this);

    try
    {
        return formats().deserialize(type, from, into, *this);
    }
    catch (const deserialization_error& ex)
    {
        // An adapter on the value bridge reports failure by throwing, since that is the interface it was written
        // against. Fold what it collected onto this context so the path and message survive the boundary, then carry
        // on down the std::expected channel the rest of the pipeline speaks. `detail::deserialize_entry`, which the
        // value-based overload runs through, hands its problems to the exception rather than leaving them here, so
        // nothing is recorded twice.
        for (const auto& p : ex.problems())
            (void) problem(p);

        return std::unexpected(ast_node_type::error);
    }
    catch (const std::exception& ex)
    {
        return problem(take_failure_path(from), ex.what(), std::current_exception());
    }
    catch (...)
    {
        return problem(take_failure_path(from),
                       std::string("Exception with type ") + current_exception_type_name(),
                       std::current_exception()
                      );
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// detail::deserialize_entry                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void detail::deserialize_entry(deserialization_context&   context,
                               const std::type_info& type,
                               reader&               from,
                               void*                 into,
                               void               (* destroy)(void*) noexcept,
                               source_lifetime       lifetime
                              )
{
    // Only what this call records belongs to the exception it throws. The `value` bridge enters here with a context
    // part-way through a deserialization of its own, and whatever that already holds is for its own caller to report.
    const auto mark = context.problems().size();

    // A depth rather than a flag, so it nests with any `borrowed_subtree` beneath it.
    const bool owned = lifetime == source_lifetime::deserialization;
    if (owned)
        ++context._temporary_source_depth;
    auto release_source = on_scope_exit([&] { if (owned) --context._temporary_source_depth; });

    // Decided once and then trusted by all three of the steps which depend on it -- validating, stepping onto the
    // value, and requiring nothing after it -- so they cannot disagree about whether this is a whole document.
    const auto entry_type     = from.good() ? std::optional(from.current_type()) : std::nullopt;
    const bool whole_document = entry_type == ast_node_type::document_start;

    // An `error` node under the cursor means the parse failed there, so there is no value to deserialize whether or not
    // the caller positioned the reader. It has to be asked about separately because it is not always preceded by a
    // `document_start`: a parse which fails before writing one -- a `max_structure_depth` of 0 does -- leaves a tape
    // which is nothing else.
    if (whole_document || entry_type == ast_node_type::error)
    {
        try
        {
            from.validate();
        }
        catch (const parse_error& ex)
        {
            // A deserializer walking a tape which stopped at an `error` node can only say what it expected to find
            // there instead, which describes the symptom. The parse knows what actually went wrong.
            (void) context.problem(context.problem_path(from),
                                   std::string("Could not parse JSON: ") + ex.what(),
                                   std::current_exception()
                                  );
            throw deserialization_error(context.take_problems_since(mark));
        }
    }

    // The one place `document_start` is stepped over, so that no deserializer is ever entered on one.
    if (whole_document)
        (void) from.next_token();

    if (!context.deserialize(type, from, into))
    {
        // A deserializer is meant to record why it failed, but nothing makes it. Recording the stand-in here, rather
        // than leaving `deserialization_error` to make one up, is what puts it in the document the context names.
        if (context.problems().size() == mark)
            (void) context.problem(jsonv::path(), "Unspecified deserialization error");

        throw deserialization_error(context.take_problems_since(mark));
    }

    if (!whole_document)
        return;

    // There is an object in `into` from here on, so every way out but success destroys it.
    auto discard = on_scope_exit([&] { destroy(into); });

    if (from.good() && from.current_type() == ast_node_type::document_end)
    {
        discard.release();
        return;
    }

    // The value did not take up the whole document. A source with anything after its value fails to parse, so this is
    // a deserializer which left the cursor somewhere other than one past its value. `expect` names what was found
    // instead, but reading `current` off an exhausted reader throws, so that case gets a message of its own.
    if (from.good())
        (void) context.expect(from, ast_node_type::document_end);
    else
        (void) context.problem(context.problem_path(from), "Deserialization read past the end of the document");

    throw deserialization_error(context.take_problems_since(mark));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// read_value                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// \param depth How many structures enclose this value below where the read began.
static value read_value_impl(reader& from, deserialize_options::duplicate_key_action on_duplicate, std::size_t depth);

static std::string read_key(const reader& from)
{
    const auto& node = from.current();
    switch (node.type())
    {
    case ast_node_type::key_canonical:
        return std::string(node.as<ast_node::key_canonical>().value());
    case ast_node_type::key_escaped:
        return node.as<ast_node::key_escaped>().value();
    default:
    {
        std::ostringstream os;
        os << "Read node of type " << describe(node.type()) << " when expecting an object key";
        throw deserialization_error(safe_current_path(from), std::move(os).str());
    }
    }
}

namespace
{

/// A key refused for repeating, part-way through reading a \c value. Its path is the reader's, which is the whole
/// answer only for a deserialization with no location of its own; so it also says how much of that path lies below
/// where the read began, which is the part the reader is still right about. See \c relocate.
class duplicate_key_error final :
        public deserialization_error
{
public:
    explicit duplicate_key_error(jsonv::path where, std::string message, std::size_t below) noexcept :
            deserialization_error(std::move(where), std::move(message)),
            _below(below)
    { }

    /// How many trailing elements of \c path lie below where the read began: the repeated key, and one for each
    /// structure between it and the value the read started on.
    JSONV_NODISCARD
    std::size_t below() const noexcept { return _below; }

private:
    std::size_t _below;
};

}

/// Refuse the object \a from is part-way through for repeating \a key, which \a from is on, \a depth structures below
/// where the read began. The message is the one \c parse_index::extract_tree and the serialization builder DSL raise
/// for the same document.
[[noreturn]] static void throw_duplicate_key(const reader& from, std::string_view key, std::size_t depth)
{
    std::string message("Duplicate key in object: \"");
    message.append(key);
    message.append("\"");
    throw duplicate_key_error(safe_current_path(from), std::move(message), depth + 1U);
}

/// Translate \a ex for \a context. Where the context has a location of its own -- a base path, or a scope saying where
/// the deserializer is -- that location is authoritative over the reader's, as \c deserialization_context::problem_path
/// has it, and only the part below where the read began is the reader's to add. With none, the reader's path stands.
static deserialization_error relocate(const deserialization_context& context, const duplicate_key_error& ex)
{
    jsonv::path located = context.path();
    if (located.empty())
        return deserialization_error(ex.path(), ex.problems().front().message());

    const jsonv::path& found = ex.path();
    const auto         below = static_cast<std::ptrdiff_t>(std::min(ex.below(), found.size()));
    for (auto iter = found.end() - below; iter != found.end(); ++iter)
        located += *iter;

    return deserialization_error(std::move(located), ex.problems().front().message());
}

/// Step \a from over what is left of a structure which failed to read part-way through, up to and including its
/// \a close token -- which is where success would have left the cursor. A position inside the structure means nothing
/// to whoever recovers from the failure: a container resuming from there would take the structure's own close for its
/// own, and lose every sibling after it.
///
/// Stepping over whole children with \c reader::next_value is what makes the only close this can stop on its own; on
/// an object's key it is \c reader::next_token, which lands on the key's value. A truncated tape has no close to find,
/// so this stops on the end of the document instead.
static void finish_structure(reader& from, ast_node_type close)
{
    while (from.good())
    {
        auto type = from.current_type();
        if (type == close)
        {
            (void) from.next_token();
            return;
        }
        else if (type == ast_node_type::document_end || type == ast_node_type::error)
        {
            return;
        }

        (void) from.next_value();
    }
}

static value read_object(reader& from, deserialize_options::duplicate_key_action on_duplicate, std::size_t depth)
{
    // Step off the `{` and onto the first key, or onto the `}` of an empty object -- before anything which can fail,
    // so that a failure always has the object to finish walking rather than still in front of the cursor.
    (void) from.next_token();
    try
    {
        value out = object();
        while (from.good())
        {
            if (from.current_type() == ast_node_type::object_end)
            {
                // One past the `}` -- the contract every caller of read_value is promised.
                (void) from.next_token();
                return out;
            }

            std::string key = read_key(from);

            // Settled while the cursor is still on the key, so a refusal names the key which repeated. The default
            // keeps whichever value comes last, which the assignment below does without having to look first.
            const bool repeated = on_duplicate != deserialize_options::duplicate_key_action::replace
                               && out.count(key) != 0U;
            if (repeated && on_duplicate == deserialize_options::duplicate_key_action::exception)
                throw_duplicate_key(from, key, depth);

            if (!from.next_token())
                break;

            // Stepping over the repeat lands where reading it would have.
            if (repeated)
            {
                (void) from.next_value();
                continue;
            }

            // read_value_impl leaves the cursor on the next key or on the `}`, so this loop never advances itself.
            value member = read_value_impl(from, on_duplicate, depth + 1U);

            // Assignment rather than `insert`, which keeps the *first* of a duplicated key. `parse_index::extract_tree`
            // defaults to `duplicate_key_action::replace`, and a reader disagreeing with `parse` about which of
            // `{"x":1,"x":2}` survives would make deserializing from text and deserializing from the parsed tree select
            // different data.
            out[std::move(key)] = std::move(member);
        }
    }
    catch (...)
    {
        finish_structure(from, ast_node_type::object_end);
        throw;
    }

    throw deserialization_error(jsonv::path(), "Unterminated object");
}

static value read_array(reader& from, deserialize_options::duplicate_key_action on_duplicate, std::size_t depth)
{
    // Step off the `[` and onto the first element, or onto the `]` of an empty array -- before anything which can
    // fail, for the same reason as in `read_object`.
    (void) from.next_token();
    try
    {
        value out = array();
        while (from.good())
        {
            if (from.current_type() == ast_node_type::array_end)
            {
                (void) from.next_token();
                return out;
            }

            // read_value_impl leaves the cursor on the next element or on the `]`, so this loop never advances itself.
            out.push_back(read_value_impl(from, on_duplicate, depth + 1U));
        }
    }
    catch (...)
    {
        finish_structure(from, ast_node_type::array_end);
        throw;
    }

    throw deserialization_error(jsonv::path(), "Unterminated array");
}

static value read_scalar(const ast_node& node)
{
    switch (node.type())
    {
    case ast_node_type::string_canonical:
        return value(node.as<ast_node::string_canonical>().value());
    case ast_node_type::string_escaped:
        return value(node.as<ast_node::string_escaped>().value());
    case ast_node_type::literal_true:
        return value(true);
    case ast_node_type::literal_false:
        return value(false);
    case ast_node_type::integer:
        return integer_node_value(node.as<ast_node::integer>());
    case ast_node_type::decimal:
        return value(node.as<ast_node::decimal>().value());
    default:
        return value();
    }
}

static value read_value_impl(reader& from, deserialize_options::duplicate_key_action on_duplicate, std::size_t depth)
{
    const auto& node = from.current();
    switch (node.type())
    {
    case ast_node_type::object_begin:
        return read_object(from, on_duplicate, depth);
    case ast_node_type::array_begin:
        return read_array(from, on_duplicate, depth);
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
    case ast_node_type::literal_true:
    case ast_node_type::literal_false:
    case ast_node_type::literal_null:
    case ast_node_type::integer:
    case ast_node_type::decimal:
    {
        value out = read_scalar(node);
        (void) from.next_token();
        return out;
    }
    case ast_node_type::document_start:
    case ast_node_type::document_end:
    case ast_node_type::object_end:
    case ast_node_type::array_end:
    case ast_node_type::key_canonical:
    case ast_node_type::key_escaped:
    case ast_node_type::error:
        break;
    }

    std::ostringstream os;
    os << "Unexpected token of type " << describe(node.type()) << " while reading a value";
    throw deserialization_error(safe_current_path(from), std::move(os).str());
}

/// Is a node the whole of a value, rather than the opening of a structure which has to be walked to be read?
static bool is_scalar(ast_node_type type)
{
    switch (type)
    {
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
    case ast_node_type::literal_true:
    case ast_node_type::literal_false:
    case ast_node_type::literal_null:
    case ast_node_type::integer:
    case ast_node_type::decimal:
        return true;
    default:
        return false;
    }
}

detail::borrowed_subtree::borrowed_subtree(deserialization_context& context, reader& from) :
        _context(&context),
        _from(&from),
        _borrowed(),
        _materialised(false),
        _advanced(false),
        _committed(false),
        _uncaught_on_entry(std::uncaught_exceptions())
{
    if (from.good() && from.current_type() == ast_node_type::document_start)
        (void) from.next_token();

    if (auto lent = from.current_value())
    {
        // The reader is walking a tree which already exists, so hand out the node itself and leave the cursor on it.
        _borrowed = lent;
    }
    else if (from.good() && is_scalar(from.current_type()))
    {
        // One token is the whole value, so it can be read without moving.
        _owned        = read_scalar(from.current());
        _materialised = true;
    }
    else
    {
        // A structure has to be walked to be read, so the cursor is past it by the time this returns and `commit` has
        // nothing left to do. This is the one shape whose failures name the following sibling rather than the structure
        // itself: deserializing from a `value` never reaches it, since that reader lends instead, and cases 07 through
        // 09 remove it for text by giving these adapters a reader of their own. Noting the position up front instead
        // would mean building a path before every successful deserialization, which on a text-backed reader rescans
        // from the start of the document and turns a streaming loop quadratic.
        //
        // A structure which fails to materialise is walked past all the same, and with no object constructed there is
        // no destructor to say so -- which is why this is the overload that tells the context.
        _owned        = read_value(context, from);
        _materialised = true;
        _advanced     = true;
    }

    if (_materialised)
        ++_context->_temporary_source_depth;
}

detail::borrowed_subtree::~borrowed_subtree() noexcept
{
    if (_materialised)
        --_context->_temporary_source_depth;

    // Walked past the value, and the older body it was walked for did not succeed. Whatever recovers from this has to
    // be told, or its own step over the failed value lands on the sibling after the one it meant to skip. This is
    // outside the unwinding check below because a failure reported by returning is just as consuming as one thrown.
    if (_advanced && !_committed)
        _context->note_value_consumed(*_from);

    if (!_advanced || std::uncaught_exceptions() <= _uncaught_on_entry)
        return;

    // A live scope already says where the deserializer is and wins over anything derived from the reader, so there is
    // nothing to look up -- which also means not decoding the keys a lookup would walk through.
    if (_context->_innermost || !_context->_base_path.empty())
        return;

    // Leaving through a throw, having already walked the cursor past the value which failed. The handler which turns
    // that into a problem runs *after* this destructor, and by then the cursor names an unrelated sibling -- so say
    // where the failure belongs now, while there is still something true to say about it. Nothing named the position
    // before the walk because doing so would mean building a path before every successful deserialization.
    try
    {
        _context->_failure_path.emplace(enclosing_path(*_from));
    }
    catch (...)
    {
        // Naming a position must never cost more than the name. An engaged-but-empty deposit also keeps the handler
        // from retrying the scan which just failed.
        try
        {
            _context->_failure_path.emplace();
        }
        catch (...) // NOLINT(bugprone-empty-catch): nothing is left to fall back on
        { }
    }
}

void detail::borrowed_subtree::commit()
{
    _committed = true;

    if (!_advanced)
    {
        (void) _from->next_value();
        _advanced = true;
    }
}

/// \ref read_value, settling a repeated key by \a on_duplicate.
static value read_value_settling(reader& from, deserialize_options::duplicate_key_action on_duplicate)
{
    if (from.good() && from.current_type() == ast_node_type::document_start)
        (void) from.next_token();

    if (!from.good())
        throw deserialization_error(jsonv::path(), "Unexpected end of input while reading a value");

    return read_value_impl(from, on_duplicate, 0U);
}

value read_value(reader& from)
{
    return read_value_settling(from, deserialize_options::duplicate_key_action::replace);
}

value read_value(deserialization_context& context, reader& from)
{
    if (from.good() && from.current_type() == ast_node_type::document_start)
        (void) from.next_token();

    // A scalar is converted before the cursor steps over it, and a structure is stepped into before anything which can
    // fail, so whether a failure leaves the value behind the cursor is exactly whether it was a structure.
    const bool structure = from.good()
                        && (  from.current_type() == ast_node_type::object_begin
                           || from.current_type() == ast_node_type::array_begin
                           );
    try
    {
        return read_value_settling(from, context.options().on_duplicate_key());
    }
    catch (const duplicate_key_error& ex)
    {
        // Only an object refuses a repeated key, and it is walked to its end before the refusal gets out.
        context.note_value_consumed(from);
        throw relocate(context, ex);
    }
    catch (...)
    {
        if (structure)
            context.note_value_consumed(from);
        throw;
    }
}

value detail::peek_value(const deserialization_context& context, const reader& from)
{
    // A scalar is one token, which a reader over text can read where it sits -- opening a second cursor to read it
    // costs an allocation, which is all looking up a number in an `enum_adapter` would otherwise pay. A value-backed
    // reader would have to synthesise the node into the caller's arena to do the same, where it would stay for as long
    // as that reader lives, so that one still reads through a second cursor.
    if (!from.current_value() && from.good() && is_scalar(from.current_type()))
        return read_scalar(from.current());

    reader probe = reader_lookahead::open(from);
    try
    {
        return read_value_settling(probe, context.options().on_duplicate_key());
    }
    catch (const duplicate_key_error& ex)
    {
        throw relocate(context, ex);
    }
}

std::optional<parse_index::const_iterator> detail::bookmark(const reader& from) noexcept
{
    return reader_lookahead::mark(from);
}

value detail::peek_value_at(const deserialization_context& context, const reader& from, parse_index::const_iterator at)
{
    reader probe = reader_lookahead::open(from, at);
    try
    {
        return read_value_settling(probe, context.options().on_duplicate_key());
    }
    catch (const duplicate_key_error& ex)
    {
        throw relocate(context, ex);
    }
}

value detail::peek_members(const deserialization_context&                 context,
                           const reader&                             from,
                           const std::set<std::string, std::less<>>& keys
                          )
{
    const auto on_duplicate = context.options().on_duplicate_key();

    reader probe = reader_lookahead::open(from);
    if (probe.good() && probe.current_type() == ast_node_type::document_start)
        (void) probe.next_token();

    if (!probe.good() || probe.current_type() != ast_node_type::object_begin)
        return value();

    value out = object();

    // Onto the first key, or onto the `}` of an empty object. Anything other than a key from here on is that `}` or
    // the end of a document which stopped part-way through, and either way there are no more members to find.
    (void) probe.next_token();
    try
    {
        while (probe.good() && (  probe.current_type() == ast_node_type::key_canonical
                               || probe.current_type() == ast_node_type::key_escaped
                               )
              )
        {
            // Compared before it is copied, so a canonical key nobody asked for costs no allocation.
            auto wanted = probe.current().visit_key([&] (const auto& k) -> std::optional<std::string>
                                                    {
                                                        auto name = k.value();
                                                        if (keys.contains(name))
                                                            return std::string(std::move(name));
                                                        else
                                                            return std::nullopt;
                                                    }
                                                   );

            // A repeat is settled as `read_object` settles one, and on the key for the same reason. The object being
            // walked is where the read began, so it is no structures deep.
            const bool repeated = wanted
                               && on_duplicate != deserialize_options::duplicate_key_action::replace
                               && out.count(*wanted) != 0U;
            if (repeated && on_duplicate == deserialize_options::duplicate_key_action::exception)
                throw_duplicate_key(probe, *wanted, 0U);

            // Onto the member's value, whether or not it is wanted. A document which ends here has nothing more to
            // find.
            if (  !probe.next_token()
               || probe.current_type() == ast_node_type::document_end
               || probe.current_type() == ast_node_type::error
               )
            {
                break;
            }

            // `reader::next_key` would do for an unwanted member, but over text it walks every token of the value to
            // find the next key. Stepping over the value itself jumps the whole subtree.
            if (!wanted || repeated)
            {
                (void) probe.next_value();
                continue;
            }

            // read_value_impl leaves the cursor on the next key or on the `}`, so this loop never advances itself.
            out[*std::move(wanted)] = read_value_impl(probe, on_duplicate, 1U);
        }
    }
    catch (const duplicate_key_error& ex)
    {
        throw relocate(context, ex);
    }
    return out;
}

}
