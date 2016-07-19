#include "StopWatch.h"

#include <reig.h>

#include <SDL2_gfxPrimitives.h>
#include <SDL2_framerate.h>

#include <iostream>
#include <iomanip>

using namespace std::string_literals;

class Main {
public:
    Main() {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
        sdl.window = SDL_CreateWindow(
            "reig SDL testbed",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            sdl.width, sdl.height,
            SDL_WINDOW_SHOWN
        );
        sdl.renderer = SDL_CreateRenderer(
            sdl.window, -1, 
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );
        
        gui.ctx.set_render_handler(&gui_handler);
        gui.ctx.set_user_ptr(this);
        
        gui.font.data = gui.ctx.set_font("/usr/share/fonts/TTF/impact.ttf", gui.font.id, 20.f);
        auto surf = SDL_CreateRGBSurfaceFrom(
            gui.font.data.bitmap, gui.font.data.width, gui.font.data.height, 8, gui.font.data.width, 
            0, 0, 0, 0xFF
        );
        SDL_Color colors[256];
        for(auto i = 0u; i < 256; ++i) {
            colors[i].r = 0xFF;
            colors[i].g = 0xFF;
            colors[i].b = 0xFF;
            colors[i].a = i;
        }
        SDL_SetPaletteColors(surf->format->palette, colors, 0, 256);
        gui.font.tex = SDL_CreateTextureFromSurface(sdl.renderer, surf);
    }
    
    int run() {
        int last;
        std::string fpsString;
        bool quit = false;
        // FPSmanager fpsManager;
        // SDL_initFramerate(&fpsManager);
        // SDL_setFramerate(&fpsManager, 60);
        while(!quit) {
            last = SDL_GetTicks();
            gui.ctx.start_new_frame();
            
            // =================== Input polling ===============
            for(SDL_Event evt; SDL_PollEvent(&evt);) {
                switch(evt.type) {
                    case SDL_QUIT:
                    quit = true;
                    break;
                    
                    case SDL_MOUSEMOTION:
                    gui.ctx.mouse.place(evt.motion.x, evt.motion.y);
                    break;
                    
                    case SDL_MOUSEWHEEL:
                    gui.ctx.mouse.scroll(-evt.wheel.y);
                    break;
                    
                    case SDL_MOUSEBUTTONDOWN: {
                        if(evt.button.button == SDL_BUTTON_LEFT) {
                            gui.ctx.mouse.left.press(evt.button.x, evt.button.y);
                        }
                    }
                    break;
                    
                    case SDL_MOUSEBUTTONUP: {
                        if(evt.button.button == SDL_BUTTON_LEFT) {
                            gui.ctx.mouse.left.release();
                        }
                    }
                    
                    default:;
                }
                if(quit) break;
            }
            if(quit) break;
            
            // ================== GUI setup =================
            reig::Rectangle rect {40, 0, 100, 30};
            reig::Color color {120, 100, 150};
            
            static float winX = 10, winY = 10;
            gui.ctx.start_window(fpsString.c_str(), winX, winY);
            for(int i = 0; i < 4; ++i) {
                rect.x -= 10; rect.y = 40 * i;
                color.r += 25; color.g += 25;
                std::string title = "some  " + std::to_string(i + 1);
                if(gui.ctx.button(title.c_str(), rect, color)) {
                    std::cout << "Button " << (i + 1) << ": pressed" << std::endl;
                }
            }
            
            color.g += 50;
            rect.y += 40; rect.w += 50;
            static float sliderValue0 = 20;
            if(gui.ctx.slider(rect, color, sliderValue0, 20, 40, 5)) {
                std::cout << "Slider 1: new value " << sliderValue0 << std::endl;
            }
            
            color.g += 50;
            rect.y += 40;
            rect.w += 50;
            
            static float sliderValue1 = 5.4f;
            if(gui.ctx.slider(rect, color, sliderValue1, 3, 7, 0.1f)) {
                std::cout << "Slider 2: new value " << sliderValue1 << std::endl;
            }
            
            static float sliderValue2 = 0.3f;
            rect.y += 40; rect.w += 50; rect.h += 10;
            if(gui.ctx.slider(
                rect, 
                reig::Color(220, 200, 150),
                sliderValue2, 0.1f, 0.5f, 0.05f
            )) {
                std::cout << "Slider 2: new value " << sliderValue2 << std::endl;
            }
            
            static bool checkBox1 = false;
            color.r += 15; color.g -= 35; color.b -= 10;
            rect.x += 270; rect.w = 40; rect.h = 20;
            if(gui.ctx.checkbox(rect, color, checkBox1)) {
                std::cout << "Checkbox 1: new value " << checkBox1 << std::endl;
            }
            
            static bool checkBox2 = true;
            color.r -= 100; color.g += 100; color.b += 100;
            rect.y -= 100; rect.w = rect.h = 50;
            if(gui.ctx.checkbox(rect, color, checkBox2)) {
                std::cout << "Checkbox 2: new value " << checkBox2 << std::endl;
            }
            
            static bool checkBox3 = false;
            color.asUint = 0xFFFFFFFF;
            rect.y += 60; rect.w = rect.h = 25;
            if(gui.ctx.checkbox(rect, color, checkBox3)) {
                std::cout << "Checkbox 3: new value " << checkBox3 << std::endl;
            }
            
            gui.ctx.end_window();
            
            // ================== Render ==================== 
            SDL_SetRenderDrawColor(sdl.renderer, 50, 50, 50, 255);
            SDL_RenderClear(sdl.renderer);
            
            auto ticks = SDL_GetTicks() - last;
            if(ticks != 0) {
                ticks = 1000 / ticks;
                fpsString = std::to_string(ticks) + " FPS";
            }
            gui.ctx.render_all();
            
            int mx, my;
            int state = SDL_GetMouseState(&mx, &my);
            if((state & SDL_BUTTON_LMASK) == SDL_BUTTON_LMASK) {
                filledCircleRGBA(sdl.renderer, mx, my, 15, 150, 220, 220, 150);
            }
            
            SDL_RenderPresent(sdl.renderer);
        }
        
        return 0;
    }

    static void gui_handler(reig::DrawData const& drawData, void* vSelf) {
        Main* self = static_cast<Main*>(vSelf);
        
        for(auto const& fig : drawData) {
            auto const& vertices = fig.vertices();
            auto const& indices = fig.indices();
            auto number = indices.size();
            
            if(number % 3 != 0) {
                continue;
            }
            
            if(fig.texture() == 0) {
                for(auto i = 0ul; i < number; i += 3) {
                    filledTrigonColor(
                        self->sdl.renderer,
                        vertices[indices[i  ]].position.x,
                        vertices[indices[i  ]].position.y,
                        vertices[indices[i+1]].position.x,
                        vertices[indices[i+1]].position.y,
                        vertices[indices[i+2]].position.x,
                        vertices[indices[i+2]].position.y,
                        vertices[i].color.asUint
                    );
                }
            }
            else if(fig.texture() == self->gui.font.id) {
                SDL_Rect src;
                src.x = vertices[0].texCoord.x * self->gui.font.data.width;
                src.y = vertices[0].texCoord.y * self->gui.font.data.height;
                src.w = vertices[2].texCoord.x * self->gui.font.data.width  - src.x;
                src.h = vertices[2].texCoord.y * self->gui.font.data.height - src.y;
                SDL_Rect dst;
                dst.x = vertices[0].position.x;
                dst.y = vertices[0].position.y;
                dst.w = vertices[2].position.x - dst.x;
                dst.h = vertices[2].position.y - dst.y;
                SDL_RenderCopy(self->sdl.renderer, self->gui.font.tex, &src, &dst);
            }
        }
    }
    
private:
    struct sdl {
        SDL_Renderer* renderer = nullptr;
        SDL_Window*   window   = nullptr;
        int width  = 800;
        int height = 600;
    }
    sdl;
    
    struct gui {
        reig::Context ctx;
        
        struct font {
            reig::FontData data;
            SDL_Texture*   tex  = nullptr;
            unsigned       id   = 100;
        }
        font;
    }
    gui;
};

int main(int, char*[]) {
    return Main{}.run();
}
