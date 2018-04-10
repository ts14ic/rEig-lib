#include "reig.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC

#include "stb_truetype.h"
#include <sstream>
#include <memory>

using std::unique_ptr;
using std::vector;

namespace internal {
    using namespace reig;
    using namespace primitive;

    template<typename T>
    bool is_between(T val, T min, T max) {
        return val > min && val < max;
    }

    bool is_boxed_in(Point const& pt, Rectangle const& box) {
        return is_between(pt.x, box.x, box.x + box.width) && is_between(pt.y, box.y, box.y + box.height);
    }

    template<typename T>
    T clamp(T val, T min, T max) {
        return val < min ? min :
               val > max ? max :
               val;
    }

    template<typename T>
    T min(T a, T b) {
        return a < b ? a : b;
    }

    template<typename T>
    T max(T a, T b) {
        return a > b ? a : b;
    }

    template<typename T>
    T abs(T a) {
        return a < 0 ? -a : a;
    }

    template<typename T>
    int sign(T a) {
        return a < 0 ? -1 :
               a > 0 ? 1 :
               0;
    }

    primitive::Color get_yiq_contrast(primitive::Color color) {
        using namespace primitive::colors::literals;

        uint32_t y = (299u * color.red.val + 587 * color.green.val + 114 * color.blue.val) / 1000;
        return y >= 128
               ? Color{0_r, 0_g, 0_b, 255_a}
               : Color{255_r, 255_g, 255_b};
    }

    Color lighten_color_by(Color color, uint8_t delta) {
        uint8_t max = 255u;
        color.red.val < max - delta ? color.red.val += delta : color.red.val = max;
        color.green.val < max - delta ? color.green.val += delta : color.green.val = max;
        color.blue.val < max - delta ? color.blue.val += delta : color.blue.val = max;
        return color;
    }

    Rectangle decrease_box(Rectangle aRect, int by) {
        int moveBy = by / 2;
        aRect.x += moveBy;
        aRect.y += moveBy;
        aRect.width -= by;
        aRect.height -= by;
        return aRect;
    }

    template <typename R, typename T, typename = std::enable_if_t<std::is_integral_v<R> && std::is_integral_v<R>>>
    R integral_cast(T t) {
        auto r = static_cast<R>(t);
        if (r != t || (std::is_signed_v<T> != std::is_signed_v<R> && ((t < T{}) != r < R{}))) {
            std::stringstream ss;
            ss << "Bad integral cast from " << typeid(T).name() << "(" << t << ") to type " << typeid(R).name();
            throw std::range_error(ss.str());
        }
        return r;
    };
}

using namespace reig::primitive;

reig::uint32_t colors::to_uint(Color const& color) {
    return (color.alpha.val << 24)
           + (color.blue.val << 16)
           + (color.green.val << 8)
           + color.red.val;
}

Color colors::from_uint(uint32_t rgba) {
    return Color {
            Color::Red{static_cast<uint8_t>((rgba >> 24) & 0xFF)},
            Color::Green{static_cast<uint8_t>((rgba >> 16) & 0xFF)},
            Color::Blue{static_cast<uint8_t>((rgba >> 8) & 0xFF)},
            Color::Alpha{static_cast<uint8_t>(rgba & 0xFF)}
    };
}

void reig::Context::set_render_handler(RenderHandler renderHandler) {
    mRenderHandler = renderHandler;
}

void reig::Context::set_user_ptr(std::any ptr) {
    mUserPtr = std::move(ptr);
}

std::any const& reig::Context::get_user_ptr() const {
    return mUserPtr;
}

reig::exception::FailedToLoadFontException::FailedToLoadFontException(std::string message)
        : message{std::move(message)} {}

const char* reig::exception::FailedToLoadFontException::what() const noexcept {
    return message.c_str();
}

reig::exception::FailedToLoadFontException reig::exception::FailedToLoadFontException::noTextureId(const char* filePath) {
    std::ostringstream ss;
    ss << "No texture id was specified for font: [" << filePath << "]";
    return FailedToLoadFontException(ss.str());
}

reig::exception::FailedToLoadFontException reig::exception::FailedToLoadFontException::invalidSize(const char* filePath, float fontSize) {
    std::ostringstream ss;
    ss << "Invalid size specified for font: [" << filePath << "], size: [" << fontSize << "]";
    return FailedToLoadFontException(ss.str());
}

reig::exception::FailedToLoadFontException reig::exception::FailedToLoadFontException::couldNotOpenFile(const char* filePath) {
    std::ostringstream ss;
    ss << "Could not open font file: [" << filePath << "]";
    return FailedToLoadFontException(ss.str());
}

reig::exception::FailedToLoadFontException reig::exception::FailedToLoadFontException::couldNotFitCharacters(
        const char* filePath, float fontSize, int width, int height) {
    std::ostringstream ss;
    ss << "Could not fit characters for font: ["
       << filePath << "], size: [" << fontSize << "], atlas size: ["
       << width << "x" << height << "]";
    return FailedToLoadFontException(ss.str());
}

reig::exception::FailedToLoadFontException reig::exception::FailedToLoadFontException::invalidFile(const char* filePath) {
    std::ostringstream ss;
    ss << "Invalid file for font: [" << filePath << "]";
    return FailedToLoadFontException(ss.str());
}

vector<reig::uint8_t> read_font_into_buffer(char const* fontFilePath) {
    using reig::exception::FailedToLoadFontException;

    auto file = std::unique_ptr<FILE, decltype(&std::fclose)>(std::fopen(fontFilePath, "rb"), &std::fclose);
    if (!file) throw FailedToLoadFontException::couldNotOpenFile(fontFilePath);

    std::fseek(file.get(), 0, SEEK_END);
    long filePos = ftell(file.get());
    if (filePos < 0) throw FailedToLoadFontException::invalidFile(fontFilePath);

    auto fileSize = internal::integral_cast<size_t>(filePos);
    std::rewind(file.get());

    auto ttfBuffer = std::vector<unsigned char>(fileSize);
    std::fread(ttfBuffer.data(), 1, fileSize, file.get());
    return ttfBuffer;
}

reig::Context::FontData reig::Context::set_font(char const* fontFilePath, int textureId, float fontHeightPx) {
    using exception::FailedToLoadFontException;

    if (textureId == 0) throw FailedToLoadFontException::noTextureId(fontFilePath);
    if (fontHeightPx <= 0) throw FailedToLoadFontException::invalidSize(fontFilePath, fontHeightPx);

    auto ttfBuffer = read_font_into_buffer(fontFilePath);

    // We want all ASCII chars from space to square
    int const charsNum = 96;
    int bitmapWidth = 512;
    int bitmapHeight = 512;

    auto bakedChars = std::vector<stbtt_bakedchar>(charsNum);
    auto bitmap = vector<uint8_t>(internal::integral_cast<size_t>(bitmapWidth * bitmapHeight));
    auto height = stbtt_BakeFontBitmap(
            ttfBuffer.data(), 0, fontHeightPx, bitmap.data(), bitmapWidth, bitmapHeight, ' ', charsNum, std::data(bakedChars)
    );

    if (height < 0 || height > bitmapHeight) {
        throw FailedToLoadFontException::couldNotFitCharacters(fontFilePath, fontHeightPx, bitmapWidth, bitmapHeight);
    }

    // If all successfull, replace current font data
    mFont.bakedChars = std::move(bakedChars);
    mFont.texture = textureId;
    mFont.width = bitmapWidth;
    mFont.height = height;
    mFont.size = fontHeightPx;

    // Return texture creation info
    FontData ret;
    ret.bitmap = bitmap;
    ret.width = bitmapWidth;
    ret.height = height;

    return ret;
}

float reig::Context::get_font_height() const {
    return mFont.height;
}

const char* reig::exception::NoRenderHandlerException::what() const noexcept {
    return "No render handler specified";
}

void reig::Context::render_all() {
    if (!mRenderHandler) {
        throw exception::NoRenderHandlerException{};
    }
    if (mCurrentWindow.started) end_window();

    if (!mCurrentWindow.drawData.empty()) {
        mRenderHandler(mCurrentWindow.drawData, mUserPtr);
    }
    if (!mDrawData.empty()) {
        mRenderHandler(mDrawData, mUserPtr);
    }
}

void reig::Context::start_new_frame() {
    mCurrentWindow.drawData.clear();
    mDrawData.clear();

    mouse.leftButton.mIsClicked = false;
    mouse.mScrolled = 0.f;
}

void reig::detail::Mouse::move(float difx, float dify) {
    mCursorPos.x += difx;
    mCursorPos.y += dify;
}

void reig::detail::Mouse::place(float x, float y) {
    mCursorPos.x = x;
    mCursorPos.y = y;
}

void reig::detail::Mouse::scroll(float dy) {
    mScrolled = dy;
}

const Point& reig::detail::Mouse::get_cursor_pos() const {
    return mCursorPos;
}

float reig::detail::Mouse::get_scrolled() const {
    return mScrolled;
}

void reig::detail::MouseButton::press(float x, float y) {
    if (!mIsPressed) {
        mIsPressed = true;
        mIsClicked = true;
        mClickedPos = {x, y};
    }
}

void reig::detail::MouseButton::release() {
    mIsPressed = false;
}

const Point& reig::detail::MouseButton::get_clicked_pos() const {
    return mClickedPos;
}

bool reig::detail::MouseButton::is_pressed() const {
    return mIsPressed;
}

bool reig::detail::MouseButton::is_clicked() const {
    return mIsClicked;
}

void Figure::form(vector<Vertex>& vertices, vector<int>& indices, int id) {
    vertices.swap(mVertices);
    indices.swap(mIndices);
    mTextureId = id;
}

vector<Vertex> const& Figure::vertices() const {
    return mVertices;
}

vector<int> const& Figure::indices() const {
    return mIndices;
}

int Figure::texture() const {
    return mTextureId;
}

void reig::Context::start_window(char const* aTitle, float& aX, float& aY) {
    if (mCurrentWindow.started) end_window();

    mCurrentWindow.started = true;
    mCurrentWindow.title = aTitle;
    mCurrentWindow.x = &aX;
    mCurrentWindow.y = &aY;
    mCurrentWindow.w = 0;
    mCurrentWindow.h = 0;
    mCurrentWindow.headerSize = 8 + mFont.size;
}

void reig::Context::end_window() {
    if (!mCurrentWindow.started) return;
    mCurrentWindow.started = false;

    mCurrentWindow.w += 4;
    mCurrentWindow.h += 4;

    Rectangle headerBox{
            *mCurrentWindow.x, *mCurrentWindow.y,
            mCurrentWindow.w, mCurrentWindow.headerSize
    };
    Triangle headerTriangle{
            *mCurrentWindow.x + 3.f, *mCurrentWindow.y + 3.f,
            *mCurrentWindow.x + 3.f + mCurrentWindow.headerSize, *mCurrentWindow.y + 3.f,
            *mCurrentWindow.x + 3.f + mCurrentWindow.headerSize / 2.f,
            *mCurrentWindow.y + mCurrentWindow.headerSize - 3.f
    };
    Rectangle titleBox{
            *mCurrentWindow.x + mCurrentWindow.headerSize + 4, *mCurrentWindow.y + 4,
            mCurrentWindow.w - mCurrentWindow.headerSize - 4, mCurrentWindow.headerSize - 4
    };
    Rectangle bodyBox{
            *mCurrentWindow.x, *mCurrentWindow.y + mCurrentWindow.headerSize,
            mCurrentWindow.w, mCurrentWindow.h - mCurrentWindow.headerSize
    };

    using namespace colors::literals;
    using namespace colors::operators;

    render_rectangle(headerBox, colors::mediumGrey | 200_a);
    render_triangle(headerTriangle, colors::lightGrey);
    render_text(mCurrentWindow.title, titleBox);
    render_rectangle(bodyBox, colors::mediumGrey | 100_a);

    if (mouse.leftButton.is_pressed() && internal::is_boxed_in(mouse.leftButton.get_clicked_pos(), headerBox)) {
        Point moved{
                mouse.get_cursor_pos().x - mouse.leftButton.get_clicked_pos().x,
                mouse.get_cursor_pos().y - mouse.leftButton.get_clicked_pos().y
        };

        *mCurrentWindow.x += moved.x;
        *mCurrentWindow.y += moved.y;
        mouse.leftButton.mClickedPos.x += moved.x;
        mouse.leftButton.mClickedPos.y += moved.y;
    }
}

void reig::detail::Window::expand(Rectangle& aBox) {
    if (started) {
        aBox.x += *x + 4;
        aBox.y += *y + headerSize + 4;

        if (*x + w < aBox.x + aBox.width) {
            w = aBox.x + aBox.width - *x;
        }
        if (*y + h < aBox.y + aBox.height) {
            h = aBox.y + aBox.height - *y;
        }
        if (aBox.x < *x) {
            auto d = *x - aBox.x;
            aBox.x += d + 4;
        }
        if (aBox.y < *y) {
            auto d = *y - aBox.y;
            aBox.y += d + 4;
        }
    }
}

void reig::Context::fit_rect_in_window(Rectangle& rect) {
    mCurrentWindow.expand(rect);
}

bool reig::reference_widget::button::draw(reig::Context& ctx) const {
    Rectangle box = this->mBoundingBox;
    ctx.fit_rect_in_window(box);

    // Render button outline first
    Color outlineCol = internal::get_yiq_contrast(mBaseColor);
    ctx.render_rectangle(box, outlineCol);

    Color color = this->mBaseColor;
    // if cursor is over the button, highlight it
    if (internal::is_boxed_in(ctx.mouse.get_cursor_pos(), box)) {
        color = internal::lighten_color_by(color, 50);
    }

    // see, if clicked the inner part of button
    box = internal::decrease_box(box, 4);
    bool clickedInBox = internal::is_boxed_in(ctx.mouse.leftButton.get_clicked_pos(), box);

    // highlight even more, if clicked
    if (ctx.mouse.leftButton.is_pressed() && clickedInBox) {
        color = internal::lighten_color_by(color, 50);
    }

    // render the inner part of button
    ctx.render_rectangle(box, color);
    // render button's title
    ctx.render_text(mTitle, box);

    return ctx.mouse.leftButton.is_clicked() && clickedInBox;
}


bool reig::reference_widget::textured_button::draw(reig::Context& ctx) const {
    Rectangle box = this->mBoundingBox;
    ctx.fit_rect_in_window(box);

    bool clickedInBox = internal::is_boxed_in(ctx.mouse.leftButton.get_clicked_pos(), box);
    bool hoveredOnBox = internal::is_boxed_in(ctx.mouse.get_cursor_pos(), box);

    if((ctx.mouse.leftButton.is_pressed() && clickedInBox) || hoveredOnBox) {
        ctx.render_rectangle(box, mBaseTexture);
    } else {
        ctx.render_rectangle(box, mHoverTexture);
    }

    box = internal::decrease_box(box, 8);
    ctx.render_text(mTitle, box);

    return ctx.mouse.leftButton.is_clicked() && clickedInBox;
}

void reig::reference_widget::label::draw(reig::Context& ctx) const {
    Rectangle boundingBox = this->mBoundingBox;
    ctx.fit_rect_in_window(boundingBox);
    ctx.render_text(mTitle, boundingBox);
}

enum class SliderOrientation {
    HORIZONTAL, VERTICAL
};

void render_widget_frame(reig::Context& ctx, Rectangle& boundingBox, Color const& baseColor) {
    Color frameColor = internal::get_yiq_contrast(baseColor);
    ctx.render_rectangle(boundingBox, frameColor);
    boundingBox = internal::decrease_box(boundingBox, 4);
    ctx.render_rectangle(boundingBox, baseColor);
}

struct SliderValues {
    float min = 0.0f;
    float max = 0.0f;
    float value = 0.0f;
    int offset = 0;
    int valuesNum = 0;
};

SliderValues prepare_slider_values(float aMin, float aMax, float aValue, float aStep) {
    float min = internal::min(aMin, aMax);
    float max = internal::max(aMin, aMax);
    return SliderValues{
            min, max, internal::clamp(aValue, min, max),
            static_cast<int>((aValue - min) / aStep),
            static_cast<int>((max - min) / aStep + 1)
    };
};

void resize_slider_cursor(float& coord, float& size, int valuesNum, int offset) {
    size /= valuesNum;
    if(size < 1) size = 1;
    coord += offset * size;
}

void progress_slider_value(float mouseCursorCoord, float cursorSize, float cursorCoord,
                           float min, float max, float step, float& value) {
    auto halfCursorW = cursorSize / 2;
    auto distanceToMouseCoord = mouseCursorCoord - cursorCoord - halfCursorW;

    if (internal::abs(distanceToMouseCoord) > halfCursorW) {
        value += static_cast<int>(distanceToMouseCoord / cursorSize) * step;
        value = internal::clamp(value, min, max);
    }
}

bool base_slider_draw(reig::Context& ctx,
                      Rectangle boundingBox,
                      SliderOrientation orientation,
                      Color const& baseColor,
                      float& valueRef,
                      float aMin, float aMax, float step) {
    ctx.fit_rect_in_window(boundingBox);

    render_widget_frame(ctx, boundingBox, baseColor);

    auto [min, max, value, offset, valuesNum] = prepare_slider_values(aMin, aMax, valueRef, step);

    // Render the cursor
    auto cursorBox = internal::decrease_box(boundingBox, 4);
    if(orientation == SliderOrientation::HORIZONTAL) {
        resize_slider_cursor(cursorBox.x, cursorBox.width, valuesNum, offset);
    } else {
        resize_slider_cursor(cursorBox.y, cursorBox.height, valuesNum, offset);
    }

    auto cursorColor = internal::get_yiq_contrast(baseColor);
    if (internal::is_boxed_in(ctx.mouse.get_cursor_pos(), cursorBox)) {
        cursorColor = internal::lighten_color_by(cursorColor, 50);
    }
    ctx.render_rectangle(cursorBox, cursorColor);

    if (ctx.mouse.leftButton.is_pressed() && internal::is_boxed_in(ctx.mouse.leftButton.get_clicked_pos(), boundingBox)) {
        if(orientation == SliderOrientation::HORIZONTAL) {
            progress_slider_value(ctx.mouse.get_cursor_pos().x, cursorBox.width, cursorBox.x, min, max, step, value);
        } else {
            progress_slider_value(ctx.mouse.get_cursor_pos().y, cursorBox.height, cursorBox.y, min, max, step, value);
        }
    } else if (ctx.mouse.get_scrolled() != 0 && internal::is_boxed_in(ctx.mouse.get_cursor_pos(), boundingBox)) {
        value += static_cast<int>(ctx.mouse.get_scrolled()) * step;
        value = internal::clamp(value, min, max);
    }

    if (valueRef != value) {
        valueRef = value;
        return true;
    } else {
        return false;
    }
}

bool reig::reference_widget::slider::draw(reig::Context& ctx) const {
    return base_slider_draw(ctx, {mBoundingBox}, SliderOrientation::HORIZONTAL, mBaseColor, mValueRef, mMin, mMax, mStep);
}

bool reig::reference_widget::scrollbar::draw(reig::Context& ctx) const {
    auto step = ctx.get_font_height() / 2.0f;
    auto max = internal::min(mViewHeight - mBoundingBox.height, mBoundingBox.height);
    return base_slider_draw(ctx, {mBoundingBox}, SliderOrientation::VERTICAL, mBaseColor, mValueRef, 0.0f, max, step);
}

bool reig::reference_widget::slider_textured::draw(reig::Context& ctx) const {
    Rectangle boundingBox = this->mBoundingBox;
    ctx.fit_rect_in_window(boundingBox);

    // Render slider's base
    ctx.render_rectangle(boundingBox, mBaseTexture);

    auto [min, max, value, offset, valuesNum] = prepare_slider_values(mMin, mMax, mValueRef, mStep);

    // Render the cursor
    auto cursorBox = internal::decrease_box(boundingBox, 8);
    resize_slider_cursor(cursorBox.x, cursorBox.width, valuesNum, offset);
    ctx.render_rectangle(cursorBox, mCursorTexture);

    if (ctx.mouse.leftButton.is_pressed() && internal::is_boxed_in(ctx.mouse.leftButton.get_clicked_pos(), boundingBox)) {
        progress_slider_value(ctx.mouse.get_cursor_pos().x, cursorBox.width, cursorBox.x, min, max, mStep, value);
    } else if (ctx.mouse.get_scrolled() != 0 && internal::is_boxed_in(ctx.mouse.get_cursor_pos(), boundingBox)) {
        value += static_cast<int>(ctx.mouse.get_scrolled()) * mStep;
        value = internal::clamp(value, min, max);
    }

    if (mValueRef != value) {
        mValueRef = value;
        return true;
    } else {
        return false;
    }
}

bool reig::reference_widget::checkbox::draw(reig::Context& ctx) const {
    Rectangle boundingBox = this->mBoundingBox;
    ctx.fit_rect_in_window(boundingBox);

    render_widget_frame(ctx, boundingBox, mBaseColor);

    // Render check
    if (mValueRef) {
        boundingBox = internal::decrease_box(boundingBox, 4);
        Color contrastColor = internal::get_yiq_contrast(mBaseColor);
        ctx.render_rectangle(boundingBox, contrastColor);
    }

    // True if state changed
    if (ctx.mouse.leftButton.is_clicked() && internal::is_boxed_in(ctx.mouse.leftButton.get_clicked_pos(), boundingBox)) {
        mValueRef = !mValueRef;
        return true;
    } else {
        return false;
    }
}

bool reig::reference_widget::textured_checkbox::draw(reig::Context& ctx) const {
    Rectangle boundingBox = this->mBoundingBox;
    ctx.fit_rect_in_window(boundingBox);

    // Render checkbox's base
    ctx.render_rectangle(boundingBox, mBaseTexture);

    // Render check
    if (mValueRef) {
        boundingBox = internal::decrease_box(boundingBox, 8);
        ctx.render_rectangle(boundingBox, mCheckTexture);
    }

    // True if state changed
    if (ctx.mouse.leftButton.is_clicked() && internal::is_boxed_in(ctx.mouse.leftButton.get_clicked_pos(), boundingBox)) {
        mValueRef = !mValueRef;
        return true;
    } else {
        return false;
    }
}

void reig::Context::render_text(char const* ch, Rectangle aBox) {
    if (mFont.bakedChars.empty() || !ch) return;

    aBox = internal::decrease_box(aBox, 8);
    float x = aBox.x;
    float y = aBox.y + aBox.height;

    vector<stbtt_aligned_quad> quads;
    quads.reserve(20);
    float textWidth = 0.f;

    char from = ' ';
    char to = ' ' + 95;
    char c;

    for (; *ch; ++ch) {
        c = *ch;
        if (c < from || c > to) c = to;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(
                mFont.bakedChars.data(),
                mFont.width, mFont.height, c - ' ', &x, &y, &q, 1
        );
        if (q.x0 > aBox.x + aBox.width) {
            break;
        }
        if (q.x1 > aBox.x + aBox.width) {
            q.x1 = aBox.x + aBox.width;
        }
        if (q.y0 < aBox.y) {
            q.y0 = aBox.y;
        }

        textWidth += mFont.bakedChars[c - ' '].xadvance;

        quads.push_back(q);
    }

    auto deltax = (aBox.width - textWidth) / 2.f;

    for (auto& q : quads) {
        vector<Vertex> vertices{
                {{q.x0 + deltax, q.y0}, {q.s0, q.t0}, {}},
                {{q.x1 + deltax, q.y0}, {q.s1, q.t0}, {}},
                {{q.x1 + deltax, q.y1}, {q.s1, q.t1}, {}},
                {{q.x0 + deltax, q.y1}, {q.s0, q.t1}, {}}
        };
        vector<int> indices{0, 1, 2, 2, 3, 0};

        Figure fig;
        fig.form(vertices, indices, mFont.texture);
        mDrawData.push_back(fig);
    }
}

void reig::Context::render_triangle(Triangle const& aTri, Color const& aColor) {
    vector<Vertex> vertices{
            {{aTri.pos0}, {}, aColor},
            {{aTri.pos1}, {}, aColor},
            {{aTri.pos2}, {}, aColor}
    };
    vector<int> indices = {0, 1, 2};

    Figure fig;
    fig.form(vertices, indices);
    mDrawData.push_back(fig);
}

void reig::Context::render_rectangle(Rectangle const& aBox, int aTexture) {
    vector<Vertex> vertices{
            {{aBox.x,              aBox.y},               {0.f, 0.f}, {}},
            {{aBox.x + aBox.width, aBox.y},               {1.f, 0.f}, {}},
            {{aBox.x + aBox.width, aBox.y + aBox.height}, {1.f, 1.f}, {}},
            {{aBox.x,              aBox.y + aBox.height}, {0.f, 1.f}, {}}
    };
    vector<int> indices{0, 1, 2, 2, 3, 0};

    Figure fig;
    fig.form(vertices, indices, aTexture);
    mDrawData.push_back(fig);
}

void reig::Context::render_rectangle(Rectangle const& aRect, Color const& aColor) {
    vector<Vertex> vertices{
            {{aRect.x,               aRect.y},                {}, aColor},
            {{aRect.x + aRect.width, aRect.y},                {}, aColor},
            {{aRect.x + aRect.width, aRect.y + aRect.height}, {}, aColor},
            {{aRect.x,               aRect.y + aRect.height}, {}, aColor}
    };
    vector<int> indices{0, 1, 2, 2, 3, 0};

    Figure fig;
    fig.form(vertices, indices);
    mDrawData.push_back(fig);
}
