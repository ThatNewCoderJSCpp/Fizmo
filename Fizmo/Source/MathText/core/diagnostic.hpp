#ifndef FIZMO_MATHTEXT_DIAGNOSTIC_HPP
#define FIZMO_MATHTEXT_DIAGNOSTIC_HPP

#include <cstdint>
#include <string>
#include "span.hpp"

namespace fizmo {
namespace mathtext {

#define FIZMO_MATHTEXT_DIAGNOSTICS(X) \
    X(UnquotedText,          Error,   "text must be quoted with \"...\" or '...'") \
    X(InvalidNumber,         Error,   "invalid number") \
    X(UnterminatedString,    Error,   "unterminated string") \
    X(UnexpectedCharacter,   Error,   "unexpected character") \
    X(InvalidNegation,       Error,   "'-' must be attached to a number, symbol, call or string") \
    X(UnexpectedComma,       Error,   "unexpected ','") \
    X(UnexpectedCloseBrace,  Error,   "unexpected '}'") \
    X(MissingSeparator,      Error,   "expected ',' or '}' after argument") \
    X(UnclosedCall,          Error,   "missing '}' to close this call") \
    X(NestingTooDeep,        Error,   "nesting is too deep") \
    X(UnclosedCommand,       Error,   "command is missing its closing '\\'") \
    X(LoneBackslash,         Error,   "'\\' must be followed by a command name") \
    X(MissingOperand,        Error,   "expected a value after this operator") \
    X(UnexpectedOperator,    Error,   "operator has nothing on its left") \
    X(UnclosedParenthesis,   Error,   "missing ')' to close this parenthesis") \
    X(UnexpectedToken,       Error,   "unexpected token") \
    X(UnknownCommand,        Error,   "unknown command") \
    X(EmptyArgument,         Warning, "empty argument") \
    X(TrailingBackslash,     Warning, "string ends with a lone '\\'")

enum class Severity : std::uint8_t { Note, Warning, Error };

enum class DiagnosticCode : std::uint8_t {
#define FIZMO_MATHTEXT_X(name, severity, text) name,
    FIZMO_MATHTEXT_DIAGNOSTICS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
};

inline constexpr const char* diagnostic_name(DiagnosticCode code) noexcept {
    switch (code) {
#define FIZMO_MATHTEXT_X(name, severity, text) case DiagnosticCode::name: return #name;
        FIZMO_MATHTEXT_DIAGNOSTICS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return "Unknown";
}

inline constexpr const char* diagnostic_text(DiagnosticCode code) noexcept {
    switch (code) {
#define FIZMO_MATHTEXT_X(name, severity, text) case DiagnosticCode::name: return text;
        FIZMO_MATHTEXT_DIAGNOSTICS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return "";
}

inline constexpr Severity diagnostic_severity(DiagnosticCode code) noexcept {
    switch (code) {
#define FIZMO_MATHTEXT_X(name, severity, text) case DiagnosticCode::name: return Severity::severity;
        FIZMO_MATHTEXT_DIAGNOSTICS(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return Severity::Error;
}

inline constexpr const char* severity_name(Severity s) noexcept {
    switch (s) {
        case Severity::Note: return "note";
        case Severity::Warning: return "warning";
        case Severity::Error: return "error";
    }
    return "error";
}

struct Diagnostic {
    DiagnosticCode code = DiagnosticCode::UnexpectedCharacter;
    Severity       severity = Severity::Error;
    Span           span;
    std::string    message;
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_DIAGNOSTIC_HPP
