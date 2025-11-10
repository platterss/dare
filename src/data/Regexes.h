#ifndef REGEXES_H
#define REGEXES_H

#include <ctre.hpp>

// Maybe I should have used an actual HTML parser instead...
namespace Regex {
namespace Auth {
inline constexpr auto HIDDEN_INPUTS = ctll::fixed_string(R"#(<input\s+type\s*=\s*["']hidden["'][^>]*?\bname\s*=\s*["']([^"']+)["'][^>]*?\bvalue\s*=\s*["']([^"']+)["'][^>]*?>)#");
}

namespace Enrollment {
inline constexpr auto ENROLLMENT_DATA = ctll::fixed_string(R"(<span class="status-bold">([^<:]+):</span>\s*<span[^>]*>(\d+)</span>)");
}

namespace Scheduler {
inline constexpr auto REGISTRATION_TIME = ctll::fixed_string(R"(\b(\d{2}/\d{2}/\d{4} \d{2}:\d{2} (AM|PM))\b)");
}
} // namespace Regex

#endif // REGEXES_H