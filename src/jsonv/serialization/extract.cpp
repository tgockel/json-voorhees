/// \file
/// Extraction of C++ types from JSON values.
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization/extract.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/value.hpp>

#include <algorithm>
#include <exception>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "describe.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extractor                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extractor::~extractor() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_error::problem                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_error::problem::problem(jsonv::path path, std::string message, std::exception_ptr cause) noexcept :
        _path(std::move(path)),
        _message(std::move(message)),
        _cause(std::move(cause))
{
    if (_message.empty())
        _message = "Unknown problem";
}

extraction_error::problem::problem(jsonv::path path, std::string message) noexcept :
        problem(std::move(path), std::move(message), nullptr)
{ }

extraction_error::problem::problem(jsonv::path path, std::exception_ptr cause) noexcept :
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
// extraction_error                                                                                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string make_extraction_error_errmsg(const extraction_error::problem_list& problems)
{
    std::ostringstream os;

    auto write_problem =
        [&](const extraction_error::problem& problem)
        {
            if (!problem.path().empty())
                os << " at " << problem.path();

            // The separator is unconditional: an empty path used to run "Extraction error" straight into the message.
            os << ": " << problem.message();
        };

    if (problems.size() == 0U)
    {
        os << "Extraction error with unspecified problem";
    }
    else if (problems.size() == 1U)
    {
        os << "Extraction error";
        write_problem(problems[0]);
    }
    else if (problems.size() > 1U)
    {
        os << problems.size() << " extraction errors:";

        for (const auto& problem : problems)
        {
            os << std::endl;
            os << " -";
            write_problem(problem);
        }
    }

    return std::move(os).str();
}

/// Establish what \c extraction_error::problems documents: there is always at least one \c problem.
///
/// \c path and \c nested_ptr each guard the empty case themselves and hand back a static empty value, which left
/// \c problems -- the one accessor with nothing sensible to fall back on -- returning an empty list against its own
/// documentation. A caller which iterates \c problems to report what went wrong and a caller which reads \c what
/// should not disagree about whether anything did.
static extraction_error::problem_list& ensure_nonempty(extraction_error::problem_list& problems)
{
    if (problems.empty())
        problems.emplace_back(jsonv::path(), "Unspecified extraction error");

    return problems;
}

extraction_error::extraction_error(problem_list problems) noexcept :
        // The base is initialised first, so normalising here is also what `_problems` below ends up with.
        std::runtime_error(make_extraction_error_errmsg(ensure_nonempty(problems))),
        _problems(std::move(problems))
{ }

template <typename... TArgs>
extraction_error::extraction_error(std::in_place_t, TArgs&&... args) noexcept :
        extraction_error(problem_list({ problem(std::forward<TArgs>(args)...) }))
{ }

extraction_error::extraction_error(jsonv::path path, std::string message, std::exception_ptr cause) noexcept :
        extraction_error(std::in_place, std::move(path), std::move(message), std::move(cause))
{ }

extraction_error::extraction_error(jsonv::path path, std::string message) noexcept :
        extraction_error(std::in_place, std::move(path), std::move(message))
{ }

extraction_error::extraction_error(jsonv::path path, std::exception_ptr cause) noexcept :
        extraction_error(std::in_place, std::move(path), std::move(cause))
{ }

extraction_error::~extraction_error() noexcept = default;

const path& extraction_error::path() const noexcept
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

const std::exception_ptr& extraction_error::nested_ptr() const noexcept
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
// extract_options                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extract_options::extract_options() noexcept = default;

extract_options::~extract_options() noexcept = default;

extract_options extract_options::create_default()
{
    return extract_options();
}

extract_options& extract_options::failure_mode(on_error mode)
{
    _failure_mode = mode;
    return *this;
}

extract_options& extract_options::max_failures(size_type limit)
{
    _max_failures = limit;
    return *this;
}

extract_options& extract_options::on_duplicate_key(duplicate_key_action action)
{
    _on_duplicate_key = action;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_context::path_scope                                                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_context::path_scope::path_scope(extraction_context& context, std::size_t index) noexcept :
        _context(&context),
        _parent(context._innermost),
        _element(std::in_place_type<std::size_t>, index)
{
    context._innermost = this;
}

extraction_context::path_scope::path_scope(extraction_context& context, std::string_view key) noexcept :
        _context(&context),
        _parent(context._innermost),
        _element(std::in_place_type<std::string_view>, key)
{
    context._innermost = this;
}

extraction_context::path_scope::path_scope(extraction_context& context, path_element elem) :
        _context(&context),
        _parent(context._innermost),
        _element(std::in_place_type<path_element>, std::move(elem))
{
    context._innermost = this;
}

extraction_context::path_scope::path_scope(extraction_context& context, const jsonv::path& subpath) noexcept :
        _context(&context),
        _parent(context._innermost),
        _element(std::in_place_type<const jsonv::path*>, &subpath)
{
    context._innermost = this;
}

extraction_context::path_scope::~path_scope() noexcept
{
    // Unlinking rather than restoring a saved copy is the whole point: this runs on the success path of every element
    // of every array and must not allocate or free. It is also why scopes have to be destroyed in reverse order of
    // construction -- which, being stack objects, they are, including when the stack is unwound by an exception.
    _context->_innermost = _parent;
}

void extraction_context::path_scope::append_to(jsonv::path& out) const
{
    if (_parent)
        _parent->append_to(out);

    if (auto idx = std::get_if<std::size_t>(&_element))
        out += path_element(*idx);
    else if (auto key = std::get_if<std::string_view>(&_element))
        out += path_element(*key);
    else if (auto elem = std::get_if<path_element>(&_element))
        out += *elem;
    else
        out += **std::get_if<const jsonv::path*>(&_element);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extraction_context                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extraction_context::extraction_context(jsonv::formats                fmt,
                                       std::optional<jsonv::version> ver,
                                       jsonv::path                   p,
                                       const void*                   userdata,
                                       extract_options               options
                                      ) :
        context(std::move(fmt), std::move(ver), userdata),
        _options(std::move(options)),
        _base_path(std::move(p))
{ }

extraction_context::extraction_context() :
        context()
{ }

extraction_context::~extraction_context() noexcept = default;

path extraction_context::path() const
{
    jsonv::path out(_base_path);
    if (_innermost)
        _innermost->append_to(out);
    return out;
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
    catch (...)
    { }

    return path();
}

/// The structure enclosing wherever \a from is.
///
/// Used when the bridge has already walked the cursor past the value which failed: naming the cursor would blame the
/// next sibling, which is both wrong and -- since it is a value which extracted perfectly well -- actively
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
            auto type        = from.current().type();
            on_closing_token = type == ast_node_type::object_end
                            || type == ast_node_type::array_end
                            || type == ast_node_type::document_end;
        }
    }
    catch (...)
    { }

    if (!on_closing_token)
        out.pop_back();

    return out;
}

path extraction_context::problem_path(const reader& from) const
{
    // A scope says where the *extractor* is, which is authoritative over anything derived from the reader.
    if (_innermost || !_base_path.empty())
        return path();
    else
        return safe_current_path(from);
}

path extraction_context::take_failure_path(const reader& from)
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

bool extraction_context::recover() const noexcept
{
    // The problem is already recorded, so the question is whether there is room for another one.
    return _options.failure_mode() == extract_options::on_error::collect_all
        && _problems.size() < _options.max_failures();
}

bool extraction_context::recover(const extraction_error& ex)
{
    if (_options.failure_mode() != extract_options::on_error::collect_all)
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

void extraction_context::note_value_consumed(const reader& from) noexcept
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
        catch (...)
        { }
    }
}

void extraction_context::skip_failed_value(reader& from) noexcept
{
    // Matched against the reader the note was left for rather than merely taken: an adapter on the bridge may run
    // nested extractions through readers of its own, and a note left on one of those must not answer for a position
    // in this one.
    if (std::exchange(_consumed_failed_value, nullptr) != &from)
        (void) from.next_value();
}

extraction_context::problem_list extraction_context::take_problems_since(problem_list::size_type mark)
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

std::expected<void, ast_node_type> extraction_context::expect(reader& from, ast_node_type type)
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

std::expected<void, ast_node_type> extraction_context::expect(reader&                              from,
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

std::expected<void, ast_node_type> extraction_context::extract(const std::type_info& type, reader& from, void* into)
{
    // A deposited location lives and dies with this call: whatever leaves one does so while this call is unwinding,
    // and the handlers below are what read it. Clearing on the way in matters too, because `formats::extract` is
    // public and reaches the bridge without passing through here -- a caller who catches that exception themselves
    // leaves a location behind which belongs to nothing.
    _failure_path.reset();
    auto discard_deposit = detail::on_scope_exit([this] { _failure_path.reset(); });

    // Cleared on the way in but deliberately *not* on the way out: the note is left by a destructor running as this
    // call unwinds and is read by whoever recovers from the failure, which is after this returns. Bounding it to one
    // extraction is what keeps a note nobody collects from answering for an unrelated position later.
    _consumed_failed_value = nullptr;

    try
    {
        return formats().extract(type, from, into, *this);
    }
    catch (const extraction_error& ex)
    {
        // An adapter on the value bridge reports failure by throwing, since that is the interface it was written
        // against. Fold what it collected onto this context so the path and message survive the boundary, then carry
        // on down the std::expected channel the rest of the pipeline speaks. The value-based overloads below hand
        // their problems to the exception rather than leaving them here, so nothing is recorded twice.
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

void extraction_context::extract(const std::type_info& type, const value& from, void* into)
{
    auto   mark = _problems.size();
    reader rdr  = reader::from_value(from);

    // A fresh reader sits on document_start; an extractor wants to be looking at the value itself.
    (void) rdr.next_token();

    if (!extract(type, rdr, into))
        throw extraction_error(take_problems_since(mark));
}

void extraction_context::extract_sub(const std::type_info& type,
                                     const value&          from,
                                     jsonv::path           subpath,
                                     void*                 into
                                    )
{
    // `subpath` is a by-value parameter, so it outlives the scope which borrows it.
    path_scope scope(*this, subpath);

    auto mark = _problems.size();
    try
    {
        extract(type, from.at_path(subpath), into);
    }
    catch (const extraction_error&)
    {
        throw;
    }
    catch (const std::exception& ex)
    {
        // `value::at_path` reports a missing element by throwing std::out_of_range. A caller asking for a subpath
        // wants that as an extraction failure naming the subpath, not a stray standard exception.
        (void) problem(path(), ex.what(), std::current_exception());
        throw extraction_error(take_problems_since(mark));
    }
    catch (...)
    {
        (void) problem(path(),
                       std::string("Exception with type ") + current_exception_type_name(),
                       std::current_exception()
                      );
        throw extraction_error(take_problems_since(mark));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// read_value                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static value read_value_impl(reader& from);

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
        throw extraction_error(safe_current_path(from), std::move(os).str());
    }
    }
}

static value read_object(reader& from)
{
    value out = object();

    // Step off the `{` and onto the first key, or onto the `}` of an empty object.
    (void) from.next_token();
    while (from.good())
    {
        if (from.current().type() == ast_node_type::object_end)
        {
            // One past the `}` -- the contract every caller of read_value is promised.
            (void) from.next_token();
            return out;
        }

        std::string key = read_key(from);
        if (!from.next_token())
            break;

        // read_value_impl leaves the cursor on the next key or on the `}`, so this loop never advances itself.
        value member = read_value_impl(from);

        // Assignment rather than `insert`, which keeps the *first* of a duplicated key. `parse_index::extract_tree`
        // defaults to `duplicate_key_action::replace`, and a reader disagreeing with `parse` about which of
        // `{"x":1,"x":2}` survives would make extracting from text and extracting from the parsed tree select
        // different data.
        out[std::move(key)] = std::move(member);
    }

    throw extraction_error(jsonv::path(), "Unterminated object");
}

static value read_array(reader& from)
{
    value out = array();

    // Step off the `[` and onto the first element, or onto the `]` of an empty array.
    (void) from.next_token();
    while (from.good())
    {
        if (from.current().type() == ast_node_type::array_end)
        {
            (void) from.next_token();
            return out;
        }

        // read_value_impl leaves the cursor on the next element or on the `]`, so this loop never advances itself.
        out.push_back(read_value_impl(from));
    }

    throw extraction_error(jsonv::path(), "Unterminated array");
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
        return value(node.as<ast_node::integer>().value());
    case ast_node_type::decimal:
        return value(node.as<ast_node::decimal>().value());
    default:
        return value();
    }
}

static value read_value_impl(reader& from)
{
    const auto& node = from.current();
    switch (node.type())
    {
    case ast_node_type::object_begin:
        return read_object(from);
    case ast_node_type::array_begin:
        return read_array(from);
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
    throw extraction_error(safe_current_path(from), std::move(os).str());
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

detail::borrowed_subtree::borrowed_subtree(extraction_context& context, reader& from) :
        _context(&context),
        _from(&from),
        _borrowed(nullptr),
        _materialised(false),
        _advanced(false),
        _committed(false),
        _uncaught_on_entry(std::uncaught_exceptions())
{
    if (from.good() && from.current().type() == ast_node_type::document_start)
        (void) from.next_token();

    if (const value* lent = from.current_value())
    {
        // The reader is walking a tree which already exists, so hand out the node itself and leave the cursor on it.
        _borrowed = lent;
    }
    else if (from.good() && is_scalar(from.current().type()))
    {
        // One token is the whole value, so it can be read without moving.
        _owned        = read_scalar(from.current());
        _materialised = true;
    }
    else
    {
        // A structure has to be walked to be read, so the cursor is past it by the time this returns and `commit` has
        // nothing left to do. This is the one shape whose failures name the following sibling rather than the
        // structure itself: extracting from a `value` never reaches it, since that reader lends instead, and cases 07
        // through 09 remove it for text by giving these adapters a reader of their own. Noting the position up front
        // instead would mean building a path before every successful extraction, which on a text-backed reader
        // rescans from the start of the document and turns a streaming loop quadratic.
        _owned        = read_value(from);
        _materialised = true;
        _advanced     = true;
    }

    if (_materialised)
        ++_context->_materialised_depth;
}

detail::borrowed_subtree::~borrowed_subtree() noexcept
{
    if (_materialised)
        --_context->_materialised_depth;

    // Walked past the value, and the older body it was walked for did not succeed. Whatever recovers from this has to
    // be told, or its own step over the failed value lands on the sibling after the one it meant to skip. This is
    // outside the unwinding check below because a failure reported by returning is just as consuming as one thrown.
    if (_advanced && !_committed)
        _context->note_value_consumed(*_from);

    if (!_advanced || std::uncaught_exceptions() <= _uncaught_on_entry)
        return;

    // A live scope already says where the extractor is and wins over anything derived from the reader, so there is
    // nothing to look up -- which also means not decoding the keys a lookup would walk through.
    if (_context->_innermost || !_context->_base_path.empty())
        return;

    // Leaving through a throw, having already walked the cursor past the value which failed. The handler which turns
    // that into a problem runs *after* this destructor, and by then the cursor names an unrelated sibling -- so say
    // where the failure belongs now, while there is still something true to say about it. Nothing named the position
    // before the walk because doing so would mean building a path before every successful extraction.
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
        catch (...)
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

value read_value(reader& from)
{
    if (from.good() && from.current().type() == ast_node_type::document_start)
        (void) from.next_token();

    if (!from.good())
        throw extraction_error(jsonv::path(), "Unexpected end of input while reading a value");

    return read_value_impl(from);
}

}
