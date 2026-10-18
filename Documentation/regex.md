# Regex, Pattern and CharSet

`#include <regex.hpp>` (or `FIZMO_STRING` / `FIZMO_REGEX` with `fizmo/includes.hpp`) gives three classes in `namespace fizmo`:

| Class | For |
|---|---|
| [`Regex`](#regex) | Classic regular expressions: `\d{4}-\d{2}`, `(?<name>...)`, lookarounds, everything you know from other languages |
| [`Pattern`](#pattern) | The same power written as named steps you can read: `Pattern::digit().repeat(4).then("-")...` |
| [`CharSet`](#charset) | Which characters are allowed: `CharSet::everything().disable_letters().disable_special()`. Text fields use it to block keys |

All three understand UTF-8: `.` is one character, not one byte, `\p{L}` is any letter in any script, and positions in results are byte offsets into the `std::string` you passed in. Any of them can tell you what it does in plain English with `explain()` or `describe()`.

## Regex

```cpp
fizmo::Regex date(R"((?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2}))");

if (auto m = date.search("Due 2026-10-18, paid")) {
    std::string year = m->str("year");     // "2026"
    std::size_t at = m->begin();          // 4
}
date.matches("2026-10-18");                         // true: the whole text fits
date.contains("on 2026-10-18");                     // true: somewhere in the text
date.replace("2026-10-18", "$<day>/$<month>/$1");   // "18/10/2026"
fizmo::Regex(R"(\s*,\s*)").split("a , b,c");        // {"a", "b", "c"}
```

A pattern that doesn't make sense doesn't throw: `ok()` is false, `error()` says what's wrong in plain words (`"this ( is never closed"`) and `error_position()` says where.

| Method | Gives |
|---|---|
| `matches(text)` | `true` if the whole text fits |
| `contains(text)` | `true` if it fits anywhere |
| `match(text)` | the match of the whole text, or nothing |
| `search(text, start = 0)` | the first match at or after `start` |
| `match_at(text, pos)` | a match that starts exactly at `pos` |
| `find_all(text)` | every match, left to right, never overlapping |
| `count(text)` | how many |
| `replace(text, with, limit = all)` | `with` can use `$1`, `$<name>` (or `${name}`), `$&` for the whole match, `` $` `` and `$'` for the text before and after, `$$` for a `$` |
| `replace(text, function)` | the function gets each `RegexMatch` and returns its replacement |
| `split(text, limit = all)` | the pieces between matches; captured groups are kept between them |
| `explain()` | the pattern in plain English |
| `Regex::escape(text)` | `text` with every special character escaped, to match it literally |

A `RegexMatch` has `begin(g)`, `end(g)`, `length(g)`, `text(g)` (a `string_view`), `str(g)`, `matched(g)`, the same by group name, `before()`, `after()` and `group_count()`. Group 0 is the whole match. It points into the text it came from, so keep that text alive while you use it.

### Flags

`Regex(pattern, RegexFlags::ignore_case | RegexFlags::multiline)`, or the same letters inside the pattern: `(?i)` for the rest of it or `(?i:...)` for one part.

| Flag | Letter | Effect |
|---|---|---|
| `ignore_case` | `i` | `a` matches `A`, `é` matches `É` |
| `multiline` | `m` | `^` and `$` match at every line, not just the ends of the text |
| `dot_all` | `s` | `.` matches line breaks too |
| `extended` | `x` | spaces and `# comments` in the pattern are ignored, so it can be laid out over several lines |
| `unicode_classes` | `u` | `\d`, `\w`, `\s` and `\b` understand every script instead of only ASCII |

### Syntax

| Write | Matches |
|---|---|
| `abc` | that text |
| `.` | any character except a line break |
| `[abc]`, `[a-z]`, `[^abc]` | one of those, one in that range, anything but those |
| `[\p{L}--[aeiou]]`, `[a-z&&[^aeiou]]` | letters except vowels: `--` takes away, `&&` keeps what's in both |
| `[[:alpha:]]`, `[[:digit:]]`, ... | POSIX classes |
| `\d`, `\w`, `\s` | a digit, a letter/digit/underscore, whitespace; `\D`, `\W`, `\S` are the opposite |
| `\p{L}`, `\p{Lu}`, `\p{N}`, `\p{P}`, `\p{S}`, `\p{Emoji}`, `\p{ASCII}`, ... | a Unicode category; `\P{...}` is the opposite |
| `\t`, `\n`, `\x41`, `é`, `\u{1F600}` | a tab, a line feed, characters by number |
| `^`, `$`, `\A`, `\z`, `\Z` | start and end of the text (or line, with `m`); `\A` and `\z` always mean the text |
| `\b`, `\B` | a word boundary, and not one |
| `x*`, `x+`, `x?` | zero or more, one or more, zero or one |
| `x{3}`, `x{2,5}`, `x{2,}`, `x{,5}` | counted |
| `x*?`, `x+?`, ... | as few as possible |
| `x*+`, `x++`, ... | as many as possible, never giving any back (fast, and safe from slow backtracking) |
| `a\|b` | either |
| `(x)`, `(?<name>x)` | a numbered group, a named group |
| `(?:x)` | a group that doesn't capture |
| `(?=x)`, `(?!x)` | followed by, not followed by |
| `(?<=x)`, `(?<!x)` | preceded by, not preceded by (any length) |
| `(?>x)` | an atomic group: tried one way only |
| `\1`, `\k<name>` | the same text a group matched |
| `(?#comment)` | nothing |

### Slow patterns

Patterns like `^(a|aa)+$` take exponential time on some texts in every backtracking engine. Fizmo's stops after `step_limit()` steps (20 million unless you call `set_step_limit`) and reports no match, with `gave_up()` true, instead of hanging. `x++` and `(?>...)` avoid the problem in the first place.

### explain()

```cpp
std::cout << fizmo::Regex(R"(^(?<user>[\w.]+)@(\w+\.)+[a-z]{2,}$)").explain();
```

```text
1. the start of the text
2. group 1 ("user"), which is one or more of: a letter, digit or underscore (a-z, A-Z, 0-9, _) or '.'
3. '@'
4. one or more of:
   - group 2, which is:
     - one or more of: a letter, digit or underscore (a-z, A-Z, 0-9, _)
     - '.'
5. at least 2 of: 'a' to 'z'
6. the end of the text
```

## Pattern

`Pattern` builds the same expressions from named pieces, so nothing has to be escaped or remembered. Every `Pattern` is a real regex underneath: `to_regex()` shows it, `compile()` turns it into a `Regex`, and `explain()` describes it.

```cpp
using fizmo::Pattern;

Pattern price = Pattern::text("$")
    .then(Pattern::digits().capture("dollars"))
    .then(Pattern::text(".").then(Pattern::digit().repeat(2).capture("cents")).optional());

price.to_regex();                                     // \$(?<dollars>\d+)(?:\.(?<cents>\d{2}))?
price.matches("$12.99");                              // true
price.compile().search("only $5 today")->str("dollars");   // "5"
```

**Pieces** (static): `text("...")` (matched exactly), `one_of(CharSet)`, `one_of("abc")`, `not_one_of("abc")`, `digit()`, `letter()`, `whitespace()`, `word_character()`, `any_character()`, `any_except_newline()`, `start_of_text()`, `end_of_text()`, `start_of_line()`, `end_of_line()`, `word_boundary()`, `either({a, b, c})`, `sequence({a, b, c})`, `same_as("name")`, `same_as(1)`, and `raw("\\d+")` to mix in classic syntax.

**Ready-made**: `digits(n = 1)`, `integer(allow_sign = true)`, `decimal(allow_sign = true)` (`3`, `-3.5`, `.5`), `identifier()`, `hex_number()` (`0xFF`), `hex_color()` (`#rgb`, `#rrggbb`, ...), `email()`, `word()`.

**Joining**: `a.then(b)`, `a.then("text")`, `a.then_one_of(set)`, `a.or_else(b)`. These change `a` and return it.

**Repeating and wrapping** (each returns a new pattern): `optional()`, `one_or_more()`, `zero_or_more()`, `repeat(n)`, `repeat(min, max)`, `at_least(n)`, `at_most(n)`, then `lazy()` for as few as possible or `possessive()` for no backtracking; `capture()`, `capture("name")`, `group()`, `followed_by(p)`, `not_followed_by(p)`, `preceded_by(p)`, `not_preceded_by(p)`, and `whole()` to require the entire text.

## CharSet

A `CharSet` is a set of characters, built by starting from `everything()` or `nothing()` and allowing or disabling kinds of characters, in order (a later rule wins over an earlier one):

```cpp
using fizmo::CharSet;

CharSet price = CharSet::everything().disable_letters().disable_special().disable_spaces();
price.accepts("3.14");          // true: disable_special() keeps '.' for decimals
price.accepts("3,14");          // false
price.filter("a1b.2 c!");       // "1.2"
price.describe();               // any character except: letters; punctuation and symbols other than '.'; spaces, tabs and line breaks
price.to_regex();               // [^\p{L}[\p{P}\p{S}--[.]]\p{White_Space}]

CharSet name = CharSet::nothing().allow_letters().allow(" '-");
name.accepts("Anne-Marie O'Neil");   // true
name.accepts("R2D2");                // false
```

| Method | |
|---|---|
| `everything()`, `nothing()`, `of("abc")`, `range('a', 'f')`, `kind(CharKind::...)` | starting points |
| `digits()`, `numbers()`, `letters()`, `uppercase()`, `lowercase()`, `spaces()`, `punctuation()`, `symbols()`, `special()`, `emoji()`, `hex_digits()`, `word_characters()`, `ascii()` | ready-made sets |
| `allow_X()` / `disable_X()` for `digits`, `numbers`, `letters`, `uppercase`, `lowercase`, `spaces`, `newlines`, `punctuation`, `symbols`, `special`, `emoji`, `control` | add or take away a kind |
| `disable_special(keep = ".")`, `disable_punctuation(keep)`, `disable_symbols(keep)` | the same, keeping the characters in `keep` |
| `allow("chars")`, `disable("chars")`, `allow_range(a, b)`, `disable_range(a, b)`, `allow(set)`, `disable(set)` | specific characters, ranges, other sets |
| `only_ascii()` | take away everything outside ASCII |
| `contains(c)`, `accepts(text)`, `first_rejected(text)`, `filter(text)`, `inverted()` | using it |
| `describe()`, `to_regex()` | what it is, in English and as a regex class |

The kinds: **digits** are decimal digits in any script (`0`–`9`, `٣`, ...); **numbers** also include `½`, `²` and Roman numerals; **spaces** are all whitespace, including line breaks; **special** is punctuation and symbols together; **emoji** are pictographs and flags (not `#`, `*` or digits, which Unicode also counts as emoji).

### In text fields

`ui::TextField` takes them directly: `set_allowed(set)` lets only those characters be typed or pasted, `set_blocked(set)` refuses those, and `set_pattern(regex)` (or a `Pattern`) is the check the whole text has to pass before it's accepted. `ui::CharFilter::from(set)` turns a `CharSet` into a filter for anything else that takes one.

```cpp
field.set_allowed(CharSet::nothing().allow_digits().allow("-"))
     .set_pattern(Pattern::digit().repeat(3).then("-").then(Pattern::digit().repeat(4)));
```
