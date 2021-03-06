#include "reference_widget.h"
#include "maths.h"
#include "context.h"

using namespace reig::primitive;

namespace reig::reference_widget {
    struct SliderValues {
        float min = 0.0f;
        float max = 0.0f;
        float value = 0.0f;
        int offset = 0;
        int num_values = 0;
    };

    SliderValues prepare_slider_values(float min, float max, float value, float step) {
        float min_end = math::min(min, max);
        float max_end = math::max(min, max);
        return SliderValues{
                min_end, max_end, math::clamp(value, min_end, max_end),
                static_cast<int>((value - min_end) / step),
                static_cast<int>((max_end - min_end) / step + 1)
        };
    };

    void size_slider_cursor(float& coord, float& size, int num_values, int offset) {
        size /= num_values;
        if (size < 1) size = 1;
        coord += offset * size;
    }

    float get_distance_to_slider(float mouse_cursor_coord, float cursor_size, float slider_cursor_coord) {
        auto half_cursor_size = cursor_size / 2;
        slider_cursor_coord += half_cursor_size;
        return mouse_cursor_coord - slider_cursor_coord;
    }

    void progress_slider_value(float mouse_cursor_coord, float cursor_size,
                               float slider_cursor_coord, float step, float& value) {
        float distance = get_distance_to_slider(mouse_cursor_coord, cursor_size, slider_cursor_coord);
        if (math::abs(distance) > cursor_size / 2) {
            value += static_cast<int>(distance / cursor_size) * step;
        }
    }

    enum class SliderOrientation {
        kHorizontal, kVertical
    };

    SliderOrientation calculate_slider_orientation(const Rectangle& rect) {
        return rect.height > rect.width
               ? SliderOrientation::kVertical
               : SliderOrientation::kHorizontal;
    }

    struct SliderModel {
        Rectangle bounding_box;
        Rectangle cursor_bounding_box;
        bool is_hovering_over_area = false;
        bool is_holding_click = false;
        bool has_value_changed = false;
    };

    SliderModel tweak_slider_model(Context& ctx, Rectangle& outline_area, const SliderValues& values,
                                   float& value, Rectangle base_area, Rectangle cursor_area) {
        bool is_hovering_over_area = ctx.mouse.is_hovering_over_rect(outline_area);
        bool is_holding_click_on_slider = ctx.mouse.left_button.clicked_in_rect(outline_area)
                                          && ctx.mouse.left_button.is_held();

        if (is_holding_click_on_slider) {
            base_area = decrease_rect(base_area, 2);
            cursor_area = decrease_rect(cursor_area, 4);
        }

        bool has_value_changed = false;
        if (value != values.value) {
            value = math::clamp(values.value, values.min, values.max);
            has_value_changed = true;
        }

        return {base_area, cursor_area, is_hovering_over_area, is_holding_click_on_slider, has_value_changed};
    }

    SliderModel
    get_slider_model(Context& ctx, Rectangle& bounding_box, float& value, float min, float max, float step) {
        ctx.fit_rect_in_window(bounding_box);
        Rectangle base_area = decrease_rect(bounding_box, 4);

        auto values = prepare_slider_values(min, max, value, step);

        SliderOrientation orientation = calculate_slider_orientation(base_area);

        auto cursor_area = decrease_rect(base_area, 4);
        if (orientation == SliderOrientation::kHorizontal) {
            size_slider_cursor(cursor_area.x, cursor_area.width, values.num_values, values.offset);
        } else {
            size_slider_cursor(cursor_area.y, cursor_area.height, values.num_values, values.offset);
        }

        bool is_hovering_over_area = ctx.mouse.is_hovering_over_rect(bounding_box);
        bool is_holding_click_on_slider = ctx.mouse.left_button.clicked_in_rect(bounding_box)
                                          && ctx.mouse.left_button.is_held();

        if (is_holding_click_on_slider) {
            if (orientation == SliderOrientation::kHorizontal) {
                progress_slider_value(ctx.mouse.get_cursor_pos().x, cursor_area.width, cursor_area.x,
                                      step, values.value);
            } else {
                progress_slider_value(ctx.mouse.get_cursor_pos().y, cursor_area.height, cursor_area.y,
                                      step, values.value);
            }
        } else if (ctx.mouse.get_scrolled() != 0 && is_hovering_over_area) {
            values.value += static_cast<int>(ctx.mouse.get_scrolled()) * step;
        }
        return tweak_slider_model(ctx, bounding_box, values, value, base_area, cursor_area);
    }

    void size_scrollbar_cursor(float& coord, float& size, float step, int offset, float viewSize) {
        float scale = size / viewSize;
        if (scale <= 1.0f) {
            coord += offset * step * scale;
            size = math::max(1.0f, scale * size);
            size = math::min(size, viewSize);
        }
    }

    SliderValues prepare_scrollbar_values(float max_scroll, float value, float step) {
        float min = 0.0f;
        float max = math::max(0.f, max_scroll);
        float clamped_value = math::clamp(value, min, max);
        return SliderValues{
                min, max, clamped_value,
                static_cast<int>((value - min) / step),
                static_cast<int>((max - min) / step + 1)
        };
    }

    void progress_scrollbar_value(float mouse_cursor_coord, float cursor_size,
                                  float slider_cursor_coord, float step, float& value) {
        float distance = get_distance_to_slider(mouse_cursor_coord, cursor_size, slider_cursor_coord);
        if (math::abs(distance) > cursor_size / 2) {
            value += static_cast<int>(distance * step) / cursor_size;
        }
    }

    SliderModel get_scrollbar_model(Context& ctx, Rectangle& bounding_box, float view_size, float& value) {
        ctx.fit_rect_in_window(bounding_box);
        Rectangle base_area = decrease_rect(bounding_box, 4);

        auto step = ctx.get_font_size();

        SliderOrientation orientation = calculate_slider_orientation(base_area);
        SliderValues values;
        if (orientation == SliderOrientation::kVertical) {
            values = prepare_scrollbar_values(view_size - base_area.height, value, step);
        } else {
            values = prepare_scrollbar_values(view_size - base_area.width, value, step);
        }

        auto cursor_area = decrease_rect(base_area, 4);
        if (orientation == SliderOrientation::kHorizontal) {
            size_scrollbar_cursor(cursor_area.x, cursor_area.width, step, values.offset, view_size);
        } else {
            size_scrollbar_cursor(cursor_area.y, cursor_area.height, step, values.offset, view_size);
        }

        bool is_hovering_over_area = ctx.mouse.is_hovering_over_rect(bounding_box);
        bool is_holding_click_on_slider = ctx.mouse.left_button.clicked_in_rect(bounding_box)
                                          && ctx.mouse.left_button.is_held();

        if (is_holding_click_on_slider) {
            if (orientation == SliderOrientation::kHorizontal) {
                progress_scrollbar_value(ctx.mouse.get_cursor_pos().x, cursor_area.width, cursor_area.x,
                                         step, values.value);
            } else {
                progress_scrollbar_value(ctx.mouse.get_cursor_pos().y, cursor_area.height, cursor_area.y,
                                         step, values.value);
            }
        } else if (ctx.mouse.get_scrolled() != 0 && is_hovering_over_area) {
            values.value += static_cast<int>(ctx.mouse.get_scrolled()) * step;
        }
        return tweak_slider_model(ctx, bounding_box, values, value, base_area, cursor_area);
    }

    void draw_slider_model(Context& ctx, const SliderModel& model, const Rectangle& bounding_box,
                           const Color& base_color) {
        Color frame_color = colors::get_yiq_contrast(base_color);
        ctx.render_rectangle(bounding_box, frame_color);
        ctx.render_rectangle(model.bounding_box, base_color);

        if (model.is_hovering_over_area) {
            frame_color = colors::lighten_color_by(frame_color, 30);
        }
        if (model.is_holding_click) {
            frame_color = colors::lighten_color_by(frame_color, 30);
        }
        ctx.render_rectangle(model.cursor_bounding_box, frame_color);
    }

    bool slider(Context& ctx, Rectangle bounding_box, Color base_color,
                float& value, float min, float max, float step) {
        auto model = get_slider_model(ctx, bounding_box, value, min, max, step);

        draw_slider_model(ctx, model, bounding_box, base_color);

        return model.has_value_changed;
    }

    bool scrollbar(Context& ctx, Rectangle bounding_box, Color base_color,
                   float& value, float view_size) {
        auto model = get_scrollbar_model(ctx, bounding_box, view_size, value);

        draw_slider_model(ctx, model, bounding_box, base_color);

        return model.has_value_changed;
    }

    bool textured_slider(Context& ctx, Rectangle bounding_box, int base_texture, int cursor_texture,
                         float& value, float min, float max, float step) {
        auto model = get_slider_model(ctx, bounding_box, value, min, max, step);

        ctx.render_rectangle(bounding_box, base_texture);
        ctx.render_rectangle(model.cursor_bounding_box, cursor_texture);

        return model.has_value_changed;
    }
}
