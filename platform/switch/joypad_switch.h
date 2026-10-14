/**************************************************************************/
/*  joypad_switch.h                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

//author: Andreas Haas <hondres,  liugam3@gmail.com>
#ifndef JOYPAD_SWITCH_H
#define JOYPAD_SWITCH_H

#include "main/input_default.h"
#include "switch_wrapper.h"

#define JOYPADS_MAX 8
#define JOYPAD_BUTTONS 16

class JoypadSwitch {
public:
	enum Layout {
		LAYOUT_STANDARD,
		LAYOUT_JOYCON_LEFT,
		LAYOUT_JOYCON_RIGHT,
	};

	JoypadSwitch(InputDefault *in);
	~JoypadSwitch();
	void process();
	int show_controller_support(int p_min_players, int p_max_players, bool p_permit_joy_dual);

private:
	struct Slot {
		PadState pad;
		bool connected = false;
		u32 style_set = 0;
		u32 device_type = 0;
		Layout layout = LAYOUT_STANDARD;
		u32 pressed_buttons = 0;
	};

	InputDefault *input;
	Slot slots[JOYPADS_MAX];

	void update_connection(int p_index);
	void update_axes(int p_index);
	void update_buttons(int p_index);
	void release_all(int p_index);
	static Layout detect_layout(u32 p_style_set, u32 p_device_type);
	static String device_name(u32 p_style_set, Layout p_layout);
	static String device_guid(u32 p_style_set, Layout p_layout);
};

#endif // JOYPAD_SWITCH_H
