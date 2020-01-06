#include "immgui.h"
#include "../renderer/renderer_imm.h"
#include "immgui_input.h"

const u32 IMMCTX_HOT_SET = (1 << 0);
const u32 IMMCTX_ACTIVE_SET = (1 << 0);

static bool active(HG_Context* ctx, int id) {
    return (ctx->active.owner == id);
}

static bool hot(HG_Context* ctx, int id) {
    return (ctx->hot.owner == id);
}

static void reset_active(HG_Context* ctx) {
    ctx->active.owner = -1;
}

static void set_active(HG_Context* ctx, int id) {
    ctx->active.owner = id;
    ctx->flags |= IMMCTX_ACTIVE_SET;
}

static void set_hot(HG_Context* ctx, int id) {
    ctx->hot.owner = id;
    ctx->flags |= IMMCTX_HOT_SET;
}

static void reset_hot(HG_Context* ctx) {
    ctx->hot.owner = -1;
}

void hg_end_frame(HG_Context* ctx) {
    if(!(ctx->flags & IMMCTX_HOT_SET))
        reset_hot(ctx);

    ctx->flags = 0;
}

bool hg_do_button(HG_Context* ctx, int id, const char* text) {
    bool result = false;
    if(active(ctx, id)) {
        if(input_mouse_button_went_up(MOUSE_LEFT_BUTTON, 0, 0)) {
            if(hot(ctx, id)) result = true;
            reset_active(ctx);
        }
    } else if(hot(ctx, id)) {
        if(input_mouse_button_went_down(MOUSE_LEFT_BUTTON, 0, 0)) {
            set_active(ctx,id);
        }
    }

    r32 width = 100.0f;
    r32 height = 25.0f;
    vec2 position = (vec2){10.0f, 10.0f};

    u32 flags = 0;
    if(input_inside(input_mouse_position(), (vec4){position.x, position.y, width, height})) {
        set_hot(ctx, id);
    }

    // Draw
    Quad_2D button_quad = {0};
    if(active(ctx, id)) {
        button_quad = quad_new(position, width, height, (vec4){1.0f, 0.3f, 0.3f, 1.0f});
    } else if(hot(ctx, id)) {
        button_quad = quad_new(position, width, height, (vec4){1.0f, 0.4f, 0.4f, 1.0f});
    } else {
        button_quad = quad_new(position, width, height, (vec4){0.7f, 0.3f, 0.3f, 1.0f});
    }
    renderer_imm_quad(&button_quad);

    r32 button_border_width[] = {2.0f, 2.0f, 2.0f, 2.0f};
    vec4 button_border_color[] = {(vec4){0.3f, 0.3f, 0.3f, 1.0f}, (vec4){0.5f, 0.5f, 0.5f, 1.0f}, (vec4){0.3f, 0.3f, 0.3f, 1.0f}, (vec4){0.5f, 0.5f, 0.5f, 1.0f}};
    renderer_imm_border(&button_quad, button_border_width, button_border_color);

    extern Font_Info font_info;

    Text_Render_Info info = text_prerender(&font_info, "Hello", sizeof("Hello")-1, 0, 0);
    vec2 text_position = (vec2){position.x + (width - info.width) / 2.0f, position.y + (height - info.height) / 2.0f };
    
    text_render(&font_info, "Hello", sizeof("Hello")-1, text_position, font_render_no_clipping());

    return result;
}

static int length_current_utf8(char* buffer) {
    int l = 1;
    while (* buffer && l < 4 && ((*buffer) & 0xc0) == 0x80) {
        buffer--;
        l++;
    }
    return l;
}

static int length_next_utf8(char* buffer, int max) {
    int l = 1;
    buffer++; // skip first
    while (*buffer && l < max && ((*buffer) & 0xc0) == 0x80) {
        buffer++;
        l++;
    }
    return l;
}

static int delete_selection_forward(char* buffer, int* cursor_index, int* selection_distance, int* buffer_length) {
    if(*selection_distance > 0) {
        memcpy(buffer + *cursor_index, buffer + *cursor_index + *selection_distance, *buffer_length - (*cursor_index + *selection_distance));
        *buffer_length -= (*selection_distance);
        *selection_distance = 0;
    } else if(*selection_distance < 0) {
        memcpy(buffer + *cursor_index + *selection_distance, buffer + *cursor_index, *buffer_length - *cursor_index);
        *buffer_length += (*selection_distance);
        *cursor_index += (*selection_distance);
        *selection_distance = 0;
    } else {
        if(*cursor_index < *buffer_length) {
            int l = length_next_utf8(buffer + *cursor_index, *buffer_length - *cursor_index);
            memcpy(buffer + *cursor_index, buffer + *cursor_index + l, *buffer_length - *cursor_index - l);
            *buffer_length -= l;
            return l;
        }
    }
    return 0;
}
static int delete_selection_backward(char* buffer, int* cursor_index, int* selection_distance, int* buffer_length) {
    if(*selection_distance > 0) {
        memcpy(buffer + *cursor_index, buffer + *cursor_index + *selection_distance, *buffer_length - (*cursor_index + *selection_distance));
        *buffer_length -= (*selection_distance);
        *selection_distance = 0;
    } else if(*selection_distance < 0) {
        memcpy(buffer + *cursor_index + *selection_distance, buffer + *cursor_index, *buffer_length - *cursor_index);
        *buffer_length += (*selection_distance);
        *cursor_index += (*selection_distance);
        *selection_distance = 0;
    } else {
        if (*buffer_length > 0 && *cursor_index > 0) {
            int l = length_current_utf8(buffer + *cursor_index - 1);
            memcpy(buffer + *cursor_index - l, buffer + *cursor_index, *buffer_length - *cursor_index);
            (*buffer_length) -= l;
            (*cursor_index) -= l;
        }
    }
    return 0;
}

static int insert_text(const char* clip, int clipboard_length, char* buffer, int buffer_max_length, int* cursor_index, int* selection_distance, int* buffer_length) {
    int insert_max = buffer_max_length - *buffer_length + (*selection_distance < 0 ? -(*selection_distance) : *selection_distance);
    if(*selection_distance == 0) {
        
        int inserted_length = MIN(insert_max, clipboard_length);
        memcpy(buffer + *cursor_index + inserted_length, buffer + *cursor_index, inserted_length);
        memcpy(buffer + *cursor_index, clip, inserted_length);
        *cursor_index += inserted_length;
        *buffer_length += inserted_length;
        return inserted_length;
    }
    return 0;
}

// selection_distance == 0 means no selection is active
bool hg_do_input(HG_Context* ctx, int id, char* buffer, int buffer_max_length, int* buffer_length, int* cursor_index, int* selection_distance) {
    bool result = true;
    bool reset_selection = true;

    if(active(ctx, id)) {
        u32 key = 0;
        s32 mods = 0;
        char utf8_key[4] = {0};
        while(input_next_key_pressed(&key, &mods)) {
            
            switch(key) {
                case GLFW_KEY_END:{
                    if(mods & GLFW_MOD_SHIFT) {
                        *selection_distance = -(*buffer_length - *cursor_index - *selection_distance);
                    }
                    *cursor_index = *buffer_length;
                } break;
                case GLFW_KEY_HOME: {
                    if(mods & GLFW_MOD_SHIFT) {
                        *selection_distance = *cursor_index + *selection_distance;
                    }
                    *cursor_index = 0;
                } break;
                case GLFW_KEY_LEFT: {
                    if(!(mods & GLFW_MOD_SHIFT)) {
                        if(*selection_distance > 0) {
                            *selection_distance = 0;
                            break;
                        } else if(*selection_distance < 0) {
                            *cursor_index += *selection_distance;
                            *selection_distance = 0;
                            break;
                        }
                    }
                    if(*cursor_index > 0) {
                        int l = length_current_utf8(buffer + *cursor_index - 1);
                        (*cursor_index) -= l;
                        if(mods & GLFW_MOD_SHIFT) {
                            *selection_distance += l;
                        }
                    }
                } break;
                case GLFW_KEY_RIGHT: {
                    if(!(mods & GLFW_MOD_SHIFT)) {
                        if(*selection_distance > 0) {
                            *cursor_index += *selection_distance;
                            *selection_distance = 0;
                            break;
                        } else if(*selection_distance < 0) {
                            *selection_distance = 0;
                            break;
                        }
                    }
                    if(*cursor_index < *buffer_length) {
                        int l = length_next_utf8(buffer + *cursor_index, *buffer_length - *cursor_index);
                        (*cursor_index) += l;
                        if(mods & GLFW_MOD_SHIFT) {
                            *selection_distance -= 1;
                        }
                    }
                } break;
                case GLFW_KEY_DELETE: {
                    delete_selection_forward(buffer, cursor_index, selection_distance, buffer_length);
                } break;
                case GLFW_KEY_BACKSPACE: {
                    delete_selection_backward(buffer, cursor_index, selection_distance, buffer_length);
                } break;
                case GLFW_KEY_ENTER: {
                    reset_active(ctx);
                } break;
                default: {
                    
                    if(mods & GLFW_MOD_CONTROL) {
                        if(key == 'V') {
                            if (*selection_distance != 0) {
                                delete_selection_forward(buffer, cursor_index, selection_distance, buffer_length);
                            }
                            const char* clip = input_get_clipboard();
                            int clipboard_length = strlen(clip);
                            insert_text(clip, clipboard_length, buffer, buffer_max_length, cursor_index, selection_distance, buffer_length);
                        }
                        if(key == 'C') {
                            if(*selection_distance > 0) {
                                input_set_clipboard(buffer + *cursor_index, *selection_distance);
                            } else if(*selection_distance < 0) {
                                input_set_clipboard(buffer + *cursor_index + *selection_distance, -(*selection_distance));
                            }
                            reset_selection = false;
                        }
                        break;
                    }
                    
                    if (*selection_distance != 0) {
                        delete_selection_forward(buffer, cursor_index, selection_distance, buffer_length);
                    }

                    int length = ustring_unicode_to_utf8(key, utf8_key);
                    if(*buffer_length + length < buffer_max_length) {
                        memcpy(buffer + *cursor_index + length, buffer + *cursor_index, *buffer_length - *cursor_index);
                        memcpy(buffer + *cursor_index, utf8_key, length);
                        (*cursor_index) += length;
                        (*buffer_length) += length;
                    } else {
                        result = false;
                    }
                } break;
            }

            if (!(mods & GLFW_MOD_SHIFT) && reset_selection) {
                *selection_distance = 0;
            }
        }
    } else if(hot(ctx, id)) {
        if(input_mouse_button_went_down(MOUSE_LEFT_BUTTON, 0, 0)) {
            set_active(ctx,id);
        }
    }

    vec2 position = (vec2){200, 10};
    r32 width = 200.0f;
    r32 height = 28.0f;

    if(input_inside(input_mouse_position(), (vec4){position.x, position.y, width, height})) {
        set_hot(ctx, id);
    }

    // -------------------------------------------
    // ---------------- Draw ---------------------
    // -------------------------------------------
    vec4 color = (vec4){0.4f, 0.4f, 0.38f, 1.0f};
    if(active(ctx, id)) {
        color = (vec4){0.3f, 0.3f, 0.3f, 1.0f};
    } else if (hot(ctx, id)) {
        color = (vec4){0.4f, 0.4f, 0.3f, 1.0f};
    }
    Quad_2D q = quad_new(position, width, height, color);
    renderer_imm_quad(&q);
    r32 input_border_width[] = {2.0f, 2.0f, 2.0f, 2.0f};
    vec4 input_border_color[] = {(vec4){0.2f, 0.2f, 0.2f, 1.0f}, (vec4){0.6f, 0.6f, 0.6f, 1.0f}, (vec4){0.5f, 0.5f, 0.5f, 1.0f}, (vec4){0.2f, 0.2f, 0.2f, 1.0f}};
    renderer_imm_outside_border(&q, input_border_width, input_border_color);

    extern Font_Info font_info;
    // Pre-render text
    Text_Render_Character_Position cursor_pos[2] = {0};
    int cindex = 0;
    int sindex = 0;
    if (*selection_distance < 0) {
        cindex++;
    } else {
        sindex++;
    }
    cursor_pos[cindex].index = *cursor_index;
    cursor_pos[sindex].index = *cursor_index + *selection_distance;

    Text_Render_Info info = text_prerender(&font_info, buffer, *buffer_length, cursor_pos, 2);
    vec2 text_position = (vec2){position.x + 2.0f, position.y + (height - font_info.max_height) / 2.0f };

    // If the cursor is outside the view, bring it into view
    if(cursor_pos[cindex].position.x > width) {
        text_position.x -= (cursor_pos[cindex].position.x - width + font_info.max_width + info.width - cursor_pos[cindex].position.x);
    }

    // Render text
    Clipping_Rect clipping = (Clipping_Rect) { position.x, position.y, width, height };
    text_render(&font_info, buffer, *buffer_length, text_position, clipping);

    if(active(ctx, id)) {
        Quad_2D cursor_quad = quad_new_clipped((vec2){cursor_pos[cindex].position.x + text_position.x + 1.0f,text_position.y - 3.0f},
            1.0f, font_info.max_height, (vec4){1.0f, 1.0f, 1.0f, 1.0f}, clipping);
        renderer_imm_quad(&cursor_quad);

        // Render Selection box if one exists
        if(*selection_distance != 0) {
            r32 selection_width = 0.0f;
            int min_index = 0;
            if(*selection_distance < 0) {
                selection_width = cursor_pos[cindex].position.x - cursor_pos[sindex].position.x;
                min_index = sindex;
            } else {
                selection_width = cursor_pos[sindex].position.x - cursor_pos[cindex].position.x;
                min_index = cindex;
            }
            vec4 sel_box_color = (vec4){0.84f, 0.84f, 0.84f, 0.5f};
            Quad_2D select_box_quad = quad_new((vec2){cursor_pos[min_index].position.x + text_position.x, text_position.y - 3.0f}, selection_width, font_info.max_height, (vec4){0.84f, 0.84f, 0.84f, 0.2f});
            Quad_2D select_box_quad_clipped = quad_new_clipped((vec2){cursor_pos[min_index].position.x + text_position.x, text_position.y - 3.0f}, selection_width, font_info.max_height, (vec4){0.84f, 0.84f, 0.84f, 0.2f}, clipping);
            renderer_imm_quad(&select_box_quad_clipped);
            r32 border_width[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            vec4 colors[] = {sel_box_color, sel_box_color, sel_box_color, sel_box_color};
            renderer_imm_border_clipped(&select_box_quad, border_width, colors, clipping);
        }
    }

    return result;
}

void hg_window_begin(HG_Context* ctx, int id, vec2 position, r32 width, r32 height, const char* name) {
    Quad_2D q = quad_new(position, width, height, (vec4){0.5f, 0.5f, 0.5f, 1.0f});
    renderer_imm_quad(&q);
}