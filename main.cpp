#include <SDL.h>
#include <cmath>
#include <algorithm>

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

static SDL_Window * window = nullptr;
static SDL_Renderer * renderer = nullptr;
// static SDL_Texture * texture = nullptr;
static SDL_Surface * paint_surface = nullptr;
static SDL_Texture * paint_texture = nullptr;
static SDL_Surface * font_surface = nullptr;
static SDL_Texture * font_texture = nullptr;

/*

TODO:
- support multiple windows
- refactor out anonymous, parameter-less,
  referencing functions

*/

// SDL_FPoint finger_to_lib_space(float finger_x, float finger_y)
// {
//     int window_w = 0;
//     int window_h = 0;
//     SDL_assert(window != nullptr);
//     SDL_GetWindowSize(window, &window_w, &window_h);


//     return {12,34};
// }



constexpr void set_pixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    Uint32 *target_pixel = (Uint32 *)(
        (Uint8 *)surface->pixels
        + y * surface->pitch
        + x * sizeof(*target_pixel)
    );
    *target_pixel = pixel;
}

// this function gives the maximum
float maxi(float arr[], int n)
{
    float m = 0;
    for (int i = 0; i < n; ++i)
        if (m < arr[i])
            m = arr[i];
    return m;
}

// this function gives the minimum
float mini(float arr[], int n)
{
    float m = 1;
    for (int i = 0; i < n; ++i)
        if (m > arr[i])
            m = arr[i];
    return m;
}


void liang_barsky_clipper(float xmin, float ymin, float xmax, float ymax,
                          float x1, float y1, float x2, float y2,
                          float *out_x1, float *out_y1, float *out_x2, float *out_y2)
{
    // defining variables
    float p1 = -(x2 - x1);
    float p2 = -p1;
    float p3 = -(y2 - y1);
    float p4 = -p3;

    float q1 = x1 - xmin;
    float q2 = xmax - x1;
    float q3 = y1 - ymin;
    float q4 = ymax - y1;

    float posarr[5], negarr[5];
    int posind = 1, negind = 1;
    posarr[0] = 1;
    negarr[0] = 0;

    // rectangle(xmin, ymin, xmax, ymax); // drawing the clipping window

    if ((p1 == 0 && q1 < 0) || (p2 == 0 && q2 < 0) || (p3 == 0 && q3 < 0) || (p4 == 0 && q4 < 0))
    {
        // outtextxy(80, 80, "Line is parallel to clipping window!");
        return;
    }
    if (p1 != 0)
    {
        float r1 = q1 / p1;
        float r2 = q2 / p2;
        if (p1 < 0)
        {
            negarr[negind++] = r1; // for negative p1, add it to negative array
            posarr[posind++] = r2; // and add p2 to positive array
        }
        else
        {
            negarr[negind++] = r2;
            posarr[posind++] = r1;
        }
    }
    if (p3 != 0)
    {
        float r3 = q3 / p3;
        float r4 = q4 / p4;
        if (p3 < 0)
        {
            negarr[negind++] = r3;
            posarr[posind++] = r4;
        }
        else
        {
            negarr[negind++] = r4;
            posarr[posind++] = r3;
        }
    }

    float xn1, yn1, xn2, yn2;
    float rn1, rn2;
    rn1 = maxi(negarr, negind); // maximum of negative array
    rn2 = mini(posarr, posind); // minimum of positive array

    if (rn1 > rn2)
    { // reject
        // outtextxy(80, 80, "Line is outside the clipping window!");
        // return;
        xn1 = x1;
        yn1 = y1;
        xn2 = x2;
        yn2 = y2;
    } else {
        xn1 = x1 + p2 * rn1;
        yn1 = y1 + p4 * rn1; // computing new points
        xn2 = x1 + p2 * rn2;
        yn2 = y1 + p4 * rn2;
    }

    if (out_x1) { *out_x1 = x1; }
    if (out_y1) { *out_y1 = y1; }
    if (out_x2) { *out_x2 = x2; }
    if (out_y2) { *out_y2 = y2; }


    // if (out_rect) {


    // setcolor(CYAN);

    // line(xn1, yn1, xn2, yn2); // the drawing the new line

    // setlinestyle(1, 1, 0);

    // line(x1, y1, xn1, yn1);
    // line(x2, y2, xn2, yn2);
}

void liang_barsky_clipper(float xmin, float ymin, float xmax, float ymax,
                          float *x1, float *y1, float *x2, float *y2)
{
    liang_barsky_clipper(
        xmin, ymin, xmax, ymax,
        *x1, *y1, *x2, *y2,
        x1, y1, x2, y2
    );
}

// SDL_FPoint get_logical_size()
// {
//     int w = 0;
//     int h = 0;
//     SDL_RenderGetLogicalSize(renderer, &w, &h);
//     if (w == 0 || h == 0) {
//         SDL_GetWindowSize(window, &w, &h);
//     }
//     return {
//         static_cast<float>(w),
//         static_cast<float>(h)
//     };
// }

// "logical space" - the SDL_Renderer's logical space
//
SDL_FPoint finger_to_logical_space(float finger_x, float finger_y, bool *is_inside_logical_space)
{
    // SDL_Window *window = SDL_GetWindowFromID(window_id);
    // SDL_assert(window != nullptr);  // TODO: handle NULL window with more grace?
    // int window_w = 0;
    // int window_h = 0;
    // SDL_GetWindowSize(window, &window_w, &window_h);

    // SDL_Rect viewport = {0};
    // SDL_RenderGetViewport(renderer, &viewport);

    int logical_w = 0;
    int logical_h = 0;
    SDL_RenderGetLogicalSize(renderer, &logical_w, &logical_h);

    // Finger coordinates are normalized in the 0..1 range
    const SDL_FPoint finger_point_in_logical_space = {
        finger_x * static_cast<float>(logical_w),
        finger_y * static_cast<float>(logical_h)
    };

    if (is_inside_logical_space) {
        *is_inside_logical_space = ! \
            (finger_point_in_logical_space.x < 0. ||
            finger_point_in_logical_space.x >= static_cast<float>(logical_w) ||
            finger_point_in_logical_space.y < 0. ||
            finger_point_in_logical_space.y >= static_cast<float>(logical_h));
    }

    return finger_point_in_logical_space;
}

void log_renderer_info()
{
    SDL_Rect rd;
    SDL_Point ptd;
    SDL_FPoint ptf;
    SDL_bool b;

    SDL_RenderGetClipRect(renderer, &rd);
    b = SDL_RenderIsClipEnabled(renderer);
    SDL_Log("clip rect, enabled?=%d, pos={%d,%d}, sz={%d,%d}", b, rd.x, rd.y, rd.w, rd.h);

    b = SDL_RenderGetIntegerScale(renderer);
    SDL_Log("integer scale?=%d", b);

    SDL_RenderGetLogicalSize(renderer, &ptd.x, &ptd.y);
    SDL_Log("logical size={%d,%d}", ptd.x, ptd.y);

    SDL_RenderGetScale(renderer, &ptf.x, &ptf.y);
    SDL_Log("scale={%f,%f}", ptf.x, ptf.y);

    SDL_RenderGetViewport(renderer, &rd);
    SDL_Log("viewport, pos={%d,%d}, sz={%d,%d}", rd.x, rd.y, rd.w, rd.h);
}

void process_paint_events(const SDL_Event &ev)
{
    switch (ev.type) {
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        {
            bool finger_in_logical_space = false;
            const SDL_FPoint finger_point_in_logical_space = \
                finger_to_logical_space(ev.tfinger.x, ev.tfinger.y, &finger_in_logical_space);

            // SDL_LogVerbose(SDL_LOG_CATEGORY_INPUT,
            //     "%s, finger point in logical space: {%d,%d} %s", __FUNCTION__,
            //     finger_point_in_logical_space.x, finger_point_in_logical_space.y,
            //     (finger_in_logical_space ? "INSIDE" : "OUTSIDE")
            // );

            if (finger_in_logical_space) {
                set_pixel(
                    paint_surface,
                    static_cast<int>(finger_point_in_logical_space.x),
                    static_cast<int>(finger_point_in_logical_space.y),
                    SDL_MapRGBA(paint_surface->format, 255, 255, 0, 255)
                );
            }
            break;
        }
        default:
            break;
    }
}


static const float Default_Radius = 40.f;

struct DL_Joy {
    SDL_FingerID fingerId;
    enum {
        Inactive,
        Active
    } state;
    SDL_FPoint center;
    SDL_FPoint current;
    float radius;
    // SDL_FPoint mid;
    // context (screen size?  display number?) -- for now, use full window size
};

DL_Joy djoy;


const char * FormatDisplayMode(char * buf, size_t bufLen, const SDL_DisplayMode * mode)
{
    SDL_snprintf(buf, bufLen, "format:0x%x, w:%d, h:%d, refresh_rate:%d, driverdata:0x%p",
        mode->format, mode->w, mode->h, mode->refresh_rate, mode->driverdata);
    return buf;
}

void LogDisplayInfo()
{
    int numVideoDisplays = 0;
    int videoDisplayIndex = 0;
    char buf[1024];

    numVideoDisplays = SDL_GetNumVideoDisplays();
    if (numVideoDisplays < 1)
    {
        SDL_Log("WARNING: SDL_GetNumVideoDisplays() failed, reason=\"%s\".\n",
            SDL_GetError());
        return;
    }
    SDL_Log("Num video displays: %d\n", numVideoDisplays);
    for (videoDisplayIndex = 0; videoDisplayIndex < numVideoDisplays; ++videoDisplayIndex)
    {
		SDL_DisplayMode displayMode;
        int numDisplayModes = 0;
		const char * videoDriverName = SDL_GetVideoDriver(videoDisplayIndex);
        const char * displayName = SDL_GetDisplayName(videoDisplayIndex);
        float ddpi, hdpi, vdpi;

		// Display general information on the video display:
		SDL_Log("Video Display #%d, video driver:%s, display name:%s\n",
			videoDisplayIndex,
			(videoDriverName ? videoDriverName : "(null)"),
            (displayName ? displayName : "(null)"));
        if (SDL_GetDisplayDPI(videoDisplayIndex, &ddpi, &hdpi, &vdpi) != 0) {
            SDL_Log("WARNING: Unable to get the dpi for video display %d, error = \"%s\"\n",
                videoDisplayIndex, SDL_GetError());
        } else {
            SDL_Log("Video Display #%d, ddpi:%f, hdpi:%f, vdpi:%f\n",
                videoDisplayIndex, ddpi, hdpi, vdpi);
        }

		// Display information on each of the display's video modes:
		numDisplayModes = SDL_GetNumDisplayModes(videoDisplayIndex);
		if (numDisplayModes < 1) {
			SDL_Log("WARNING: Unable to retrieve the number of display modes for video display %d (via SDL_GetNumDisplayModes), error = \"%s\".\n",
                videoDisplayIndex, SDL_GetError());
		} else {
            int displayModeIndex = 0;

            if (SDL_GetDesktopDisplayMode(videoDisplayIndex, &displayMode)) {
                SDL_Log("WARNING: Unable to get information on the desktop display mode for video display #%d (via SDL_GetDesktopDisplayMode), error = \"%s\".\n",
                    videoDisplayIndex, SDL_GetError());
            } else {
                SDL_Log("Video Display #%d, Desktop Display Mode: %s\n",
                    videoDisplayIndex, FormatDisplayMode(buf, sizeof(buf), &displayMode));
            }

            if (SDL_GetCurrentDisplayMode(videoDisplayIndex, &displayMode)) {
                SDL_Log("WARNING: Unable to get information on the current display mode for video display #%d (via SDL_GetCurrentDisplayMode), error = \"%s\".\n",
                    videoDisplayIndex, SDL_GetError());
            } else {
                SDL_Log("Video Display #%d, Current Display Mode: %s\n",
                    videoDisplayIndex, FormatDisplayMode(buf, sizeof(buf), &displayMode));
            }

            SDL_Log("Number of display modes for video display %d (via SDL_GetNumDisplayModes): %d\n",
                videoDisplayIndex, numDisplayModes);

			for (displayModeIndex = 0; displayModeIndex < numDisplayModes; ++displayModeIndex) {
				if (SDL_GetDisplayMode(videoDisplayIndex, displayModeIndex, &displayMode) != 0) {
					SDL_Log("WARNING: Unable to get information on display mode #%d for video display #%d (via SDL_GetDisplayMode), error = \"%s\".\n",
						displayModeIndex, videoDisplayIndex, SDL_GetError());
				} else {
					SDL_Log("Video Display #%d, Display Mode #%d: %s\n",
						videoDisplayIndex, displayModeIndex,
                        FormatDisplayMode(buf, sizeof(buf), &displayMode));
				}
			}
		}
	}
}

const char * DebugText = "A B~C";

void DrawTextScaled(int x, int y, int scale, const char *text)
{
    static bool first = true;
    const int char_w = 5;
    const int char_h = 12;
    const int total_w = 100;
    const int total_h = 60;
    // int total_w = 0;
    // int total_h = 0;
    // SDL_QueryTexture(font_texture, nullptr, nullptr, &total_w, &total_h);

    const int num_per_row = (total_w / char_w);

    SDL_Rect src_rect = {0, 0, char_w, char_h};
    SDL_Rect dst_rect = {x, y, char_w * scale, char_h * scale};
    while (*text != '\0') {
        if (*text >= ' ' && *text <= '~') {
            int ch_index = *text - ' ';
            int row = ch_index / (num_per_row);
            //int col = ch_index - (row * total_w);
            int col = ch_index - (row * num_per_row);
            src_rect.x = col * char_w;
            src_rect.y = row * char_h;
            if (first) {
                SDL_Log("%s ch_index=%02d num-pr=%d row=%d col=%d src={%d,%d,%d,%d} dst={%d,%d,%d,%d}",
                    __FUNCTION__, ch_index, num_per_row, row, col,
                    src_rect.x, src_rect.y, src_rect.w, src_rect.h,
                    dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h
                );
            }
            SDL_RenderCopy(renderer, font_texture, &src_rect, &dst_rect);
        }
        dst_rect.x += char_w * scale;
        ++text;
    }

    first = false;

}

int DefaultTextScale = 2;
void DrawText(int x, int y, const char *text)
{
    DrawTextScaled(x, y, DefaultTextScale, text);
}

int main(int, char **)
{
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    // LogDisplayInfo();

    if (SDL_CreateWindowAndRenderer(320, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP, &window, &renderer) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 1;
    }

    int vjoy_index = \
        SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                  SDL_CONTROLLER_AXIS_MAX,
                                  SDL_CONTROLLER_BUTTON_MAX,
                                  0);
    if (vjoy_index < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickAttachVirtual failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Joystick *joystick = SDL_JoystickOpen(vjoy_index);
    if (!joystick) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickOpen failed: %s", SDL_GetError());
        return 1;
    }
    // SDL_Log("joystick: %p", joystick);
    // SDL_JoystickGUID joystick_guid = SDL_JoystickGetGUID(joystick);
    // char joystick_guid_str[33] = {0};
    // SDL_JoystickGetGUIDString(joystick_guid, joystick_guid_str, sizeof(joystick_guid_str));
    // SDL_Log("joystick guid: %s\n", joystick_guid_str);

    SDL_GameController *controller = SDL_GameControllerOpen(vjoy_index);
    if (!controller) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GameControllerOpen failed: %s", SDL_GetError());
        return 1;
    }
    // SDL_Log("controller: %p", controller);

    djoy.radius = Default_Radius;

    // Since we're using renderer's logical-size to determine the
    // joystick's logical-space size, and because we want the
    // logical-space to encompass the entire window (whose size
    // may be set by the OS, via SDL), we'll apply the window-size
    // to the renderer's logical-size, here.
    int current_window_width = 0;
    int current_window_height = 0;
    SDL_GetWindowSize(window, &current_window_width, &current_window_height);
    SDL_RenderSetLogicalSize(renderer, current_window_width, current_window_height);

    // const int logical_width = 100;
    // const int logical_height = 320;
    // SDL_RenderSetLogicalSize(renderer, logical_w, logical_h);

    paint_surface = SDL_CreateRGBSurface(
        // flags
        0,

        // size
        current_window_width, current_window_height,
        
        // bits per pixel
        32,

        // masks: R, G, B, A
        0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
    );
    SDL_FillRect(paint_surface, NULL, SDL_MapRGBA(paint_surface->format, 255, 0, 0, 255));
    SDL_Rect tmp_rect = {1, 1, current_window_width - 2, current_window_height - 2};
    SDL_FillRect(paint_surface, &tmp_rect, SDL_MapRGBA(paint_surface->format, 0, 0, 0, 0));
    paint_texture = SDL_CreateTextureFromSurface(renderer, paint_surface);

    // font_surface = STBIMG_Load("matchup-pro-v1.1.png");
    font_surface = STBIMG_Load("gnsh-bitmapfont-colour8.png");
    SDL_Log("font_surface=%p ; size=%dx%d",
        font_surface,
        (font_surface ? font_surface->w : 0),
        (font_surface ? font_surface->h : 0)
    );
    font_texture = SDL_CreateTextureFromSurface(renderer, font_surface);
    int tex_w = 0;
    int tex_h = 0;
    SDL_QueryTexture(font_texture, nullptr, nullptr, &tex_w, &tex_h);
    SDL_Log("font_texture=%p ; size=%dx%d",
        font_texture,
        tex_w,
        tex_h
    );


    while (true)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    return 0;
                case SDL_FINGERDOWN: {
                    bool finger_in_logical_space = false;
                    const SDL_FPoint finger_point_in_logical_space = \
                        finger_to_logical_space(ev.tfinger.x, ev.tfinger.y, &finger_in_logical_space);

                    SDL_LogVerbose(SDL_LOG_CATEGORY_INPUT,
                        "%s, finger in logical: {%f,%f} %s fgr_id=%llu", __FUNCTION__,
                        finger_point_in_logical_space.x, finger_point_in_logical_space.y,
                        (finger_in_logical_space ? "IN" : "OUT"),
                        ev.tfinger.fingerId
                    );

                    if (finger_in_logical_space) {
                        [&]() { // start_joystick
                            if (djoy.state != DL_Joy::Inactive) {
                                return;
                            }
                            djoy.center = djoy.current = {
                                finger_point_in_logical_space.x,
                                finger_point_in_logical_space.y
                            };
                            djoy.fingerId = ev.tfinger.fingerId;
                            djoy.state = DL_Joy::Active;
                            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                                "JOY START: {%f,%f}",
                                finger_point_in_logical_space.x,
                                finger_point_in_logical_space.y
                            );
                        }();
                    }

                    // SDL_LogVerbose(SDL_LOG_CATEGORY_INPUT,
                    //     "--------------------------------------------------");
                    break;
                }
                case SDL_FINGERMOTION: {
                    if (djoy.fingerId == ev.tfinger.fingerId) {
                        if (djoy.state == DL_Joy::Active) {
                            bool is_finger_inside = false;
                            const SDL_FPoint finger_in_logical_space = \
                                finger_to_logical_space(ev.tfinger.x, ev.tfinger.y, &is_finger_inside);

                            // SDL_LogVerbose(SDL_LOG_CATEGORY_INPUT,
                            //     "%s, finger in logical: {%f,%f} %s fgr_id=%llu", __FUNCTION__,
                            //     finger_in_logical_space.x, finger_in_logical_space.y,
                            //     (is_finger_inside ? "IN" : "OUT"),
                            //     ev.tfinger.fingerId
                            // );

                            // if (is_finger_inside) {
                                djoy.current = finger_in_logical_space;
                            // } else {
                            //     int logical_w = 0;
                            //     int logical_h = 0;
                            //     SDL_RenderGetLogicalSize(renderer, &logical_w, &logical_h);
                            //     // const SDL_Rect tmpRect = {0, 0, logical_w, logical_h};
                            //     // int x1 = static_cast<int>(std::lround(djoy.center.x));
                            //     // int y1 = static_cast<int>(std::lround(djoy.center.y));
                            //     // int x2 = static_cast<int>(std::lround(finger_in_logical_space.x));
                            //     // int y2 = static_cast<int>(std::lround(finger_in_logical_space.y));
                            //     // SDL_IntersectRectAndLine(&tmpRect, x1, y1, x2, y2);
                            //     // djoy.current.x = std::clamp(x2, 0, logical_w);

                            //     liang_barsky_clipper(
                            //         // rect, top-left:
                            //         0, 0,

                            //         // rect, size:
                            //         static_cast<float>(logical_w), static_cast<float>(logical_h),

                            //         // p1
                            //         djoy.center.x, djoy.center.y,

                            //         // p2
                            //         finger_in_logical_space.x, finger_in_logical_space.y,

                            //         // output p1
                            //         nullptr, nullptr,

                            //         // output p2
                            //         &djoy.current.x, &djoy.current.y
                            //     );
                            // }

                            // SDL_FPoint get_midpoint()
                            const float dx = djoy.current.x - djoy.center.x;
                            const float dy = djoy.current.y - djoy.center.y;
                            const float dlen = std::sqrt((dx * dx) + (dy * dy));
                            // const float maxlen = 40.f;

                            djoy.center = (dlen <= djoy.radius) ?
                                djoy.center :
                                SDL_FPoint {
                                    djoy.current.x - (dx * (djoy.radius / dlen)),
                                    djoy.current.y - (dy * (djoy.radius / dlen)),
                                };

                            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                                "JOY MOVE: {%.2f,%.2f} %.2f", // -> {%f,%f} %f",
                                djoy.current.x,
                                djoy.current.y,
                                dlen
                                // midpt.x,
                                // midpt.y,
                                // dlen2
                            );

                        }
                    }
                    break;
                }
                case SDL_FINGERUP: {
                    if (djoy.fingerId == ev.tfinger.fingerId) {
                        if (djoy.state == DL_Joy::Active) {
                            // stop_joystick
                            const float backup_radius = djoy.radius;
                            memset(&djoy, 0, sizeof(djoy));
                            djoy.radius = backup_radius;
                            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                                "JOY STOP"
                            );
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            // process_paint_events(ev);
        }

        // Clear
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        // SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xff, SDL_ALPHA_OPAQUE);
        SDL_SetRenderDrawColor(renderer, 0x7f, 0x7f, 0x7f, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        // Draw Background
        // SDL_Rect fillRect = {0};
        // SDL_RenderGetLogicalSize(renderer, &fillRect.w, &fillRect.h);
        // SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x10, SDL_ALPHA_OPAQUE);
        // SDL_RenderFillRect(renderer, &fillRect);

        // Draw paint_surface
        // SDL_UpdateTexture(paint_texture, NULL, paint_surface->pixels, paint_surface->pitch);
        // SDL_RenderCopy(renderer, paint_texture, NULL, NULL);

        // Draw joystick debug-state
        if (djoy.state == DL_Joy::Active) {
            SDL_Rect tmpRect = {0, 0, 5, 5};

            tmpRect.x = static_cast<int>(std::lround(djoy.center.x)) - ((tmpRect.w-1)/2);
            tmpRect.y = static_cast<int>(std::lround(djoy.center.y)) - ((tmpRect.h-1)/2);
            SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(renderer, &tmpRect);

            // tmpRect.x = static_cast<int>(std::lround(djoy.mid.x)) - ((tmpRect.w-1)/2);
            // tmpRect.y = static_cast<int>(std::lround(djoy.mid.y)) - ((tmpRect.h-1)/2);
            // SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0xff, SDL_ALPHA_OPAQUE);
            // SDL_RenderFillRect(renderer, &tmpRect);

            tmpRect.x = static_cast<int>(std::lround(djoy.current.x)) - ((tmpRect.w-1)/2);
            tmpRect.y = static_cast<int>(std::lround(djoy.current.y)) - ((tmpRect.h-1)/2);
            SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(renderer, &tmpRect);

            SDL_FPoint circ[32];
            const float pi = std::acos(-1.f);
            const float rad_step = (2.f * pi) / (SDL_arraysize(circ) - 1);
            for (int i = 0; i < SDL_arraysize(circ); ++i) {
                circ[i].x = djoy.center.x + (std::cos(rad_step * i) * djoy.radius);
                circ[i].y = djoy.center.y + (std::sin(rad_step * i) * djoy.radius);
            }
            SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, SDL_ALPHA_OPAQUE);
            SDL_RenderDrawLinesF(renderer, circ, SDL_arraysize(circ));

            SDL_SetRenderDrawColor(renderer, 0xaa, 0xaa, 0xaa, SDL_ALPHA_OPAQUE);
            SDL_RenderDrawLine(renderer,
                static_cast<int>(std::lround(djoy.center.x)),
                static_cast<int>(std::lround(djoy.center.y)),
                static_cast<int>(std::lround(djoy.current.x)),
                static_cast<int>(std::lround(djoy.current.y))
            );
        }

        DrawText(20, 100, DebugText);

        // Present rendered frame
        SDL_RenderPresent(renderer);
    }
}
