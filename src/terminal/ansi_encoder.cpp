#include "vulpes/terminal/ansi_encoder.hpp"

#include <string>

namespace vulpes::terminal {
namespace {

void append_color(std::string& result, int base, Color color) {
    result += std::to_string(base);
    result += ";2;";
    result += std::to_string(color.red);
    result += ';';
    result += std::to_string(color.green);
    result += ';';
    result += std::to_string(color.blue);
}

} // namespace

auto ansi_reset() -> std::string {
    return "\x1B[0m";
}

auto encode_ansi(const std::vector<RenderOperation>& operations) -> std::string {
    std::string result;
    for (const auto& operation : operations) {
        switch (operation.kind) {
        case RenderOperationKind::move_cursor:
            result += "\x1B[" + std::to_string(operation.y + 1) + ';' + std::to_string(operation.x + 1) + 'H';
            break;
        case RenderOperationKind::set_style:
            result += "\x1B[";
            append_color(result, 38, operation.style.foreground);
            result += ';';
            append_color(result, 48, operation.style.background);
            if (operation.style.bold)
                result += ";1";
            if (operation.style.underline)
                result += ";4";
            if (operation.style.reverse)
                result += ";7";
            result += 'm';
            break;
        case RenderOperationKind::write:
            result += operation.text;
            break;
        }
    }
    return result;
}

} // namespace vulpes::terminal
