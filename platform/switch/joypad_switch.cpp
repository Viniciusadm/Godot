/**************************************************************************/
/*  joypad_switch.cpp                                                     */
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

#include "joypad_switch.h"

static const u64 pad_ids[JOYPADS_MAX] = {
	(1ull << HidNpadIdType_No1) | (1ull << HidNpadIdType_Handheld),
	(1ull << HidNpadIdType_No2),
	(1ull << HidNpadIdType_No3),
	(1ull << HidNpadIdType_No4),
	(1ull << HidNpadIdType_No5),
	(1ull << HidNpadIdType_No6),
	(1ull << HidNpadIdType_No7),
	(1ull << HidNpadIdType_No8)
};

// from editor "Project Settings > Input Map"
static const u64 pad_mapping[JOYPAD_BUTTONS] = {
	HidNpadButton_B, HidNpadButton_A, HidNpadButton_Y, HidNpadButton_X,
	HidNpadButton_L, HidNpadButton_R, HidNpadButton_ZL, HidNpadButton_ZR,
	HidNpadButton_StickL, HidNpadButton_StickR,
	HidNpadButton_Minus, HidNpadButton_Plus,
	HidNpadButton_Up, HidNpadButton_Down, HidNpadButton_Left, HidNpadButton_Right
};

static const u64 joycon_left_mapping[JOYPAD_BUTTONS] = {
	HidNpadButton_Left, HidNpadButton_Down, HidNpadButton_Up, HidNpadButton_Right,
	HidNpadButton_LeftSL, HidNpadButton_LeftSR, 0, 0,
	HidNpadButton_StickL, 0,
	0, HidNpadButton_Minus,
	0, 0, 0, 0
};

static const u64 joycon_right_mapping[JOYPAD_BUTTONS] = {
	HidNpadButton_A, HidNpadButton_X, HidNpadButton_B, HidNpadButton_Y,
	HidNpadButton_RightSL, HidNpadButton_RightSR, 0, 0,
	HidNpadButton_StickR, 0,
	0, HidNpadButton_Plus,
	0, 0, 0, 0
};

static const u64 *mapping_for(JoypadSwitch::Layout p_layout) {
	switch (p_layout) {
		case JoypadSwitch::LAYOUT_JOYCON_LEFT:
			return joycon_left_mapping;
		case JoypadSwitch::LAYOUT_JOYCON_RIGHT:
			return joycon_right_mapping;
		default:
			return pad_mapping;
	}
}

static float normalize_stick(s32 p_value) {
	return CLAMP(p_value / (float)JOYSTICK_MAX, -1.0f, 1.0f);
}

JoypadSwitch::JoypadSwitch(InputDefault *in) {
	input = in;

	padConfigureInput(JOYPADS_MAX, HidNpadStyleSet_NpadStandard);
	hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);

	for (int i = 0; i < JOYPADS_MAX; i++) {
		padInitializeWithMask(&slots[i].pad, pad_ids[i]);
	}
}

JoypadSwitch::~JoypadSwitch() {
}

void JoypadSwitch::process() {
	for (int index = 0; index < JOYPADS_MAX; index++) {
		padUpdate(&slots[index].pad);
		update_connection(index);
		if (!slots[index].connected) {
			continue;
		}
		update_axes(index);
		update_buttons(index);
	}
}

int JoypadSwitch::show_controller_support(int p_min_players, int p_max_players, bool p_permit_joy_dual) {
	HidLaControllerSupportArg arg;
	hidLaCreateControllerSupportArg(&arg);
	arg.hdr.player_count_max = CLAMP(p_max_players, 1, JOYPADS_MAX);
	arg.hdr.player_count_min = CLAMP(p_min_players, 0, arg.hdr.player_count_max);
	arg.hdr.enable_permit_joy_dual = p_permit_joy_dual;
	arg.hdr.enable_single_mode = arg.hdr.player_count_max == 1;

	HidLaControllerSupportResultInfo info;
	Result rc = hidLaShowControllerSupportForSystem(&info, &arg, false);

	for (int index = 0; index < JOYPADS_MAX; index++) {
		release_all(index);
		slots[index].style_set = 0;
		slots[index].device_type = 0;
	}

	if (R_FAILED(rc)) {
		return -1;
	}
	return info.player_count;
}

void JoypadSwitch::update_connection(int p_index) {
	Slot &slot = slots[p_index];
	bool connected = padIsConnected(&slot.pad);

	if (!connected) {
		if (slot.connected) {
			release_all(p_index);
			slot.connected = false;
			slot.style_set = 0;
			slot.device_type = 0;
			input->joy_connection_changed(p_index, false, "", "");
		}
		return;
	}

	u32 style_set = padGetStyleSet(&slot.pad);
	u32 device_type = padIsHandheld(&slot.pad) ? 0 : hidGetNpadDeviceType((HidNpadIdType)(HidNpadIdType_No1 + p_index));
	if (slot.connected && style_set == slot.style_set && device_type == slot.device_type) {
		return;
	}

	release_all(p_index);
	slot.connected = true;
	slot.style_set = style_set;
	slot.device_type = device_type;
	slot.layout = detect_layout(style_set, device_type);
	input->joy_connection_changed(p_index, true, device_name(style_set, slot.layout), device_guid(style_set, slot.layout));
}

void JoypadSwitch::update_axes(int p_index) {
	Slot &slot = slots[p_index];
	HidAnalogStickState l_stick = padGetStickPos(&slot.pad, 0);
	HidAnalogStickState r_stick = padGetStickPos(&slot.pad, 1);

	switch (slot.layout) {
		case LAYOUT_JOYCON_LEFT:
			input->joy_axis(p_index, 0, normalize_stick(-l_stick.y));
			input->joy_axis(p_index, 1, normalize_stick(-l_stick.x));
			break;
		case LAYOUT_JOYCON_RIGHT:
			input->joy_axis(p_index, 0, normalize_stick(r_stick.y));
			input->joy_axis(p_index, 1, normalize_stick(r_stick.x));
			break;
		default:
			input->joy_axis(p_index, 0, normalize_stick(l_stick.x));
			input->joy_axis(p_index, 1, normalize_stick(-l_stick.y));
			input->joy_axis(p_index, 2, normalize_stick(r_stick.x));
			input->joy_axis(p_index, 3, normalize_stick(-r_stick.y));
			break;
	}
}

void JoypadSwitch::update_buttons(int p_index) {
	Slot &slot = slots[p_index];
	const u64 *mapping = mapping_for(slot.layout);
	u64 held = padGetButtons(&slot.pad);

	u32 pressed = 0;
	for (int i = 0; i < JOYPAD_BUTTONS; i++) {
		if (mapping[i] != 0 && (held & mapping[i])) {
			pressed |= 1u << i;
		}
	}

	u32 changed = pressed ^ slot.pressed_buttons;
	for (int i = 0; i < JOYPAD_BUTTONS; i++) {
		if (changed & (1u << i)) {
			input->joy_button(p_index, i, (pressed & (1u << i)) != 0);
		}
	}
	slot.pressed_buttons = pressed;
}

void JoypadSwitch::release_all(int p_index) {
	Slot &slot = slots[p_index];
	for (int i = 0; i < JOYPAD_BUTTONS; i++) {
		if (slot.pressed_buttons & (1u << i)) {
			input->joy_button(p_index, i, false);
		}
	}
	slot.pressed_buttons = 0;
	for (int axis = 0; axis < 4; axis++) {
		input->joy_axis(p_index, axis, 0.0f);
	}
}

JoypadSwitch::Layout JoypadSwitch::detect_layout(u32 p_style_set, u32 p_device_type) {
	if (p_style_set & (HidNpadStyleTag_NpadHandheld | HidNpadStyleTag_NpadJoyDual | HidNpadStyleTag_NpadFullKey)) {
		return LAYOUT_STANDARD;
	}
	if ((p_style_set & HidNpadStyleTag_NpadJoyLeft) || (p_device_type & HidDeviceTypeBits_JoyLeft)) {
		return LAYOUT_JOYCON_LEFT;
	}
	if ((p_style_set & HidNpadStyleTag_NpadJoyRight) || (p_device_type & HidDeviceTypeBits_JoyRight)) {
		return LAYOUT_JOYCON_RIGHT;
	}
	return LAYOUT_STANDARD;
}

String JoypadSwitch::device_name(u32 p_style_set, Layout p_layout) {
	if (p_layout == LAYOUT_JOYCON_LEFT) {
		return "Joy-Con (L)";
	}
	if (p_layout == LAYOUT_JOYCON_RIGHT) {
		return "Joy-Con (R)";
	}
	if (p_style_set & HidNpadStyleTag_NpadHandheld) {
		return "Nintendo Switch Handheld";
	}
	if (p_style_set & HidNpadStyleTag_NpadJoyDual) {
		return "Joy-Con (L/R)";
	}
	if (p_style_set & HidNpadStyleTag_NpadFullKey) {
		return "Pro Controller";
	}
	return "Nintendo Switch Controller";
}

String JoypadSwitch::device_guid(u32 p_style_set, Layout p_layout) {
	if (p_layout == LAYOUT_JOYCON_LEFT) {
		return "__switch_joycon_left";
	}
	if (p_layout == LAYOUT_JOYCON_RIGHT) {
		return "__switch_joycon_right";
	}
	if (p_style_set & HidNpadStyleTag_NpadHandheld) {
		return "__switch_handheld";
	}
	if (p_style_set & HidNpadStyleTag_NpadJoyDual) {
		return "__switch_joycon_dual";
	}
	if (p_style_set & HidNpadStyleTag_NpadFullKey) {
		return "__switch_pro_controller";
	}
	return "__switch_controller";
}
