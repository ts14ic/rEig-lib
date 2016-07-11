#ifndef REIG_H_INCLUDED
#define REIG_H_INCLUDED

#include <stb_truetype.h>
#include <vector>

namespace reig {
    inline namespace value_types {
        using ubyte_t = unsigned char;
        using int_t   = signed;
        using uint_t  = unsigned;
        using size_t  = unsigned long long;        
        using float_t = float;
        
        static_assert(sizeof(uint_t) >= 4 * sizeof(ubyte_t), "uint_t is too small");
    }
    
    inline namespace graphic_base_types {
        struct Point {
            Point() = default;
            Point(float_t x, float_t y)
                : x{x}, y{y} {}
            
            float_t x {};
            float_t y {};
        };
        
        struct Size {
            Size() = default;
            Size(float_t w, float_t h)
                : w{w}, h{h} {}
            
            float_t w {};
            float_t h {};
        };
        
        struct Rectangle {
            Rectangle() = default;
            Rectangle(float_t x, float_t y, float_t w, float_t h)
                : x{x}, y{y}, w{w}, h{h} {}
            Rectangle(Point pos, Size size) 
                : pos{pos}, size{size} {}
            
            union {
                Point pos {};
                struct { float_t x; float_t y; };
            };
            union {
                Size size {};
                struct { float_t w; float_t h; };
            };
        };
        
        struct Triangle {
            Triangle() = default;
            Triangle(float_t x0, float_t y0, float_t x1, float_t y1, float_t x2, float_t y2)
                : pos0{x0, y0}, pos1{x1, y1}, pos2{x2, y2} {}
            Triangle(Point const& pos0, Point const& pos1, Point const& pos2)
                : pos0{pos0}, pos1{pos1}, pos2{pos2} {}
                
            union {
                Point pos0 {};
                struct { float_t x0; float_t y0; };
            };
            union {
                Point pos1 {};
                struct { float_t x1; float_t y1; };
            };
            union {
                Point pos2 {};
                struct { float_t x2; float_t y2; };
            };
        };
        
        struct Color {
            Color() 
                : asUint{0u} { a = 0xFF; }
            Color(ubyte_t r, ubyte_t g, ubyte_t b, ubyte_t a = {0xFFu})
                : r{r}, g{g}, b{b}, a{a} {}
            
            union {
                struct {
                    ubyte_t r;
                    ubyte_t g;
                    ubyte_t b;
                    ubyte_t a;
                };
                uint_t asUint;
            };
        };
        
        using Colour = Color;
        
        struct Vertex {
            Vertex() = default;
            
            Point position {};
            Point texCoord {};
            Color color    {};
        };
    }
    
    /*forward*/ class Context;
    /*forward*/ class Mouse;
    class MouseButton {
    public:
        /**
         * @brief Sets mouse pressed and clicked states
         * @param x X coordinate
         * @param y Y coordinate
         */
        void press(float_t x, float_t y);
        
        /**
         * @brief Unsets mouse pressed state
         */
        void release();
    private:
        friend class ::reig::Context;
        friend class ::reig::Mouse;
        
        MouseButton() = default;
        MouseButton(MouseButton const&) = delete;
        MouseButton(MouseButton&&) = delete;
        MouseButton& operator=(MouseButton const&) = delete;
        MouseButton& operator=(MouseButton&&) = delete;
    
        Point _clickedPos;
        bool  _pressed = false;
        bool  _clicked = false;
    };
    
    class Mouse {
    public:
        Mouse() = default;
        Mouse(Mouse const&) = delete;
        Mouse(Mouse&&) = delete;
        Mouse& operator=(Mouse const&) = delete;
        Mouse& operator=(Mouse&&) = delete;
    
        MouseButton left, right;
    
        /**
         * @brief Moves cursor against previous position
         * @param difx Delta x coordinate
         * @param dify Delta y coordinate
         */
        void move(float_t difx, float_t dify);
        
        /**
         * @brief Places the cursors in abosulute coordinates
         * @param x X coordinate
         * @param y Y coordiante
         */
        void place(float_t x, float_t y);
        
        /**
         * @brief Scrolls the virtual mouse wheel
         * @param dy Amount of scrolling
         */
        void scroll(float dy);
    private:
        friend class ::reig::Context;
        
        Point _cursorPos;
        float _scroll;
    };
    
    /**
     * @class Figure
     * @brief A bunch of vertices and indices to render a figure
     * Can be collected by the user, but formation is accessible only for the Context
     */
    class Figure {
    public:
        /**
         * @brief Returns figure's read-only vertices
         */
        std::vector<Vertex> const& vertices() const;
        
        /**
         * @brief Returns figure's read-only indices
         */
        std::vector<uint_t> const&  indices() const;
        
        /**
         * @brief Return figure's texture index
         */
        uint_t texture() const;
    private:
        Figure() = default;
        friend class ::reig::Context;
        
        void form(std::vector<Vertex>& vertices, std::vector<uint_t>& indices, uint_t id = 0);
    
        std::vector<Vertex> _vertices;
        std::vector<uint_t> _indices;
        uint_t              _texture = 0;
    };
    
    struct FontData {
        ubyte_t* bitmap = nullptr;
        uint_t width    = 0;
        uint_t height   = 0;
        
        void auto_free();
        ~FontData();
    private:
        bool _free = false;
    };
    
    using DrawData = std::vector<Figure>;
    using CallbackType = void (*)(DrawData const&, void*);
    
    /**
     * @class Context
     * @brief Used to pump in input and request gui creation
     */
    class Context {
    public:
        Context() = default;
        
        /**
         * @brief Set's a user function, which will draw the gui, based 
         * @param handler A C function pointer to a rendering callback
         * The handler should return void and take in DrawData const& and void*
         */
        void set_render_handler(CallbackType handler);
        
        /**
         * @brief Set a user pointer to store in the context.
         * @param ptr A user data pointer.
         * The pointer is then automatically passed to callbacks.
         */
        void set_user_ptr(void* ptr);
        
        /**
         * @brief Gets the stored user pointer
         */
        void const* get_user_ptr() const;
        
        /**
         * @brief Sets reig's font to be used for labels
         * @param path The path to fonts .ttf file.
         * @param textureId This id will be passed by reig to render_handler with text vertices
         * @param size Font's pixel size
         * @return Returns the bitmap, which is used to create a texture by user.
         * Set returned bitmap field to nullptr, to avoid deletion
         */
        FontData set_font(char const* path, uint_t textureId, float size);
        
        /**
         * @brief Resets draw data and inputs
         */
        void start_new_frame();
        
        /**
         * @brief Uses stored drawData and draws everything using the user handler
         */
        void render_all();
        
        // Inputs
        Mouse mouse;
        
        // Widget renders
        void start_window(char const* title, float& x, float& y);
        void end_window();
        
        /**
         * @brief Render some text
         * @param text Text to be displayed
         * @param box Text's bounding box
         */
        void label(char const* text, Rectangle box);
        
        /**
         * @brief Render a titled button
         * @param title Text to be displayed on button
         * @param box Button's bounding box
         * @param color Button's base color
         * @return True if the button was clicked, false otherwise
         */
        bool button(char const* title, Rectangle box, Color color);
        
        /**
         * @brief Render a titled textured button
         * @param box Button's bouding box
         * @param baseTexture Button's texture index, when idle
         * @param hoverTexture Button's texture index, when button is hoverred
         * @return True if the button was clicked, false otherwise
         */
        bool button(char const* title, Rectangle box, int baseTexture, int hoverTexture);
        
        /**
         * @brief Renders a slider.
         * @param box Slider's bounding box
         * @param color Slider's base color
         * @param value A reference to the value to be represented and changed
         * @param min The lowest represantable value
         * @param max The highest represantable value
         * @param step The discrete portion by which the value can change
         * @return True if value changed
         */
        bool slider(Rectangle box, Color color, float_t& value, float_t min, float_t max, float_t step);
        
        /**
         * @brief 
         * @brief Renders a slider.
         * @param box Slider's bounding box
         * @param baseTexture Slider's base texture index
         * @param cursorTexture Slider's cursor texture index
         * @param value A reference to the value to be represented and changed
         * @param min The lowest represantable value
         * @param max The highest represantable value
         * @param step The discrete portion by which the value can change
         * @return True if value changed
         */
        bool slider(Rectangle box, int baseTexture, int cursorTexture, float& value, float_t min, float_t max, float_t step);
        
        /**
         * @brief Renders a checkbox
         * @param box Checkbox's position and size
         * @param color Checkbox's base color
         * @param value A reference to the bool to be changed
         * @return True if value changed
         */
        bool checkbox(Rectangle box, Color color, bool& value);
        
        /**
         * @brief Renders a textured checkbox
         * @param box Checkbox's position and size
         * @param baseTexture Checkbox's base texture
         * @param tickTexture Checkbox's tick texture
         * @param value A reference to the bool to be changed
         * @return True if value changed
         */
        bool checkbox(Rectangle box, int baseTexture, int tickTexture, bool& value);
        
        /**
         * @brief Renders a vertical scrollbar
         * @param box Scrollbar's position and size
         * @param color Checkbox's base color
         * @param value A reference to the bool to be changed
         * @param min Scrollbar's output min value
         * @param max Scrollbar's output max value
         * @param logicalHeight Max logical value 
         * @return 
         */
        bool scrollbar(Rectangle box, Color color, float_t& value, float_t min, float_t max, float_t logicalHeight);
         
        // Primitive renders
        /**
         * @brief Schedules a rectangle drawing
         * @param rect Position and size
         * @param color Color
         */
        void render_rectangle(Rectangle const& rect, Color const& color);
        
        /**
         * @brief Schedules a textured rectangle drawing (the texture is stretched)
         * @param rect Position and size
         * @param texture Index to the texture
         */
        void render_rectangle(Rectangle const& rect, int texture);
        
        /**
         * @brief Schedules a triangle drawing
         * @param triangle Position and size
         * @param color Color
         */
        void render_triangle(Triangle const& triangle, Color const& color);
    private:
        std::vector<Figure> _drawData;
        CallbackType        _renderHandler = nullptr;
        void*               _userPtr = nullptr;
        
        struct Font {
            stbtt_bakedchar* bakedChars = nullptr;
            float_t size   = 0.f;
            uint_t texture = 0;
            uint_t width   = 0;
            uint_t height  = 0;
        } 
        _font;
        
        struct Window {
            std::vector<Figure> drawData;
            char const* title = nullptr;
            float* x = nullptr;
            float* y = nullptr;
            float w = 0.f;
            float h = 0.f;
            float headerSize = 0.f;
            bool started = false;
            
            void expand(Rectangle& box);
        }
        _window;
    };    
}

#endif // REIG_H_INCLUDED
