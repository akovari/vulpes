#include "pdf_export.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/report/pdf_font.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <pdfio-content.h>
#include <pdfio.h>
#include <string>
#include <string_view>
#include <utf8proc.h>
#include <utility>
#include <vector>

namespace vulpes::report::detail {
namespace {

constexpr double page_width = 842.0;
constexpr double page_height = 595.0;
constexpr double margin = 36.0;
constexpr double table_width = page_width - (2.0 * margin);
constexpr double title_size = 15.0;
constexpr double header_size = 8.5;
constexpr double body_size = 8.0;
constexpr double header_height = 19.0;
constexpr double row_height = 17.0;
constexpr double table_top = page_height - margin - 29.0;
constexpr double minimum_column_width = 52.0;
constexpr double maximum_column_width = 180.0;
constexpr std::size_t measure_sample_rows = 100;

auto path_text(const std::filesystem::path& path) -> std::string {
    const auto encoded = path.generic_u8string();
    return {encoded.begin(), encoded.end()};
}

struct OutputState {
    std::ofstream* output{};
    std::string last_error;
};

struct PdfColumn {
    std::size_t source_index{};
    double width{};
};

auto output_callback(void* context, const void* data, size_t length) -> ssize_t {
    auto& state = *static_cast<OutputState*>(context);
    if (length > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()) ||
        length > static_cast<size_t>(std::numeric_limits<ssize_t>::max()))
        return -1;

    state.output->write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
    return *state.output ? static_cast<ssize_t>(length) : -1;
}

auto error_callback(pdfio_file_t*, const char* message, void* context) -> bool {
    auto& state = *static_cast<OutputState*>(context);
    if (message != nullptr)
        state.last_error = message;
    return true;
}

[[noreturn]] void throw_pdf_error(std::string_view operation, const OutputState& state) {
    auto description = "cannot write PDF while " + std::string{operation};
    if (!state.last_error.empty())
        description += ": " + state.last_error;
    throw Error{ErrorCategory::io, std::move(description)};
}

void require_pdf(bool successful, std::string_view operation, const OutputState& state) {
    if (!successful)
        throw_pdf_error(operation, state);
}

auto measure_text(pdfio_obj_t* font, std::string_view text, double size, const OutputState& state) -> double {
    const auto value = pdfioContentTextMeasure(font, std::string{text}.c_str(), size);
    if (value < 0.0)
        throw_pdf_error("measuring text", state);
    return value;
}

auto fit_text(pdfio_obj_t* font, std::string_view value, double size, double width, const OutputState& state)
    -> std::string {
    if (measure_text(font, value, size, state) <= width)
        return std::string{value};

    constexpr std::string_view ellipsis{"\xE2\x80\xA6"};
    std::string result;
    const auto* cursor = reinterpret_cast<const utf8proc_uint8_t*>(value.data());
    auto remaining = static_cast<utf8proc_ssize_t>(value.size());
    while (remaining > 0) {
        utf8proc_int32_t code_point{};
        const auto consumed = utf8proc_iterate(cursor, remaining, &code_point);
        if (consumed <= 0)
            break;
        const auto candidate =
            result + std::string{reinterpret_cast<const char*>(cursor), static_cast<std::size_t>(consumed)};
        if (measure_text(font, candidate + std::string{ellipsis}, size, state) > width)
            break;
        result = candidate;
        cursor += consumed;
        remaining -= consumed;
    }
    return result.empty() ? std::string{ellipsis} : result + std::string{ellipsis};
}

auto column_bands(const db::SqlResult& result, pdfio_obj_t* font, const core::LocaleFormatter& formatter,
                  const OutputState& state) -> std::vector<std::vector<PdfColumn>> {
    std::vector<std::vector<PdfColumn>> bands;
    std::vector<PdfColumn> current;
    double used_width{};

    for (std::size_t column_index = 0; column_index < result.columns.size(); ++column_index) {
        auto width = measure_text(font, result.columns.at(column_index), header_size, state) + 12.0;
        const auto sample_rows = std::min(result.rows.size(), measure_sample_rows);
        for (std::size_t row_index = 0; row_index < sample_rows; ++row_index) {
            const auto value = display_text(result.rows.at(row_index).at(column_index), formatter);
            width = std::max(width, measure_text(font, value, body_size, state) + 12.0);
        }
        width = std::clamp(width, minimum_column_width, maximum_column_width);

        if (!current.empty() && used_width + width > table_width) {
            bands.push_back(std::move(current));
            current = {};
            used_width = 0.0;
        }
        current.push_back({.source_index = column_index, .width = width});
        used_width += width;
    }
    if (!current.empty())
        bands.push_back(std::move(current));
    return bands;
}

void draw_text(pdfio_stream_t* stream, std::string_view text, double x, double y, double size,
               const OutputState& state) {
    require_pdf(pdfioContentTextBegin(stream), "starting text", state);
    require_pdf(pdfioContentSetTextFont(stream, "F1", size), "selecting the embedded font", state);
    require_pdf(pdfioContentTextMoveTo(stream, x, y), "positioning text", state);
    const auto owned_text = std::string{text};
    require_pdf(pdfioContentTextShow(stream, true, owned_text.c_str()), "drawing text", state);
    require_pdf(pdfioContentTextEnd(stream), "ending text", state);
}

void draw_page(pdfio_file_t* document, pdfio_obj_t* font, const db::SqlResult& result,
               const std::vector<PdfColumn>& columns, std::size_t first_row, std::size_t rows_on_page,
               std::string_view title, std::size_t page_number, const core::LocaleFormatter& formatter,
               const OutputState& state) {
    auto* page_dictionary = pdfioDictCreate(document);
    if (page_dictionary == nullptr)
        throw_pdf_error("creating a page", state);
    require_pdf(pdfioPageDictAddFont(page_dictionary, "F1", font), "adding the embedded font to a page", state);

    auto* stream = pdfioFileCreatePage(document, page_dictionary);
    if (stream == nullptr)
        throw_pdf_error("creating a page stream", state);

    const auto table_height = header_height + (static_cast<double>(rows_on_page) * row_height);
    const auto table_bottom = table_top - table_height;
    require_pdf(pdfioContentSetFillColorDeviceGray(stream, 0.0), "setting text colour", state);
    draw_text(stream, fit_text(font, title, title_size, table_width - 80.0, state), margin, page_height - margin - 4.0,
              title_size, state);
    const auto page_label = "Page " + std::to_string(page_number);
    const auto page_label_width = measure_text(font, page_label, body_size, state);
    draw_text(stream, page_label, page_width - margin - page_label_width, page_height - margin - 4.0, body_size, state);

    require_pdf(pdfioContentSetFillColorDeviceGray(stream, 0.91), "setting header background", state);
    require_pdf(pdfioContentPathRect(stream, margin, table_top - header_height, table_width, header_height),
                "drawing a table header", state);
    require_pdf(pdfioContentFill(stream, false), "filling a table header", state);

    require_pdf(pdfioContentSetFillColorDeviceGray(stream, 0.0), "setting text colour", state);
    double x = margin;
    for (const auto& column : columns) {
        const auto label =
            fit_text(font, result.columns.at(column.source_index), header_size, column.width - 8.0, state);
        draw_text(stream, label, x + 4.0, table_top - 13.0, header_size, state);
        x += column.width;
    }

    for (std::size_t row_offset = 0; row_offset < rows_on_page; ++row_offset) {
        const auto& row = result.rows.at(first_row + row_offset);
        const auto baseline = table_top - header_height - 12.0 - (static_cast<double>(row_offset) * row_height);
        x = margin;
        for (const auto& column : columns) {
            const auto value = display_text(row.at(column.source_index), formatter);
            draw_text(stream, fit_text(font, value, body_size, column.width - 8.0, state), x + 4.0, baseline, body_size,
                      state);
            x += column.width;
        }
    }

    require_pdf(pdfioContentSetStrokeColorDeviceGray(stream, 0.55), "setting table border colour", state);
    require_pdf(pdfioContentSetLineWidth(stream, 0.35), "setting table border width", state);
    require_pdf(pdfioContentPathRect(stream, margin, table_bottom, table_width, table_height), "drawing a table border",
                state);
    x = margin;
    for (const auto& column : columns) {
        x += column.width;
        if (x >= margin + table_width)
            continue;
        require_pdf(pdfioContentPathMoveTo(stream, x, table_bottom), "drawing a column border", state);
        require_pdf(pdfioContentPathLineTo(stream, x, table_top), "drawing a column border", state);
    }
    for (std::size_t row_offset = 0; row_offset <= rows_on_page; ++row_offset) {
        const auto y = table_top - header_height - (static_cast<double>(row_offset) * row_height);
        require_pdf(pdfioContentPathMoveTo(stream, margin, y), "drawing a row border", state);
        require_pdf(pdfioContentPathLineTo(stream, margin + table_width, y), "drawing a row border", state);
    }
    require_pdf(pdfioContentStroke(stream), "drawing table borders", state);
    require_pdf(pdfioStreamClose(stream), "closing a page", state);
}

} // namespace

void write_pdf(const std::filesystem::path& path, const db::SqlResult& result, std::string_view title,
               const core::LocaleFormatter& formatter) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
        throw Error{ErrorCategory::io, "cannot create PDF export file: " + path_text(path)};

    OutputState state{.output = &output};
    pdfio_rect_t media_box{.x1 = 0.0, .y1 = 0.0, .x2 = page_width, .y2 = page_height};
    auto* document =
        pdfioFileCreateOutput(output_callback, &state, "1.7", &media_box, &media_box, error_callback, &state);
    if (document == nullptr)
        throw_pdf_error("creating the document", state);

    try {
        const auto owned_title = std::string{title};
        pdfioFileSetCreator(document, "Vulpes");
        pdfioFileSetTitle(document, owned_title.c_str());

        const auto font_data = default_pdf_font();
        auto* font = pdfioFileCreateFontObjFromData(document, font_data.data(), font_data.size(), true);
        if (font == nullptr)
            throw_pdf_error("embedding the Unicode font", state);

        const auto bands = column_bands(result, font, formatter, state);
        const auto rows_per_page = std::max<std::size_t>(
            1, static_cast<std::size_t>(std::floor((table_top - header_height - margin) / row_height)));
        std::size_t page_number = 1;
        for (const auto& band : bands) {
            if (result.rows.empty()) {
                draw_page(document, font, result, band, 0, 0, title, page_number++, formatter, state);
                continue;
            }
            for (std::size_t first_row = 0; first_row < result.rows.size(); first_row += rows_per_page) {
                const auto rows_on_page = std::min(rows_per_page, result.rows.size() - first_row);
                draw_page(document, font, result, band, first_row, rows_on_page, title, page_number++, formatter,
                          state);
            }
        }

        const auto closed = pdfioFileClose(document);
        document = nullptr;
        output.close();
        if (!closed || !output)
            throw_pdf_error("finalizing the document", state);
    } catch (...) {
        if (document != nullptr)
            static_cast<void>(pdfioFileClose(document));
        throw;
    }
}

} // namespace vulpes::report::detail
