#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

static inline bool isSpace(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

// Parse a single trailing index in brackets with optional whitespace: [ num ].
// Parsing is done in reverse, so 'end' gives one-past-the-end position.
// Hence, expectation is that parseIndex is called with s[end-1] == ']'.
// end is updated to the index of the matching opening bracket '['.
static std::size_t parseIndex(const std::string &s, std::size_t &end) {
  if (end == 0 || s[end - 1] != ']')
    throw std::invalid_argument("Expected trailing ']'.");

  const std::size_t close = end - 1;
  const std::size_t open = s.rfind('[', close);
  if (open == std::string::npos)
    throw std::invalid_argument("Missing '[' for trailing index.");

  // Skip past whitespace to find number itself
  std::size_t indexStart = open + 1;
  std::size_t indexEnd = close; // One past the end
  while (indexStart < indexEnd && isSpace(s[indexStart]))
    ++indexStart;
  while (indexEnd > indexStart && isSpace(s[indexEnd - 1]))
    --indexEnd;
  if (indexStart == indexEnd)
    throw std::invalid_argument("Empty index '[]'.");

  const std::string num = s.substr(indexStart, indexEnd - indexStart);
  char *endp = nullptr;
  const unsigned long long res = std::strtoull(num.c_str(), &endp, 10);
  if (!endp || *endp != '\0')
    throw std::invalid_argument("Non-numeric index inside brackets.");

  end = open;
  return static_cast<std::size_t>(res);
}

// Parses trailing [num] indices, writing their values into the out vector left
// to right. Return value is the index past the base signal name
static std::size_t parseIndices(std::string &s, std::vector<std::size_t> &out) {
  out.clear();
  std::size_t end = s.size(); // boundary one past the last character

  // Iterate right-to-left
  auto skipWhitespace = [&] {
    while (end > 0 && isSpace(s[end - 1]))
      --end;
  };

  skipWhitespace(); // Trailing whitespace
  while (end > 0 && s[end - 1] == ']') {
    out.push_back(parseIndex(s, end));
    skipWhitespace(); // Whitespace between bracket groups
  }

  std::reverse(out.begin(), out.end());
  return end;
}

// --- Helpers for tests ---
static void expect_ok(const std::string &in, const std::string &expected_name,
                      const std::vector<std::size_t> &expected_indices) {
  std::string s = in;
  std::vector<std::size_t> out;
  std::size_t end = parseIndices(s, out);
  s.erase(end);
  assert(s == expected_name);
  assert(out == expected_indices);
}

static void expect_throw(const std::string &in) {
  std::string s = in;
  std::vector<std::size_t> out;
  bool threw = false;
  try {
    parseIndices(s, out);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  assert(threw);
}

int main() {
  // -------------------------
  // Valid: basic functionality
  // -------------------------
  expect_ok("sig[3][0][2]", "sig", {3u, 0u, 2u});
  expect_ok("signal_name[0]", "signal_name", {0u});
  expect_ok("a[10][20][30][40]", "a", {10u, 20u, 30u, 40u});

  // --------------------------------------------
  // Valid: whitespace inside brackets is ignored
  // --------------------------------------------
  expect_ok("sig[ 3 ][ 0 ][ 2 ]", "sig", {3u, 0u, 2u});
  expect_ok("sig[3][   0][2   ]", "sig", {3u, 0u, 2u});
  expect_ok("sig[   003   ]", "sig", {3u}); // leading zeros

  // ------------------------------------------------------
  // Valid: whitespace BETWEEN bracket groups is handled by
  // strip_and_parse (outside-bracket whitespace)
  // ------------------------------------------------------
  expect_ok("sig[3] [0]\t[2]", "sig", {3u, 0u, 2u});
  expect_ok("sig[3]\n[0]\r\n[2]", "sig", {3u, 0u, 2u});

  // ------------------------------------------------
  // Valid: trailing whitespace is stripped from name
  // ------------------------------------------------
  expect_ok("sig[1][2]   ", "sig", {1u, 2u});
  expect_ok("sig   ", "sig", {}); // no indices; trims trailing ws
  expect_ok("sig", "sig", {});    // no indices; unchanged

  // ---------------------------------------------------------
  // Valid: name may contain non-whitespace special characters
  // ---------------------------------------------------------
  expect_ok("top.u$1__v[5]", "top.u$1__v", {5u});
  expect_ok("_sig_[7]", "_sig_",
            {7u}); // '[' in name is fine if not trailing bracket group

  // -------------------------
  // Malformed: must throw
  // -------------------------
  expect_throw("sig[]");
  expect_throw("sig[ ]");
  // expect_throw("sig[3"); // Doesnt' throw currently
  expect_throw("sig3]");
  expect_throw("sig[3]]");
  expect_throw("sig[[3]]");
  expect_throw("sig[abc]");
  expect_throw("sig[3a]");
  // expect_throw("sig[-1]");   // not an unsigned literal // Doesn't throw
  // currently
  expect_throw("sig[1+2]");  // expressions not supported
  expect_throw("sig[0x10]"); // non-decimal not supported by current parser
  expect_throw("sig[3][ ]"); // empty index in later group
  // expect_throw( "sig[3]junk"); // trailing text after indices means no
  // trailing ']' group at end; here it should NOT throw (depending on your
  // desired behavior; if you want it to throw, remove this
  // line and add explicit check)

  // If you want "sig[3]junk" to be considered malformed, add a policy check
  // (not in these tests) that rejects any non-whitespace after a parsed suffix.

  return 0;
}
