/**
 * @file gui_theme.c
 * @brief Tema warna terang dan gelap untuk GUI.
 */

#include "gui.h"

static void fill_light(struct nk_color *t, GuiPalette *p)
{
    t[NK_COLOR_TEXT] = nk_rgb(33, 37, 41);
    t[NK_COLOR_WINDOW] = nk_rgb(243, 244, 246);
    t[NK_COLOR_HEADER] = nk_rgb(229, 231, 235);
    t[NK_COLOR_BORDER] = nk_rgb(203, 208, 215);
    t[NK_COLOR_BUTTON] = nk_rgb(255, 255, 255);
    t[NK_COLOR_BUTTON_HOVER] = nk_rgb(236, 240, 246);
    t[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(219, 228, 240);
    t[NK_COLOR_TOGGLE] = nk_rgb(255, 255, 255);
    t[NK_COLOR_TOGGLE_HOVER] = nk_rgb(236, 240, 246);
    t[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(37, 99, 235);
    t[NK_COLOR_SELECT] = nk_rgb(255, 255, 255);
    t[NK_COLOR_SELECT_ACTIVE] = nk_rgb(214, 228, 252);
    t[NK_COLOR_SLIDER] = nk_rgb(222, 226, 232);
    t[NK_COLOR_SLIDER_CURSOR] = nk_rgb(37, 99, 235);
    t[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(29, 78, 216);
    t[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(30, 64, 175);
    t[NK_COLOR_PROPERTY] = nk_rgb(255, 255, 255);
    t[NK_COLOR_EDIT] = nk_rgb(255, 255, 255);
    t[NK_COLOR_EDIT_CURSOR] = nk_rgb(33, 37, 41);
    t[NK_COLOR_COMBO] = nk_rgb(255, 255, 255);
    t[NK_COLOR_CHART] = nk_rgb(255, 255, 255);
    t[NK_COLOR_CHART_COLOR] = nk_rgb(37, 99, 235);
    t[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(220, 38, 38);
    t[NK_COLOR_SCROLLBAR] = nk_rgb(238, 240, 243);
    t[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(196, 202, 210);
    t[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(170, 178, 189);
    t[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(148, 157, 170);
    t[NK_COLOR_TAB_HEADER] = nk_rgb(229, 231, 235);
    t[NK_COLOR_KNOB] = nk_rgb(255, 255, 255);
    t[NK_COLOR_KNOB_CURSOR] = nk_rgb(37, 99, 235);
    t[NK_COLOR_KNOB_CURSOR_HOVER] = nk_rgb(29, 78, 216);
    t[NK_COLOR_KNOB_CURSOR_ACTIVE] = nk_rgb(30, 64, 175);

    p->text = t[NK_COLOR_TEXT];
    p->muted = nk_rgb(107, 114, 128);
    p->accent = nk_rgb(37, 99, 235);
    p->primary = nk_rgb(37, 99, 235);
    p->fired = nk_rgb(21, 128, 61);
    p->failed = nk_rgb(220, 38, 38);
    p->conclusion = nk_rgb(21, 101, 52);
    p->conclusion_bg = nk_rgb(220, 252, 231);
    p->error = nk_rgb(200, 30, 30);
    p->warning = nk_rgb(161, 98, 7);
    p->success = nk_rgb(21, 128, 61);
    p->overlay = nk_rgba(17, 24, 39, 90);
    p->panel = nk_rgb(255, 255, 255);
    p->tab_active = nk_rgb(255, 255, 255);
    p->tab_idle = nk_rgb(229, 231, 235);
    p->status_bg = nk_rgb(229, 231, 235);
}

static void fill_dark(struct nk_color *t, GuiPalette *p)
{
    t[NK_COLOR_TEXT] = nk_rgb(226, 229, 234);
    t[NK_COLOR_WINDOW] = nk_rgb(30, 32, 36);
    t[NK_COLOR_HEADER] = nk_rgb(42, 45, 51);
    t[NK_COLOR_BORDER] = nk_rgb(63, 68, 77);
    t[NK_COLOR_BUTTON] = nk_rgb(48, 52, 59);
    t[NK_COLOR_BUTTON_HOVER] = nk_rgb(58, 63, 72);
    t[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(70, 77, 90);
    t[NK_COLOR_TOGGLE] = nk_rgb(48, 52, 59);
    t[NK_COLOR_TOGGLE_HOVER] = nk_rgb(58, 63, 72);
    t[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(96, 165, 250);
    t[NK_COLOR_SELECT] = nk_rgb(38, 41, 47);
    t[NK_COLOR_SELECT_ACTIVE] = nk_rgb(37, 61, 100);
    t[NK_COLOR_SLIDER] = nk_rgb(48, 52, 59);
    t[NK_COLOR_SLIDER_CURSOR] = nk_rgb(96, 165, 250);
    t[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(147, 197, 253);
    t[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(59, 130, 246);
    t[NK_COLOR_PROPERTY] = nk_rgb(38, 41, 47);
    t[NK_COLOR_EDIT] = nk_rgb(24, 26, 30);
    t[NK_COLOR_EDIT_CURSOR] = nk_rgb(226, 229, 234);
    t[NK_COLOR_COMBO] = nk_rgb(38, 41, 47);
    t[NK_COLOR_CHART] = nk_rgb(38, 41, 47);
    t[NK_COLOR_CHART_COLOR] = nk_rgb(96, 165, 250);
    t[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(248, 113, 113);
    t[NK_COLOR_SCROLLBAR] = nk_rgb(34, 37, 42);
    t[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(70, 76, 86);
    t[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(90, 97, 109);
    t[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(110, 118, 131);
    t[NK_COLOR_TAB_HEADER] = nk_rgb(42, 45, 51);
    t[NK_COLOR_KNOB] = nk_rgb(48, 52, 59);
    t[NK_COLOR_KNOB_CURSOR] = nk_rgb(96, 165, 250);
    t[NK_COLOR_KNOB_CURSOR_HOVER] = nk_rgb(147, 197, 253);
    t[NK_COLOR_KNOB_CURSOR_ACTIVE] = nk_rgb(59, 130, 246);

    p->text = t[NK_COLOR_TEXT];
    p->muted = nk_rgb(148, 155, 166);
    p->accent = nk_rgb(96, 165, 250);
    p->primary = nk_rgb(37, 99, 235);
    p->fired = nk_rgb(74, 222, 128);
    p->failed = nk_rgb(248, 113, 113);
    p->conclusion = nk_rgb(134, 239, 172);
    p->conclusion_bg = nk_rgb(20, 61, 38);
    p->error = nk_rgb(248, 113, 113);
    p->warning = nk_rgb(250, 204, 21);
    p->success = nk_rgb(74, 222, 128);
    p->overlay = nk_rgba(0, 0, 0, 120);
    p->panel = nk_rgb(24, 26, 30);
    p->tab_active = nk_rgb(24, 26, 30);
    p->tab_idle = nk_rgb(42, 45, 51);
    p->status_bg = nk_rgb(42, 45, 51);
}

void gui_theme_apply(Gui *gui, int theme)
{
    struct nk_color table[NK_COLOR_COUNT];
    struct nk_style *s = &gui->ctx.style;

    if (gui->theme == theme) {
        return;
    }
    if (theme == PREF_THEME_DARK) {
        fill_dark(table, &gui->pal);
    } else {
        fill_light(table, &gui->pal);
    }
    nk_style_from_table(&gui->ctx, table);

    /* Geometry */
    s->window.padding = nk_vec2(8, 6);
    s->window.spacing = nk_vec2(6, 6);
    s->window.group_padding = nk_vec2(6, 6);
    s->window.popup_padding = nk_vec2(10, 10);
    s->window.menu_padding = nk_vec2(4, 4);
    s->window.border = 1.0f;
    s->window.group_border = 1.0f;
    s->window.rounding = 4.0f;
    s->window.scrollbar_size = nk_vec2(12, 12);
    s->window.header.padding = nk_vec2(8, 6);
    s->button.rounding = 4.0f;
    s->button.padding = nk_vec2(8, 2);
    s->edit.rounding = 3.0f;
    s->edit.border = 1.0f;
    s->edit.padding = nk_vec2(6, 3);
    s->property.rounding = 3.0f;
    s->combo.rounding = 3.0f;
    s->checkbox.padding = nk_vec2(3, 3);
    s->checkbox.border = 1.0f;
    s->checkbox.border_color = table[NK_COLOR_BORDER];
    s->checkbox.normal = nk_style_item_color(table[NK_COLOR_EDIT]);
    s->checkbox.hover = nk_style_item_color(table[NK_COLOR_TOGGLE_HOVER]);
    s->selectable.rounding = 3.0f;
    s->scrollv.rounding = 4.0f;
    s->scrollv.rounding_cursor = 4.0f;
    s->scrollh.rounding = 4.0f;
    s->scrollh.rounding_cursor = 4.0f;
    s->menu_button.padding = nk_vec2(8, 2);
    s->contextual_button.padding = nk_vec2(8, 4);

    /* Panel Backgrounds */
    s->window.group_border_color = table[NK_COLOR_BORDER];
    s->window.popup_border_color = table[NK_COLOR_BORDER];
    s->window.menu_border_color = table[NK_COLOR_BORDER];
    s->window.border_color = table[NK_COLOR_BORDER];
    s->edit.border_color = table[NK_COLOR_BORDER];
    s->button.border_color = table[NK_COLOR_BORDER];
    s->menu_button.normal = nk_style_item_color(table[NK_COLOR_HEADER]);
    s->menu_button.hover = nk_style_item_color(table[NK_COLOR_BUTTON_ACTIVE]);
    s->menu_button.active = nk_style_item_color(table[NK_COLOR_BUTTON_ACTIVE]);
    s->contextual_button.hover = nk_style_item_color(table[NK_COLOR_SELECT_ACTIVE]);

    gui->theme = theme;
}
