/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - emulate_game_controller_via_input_plugin.c              *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "emulate_game_controller_via_input_plugin.h"
#include "plugin/plugin.h"

#include "api/m64p_plugin.h"
#include "si/game_controller.h"
#include <libretro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ROUND(x)    floor((x) + 0.5)

/* snprintf not available in MSVC 2010 and earlier */
#include "api/msvc_compat.h"

extern retro_environment_t environ_cb;
extern retro_input_state_t input_cb;
extern struct retro_rumble_interface rumble;
extern int pad_pak_types[4];
extern int pad_present[4];
extern int astick_deadzone;
extern int astick_sensitivity;
extern int r_cbutton;
extern int l_cbutton;
extern int d_cbutton;
extern int u_cbutton;

extern m64p_rom_header ROM_HEADER;

// Some stuff from n-rage plugin
#define RD_GETSTATUS        0x00        // get status
#define RD_READKEYS         0x01        // read button values
#define RD_READPAK          0x02        // read from controllerpack
#define RD_WRITEPAK         0x03        // write to controllerpack
#define RD_RESETCONTROLLER  0xff        // reset controller
#define RD_READEEPROM       0x04        // read eeprom
#define RD_WRITEEPROM       0x05        // write eeprom

#define PAK_IO_RUMBLE       0xC000      // the address where rumble-commands are sent to

#define FRAME_DURATION 24

/* global data definitions */
struct
{
    CONTROL *control;               // pointer to CONTROL struct in Core library
    BUTTONS buttons;
} controller[4];

static void inputGetKeys_default( int Control, BUTTONS *Keys );
typedef void (*get_keys_t)(int, BUTTONS*);
static get_keys_t getKeys = inputGetKeys_default;

static void inputGetKeys_default_descriptor(void)
{
   static struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "(C1)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A Button (C3)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Z-Trigger" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "START Button" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up (digital)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down (digital)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left (digital)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right (digital)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "C Buttons Mode" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L-Trigger" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R-Trigger" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "(C4)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "B Button (C2)" },

      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_X, "Control Stick X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_Y, "Control Stick Y" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "C Buttons X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "C Buttons Y" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "(C1)" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A Button (C3)" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Z-Trigger" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "START Button" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up (digital)" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down (digital)" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left (digital)" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right (digital)" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "C Buttons Mode" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L-Trigger" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R-Trigger" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "(C4)" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "B Button (C2)" },

      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_X, "Control Stick X" },
      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_Y, "Control Stick Y" },
      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "C Buttons X" },
      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "C Buttons Y" },

      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "(C1)" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A Button (C3)" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Z-Trigger" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "START Button" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up (digital)" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down (digital)" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left (digital)" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right (digital)" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "C Buttons Mode" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L-Trigger" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R-Trigger" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "(C4)" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "B Button (C2)" },

      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_X, "Control Stick X" },
      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_Y, "Control Stick Y" },
      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "C Buttons X" },
      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "C Buttons Y" },

      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "(C1)" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A Button (C3)" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Z-Trigger" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "START Button" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up (digital)" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down (digital)" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left (digital)" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right (digital)" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "C Buttons Mode" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L-Trigger" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R-Trigger" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "(C4)" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "B Button (C2)" },

      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_X, "Control Stick X" },
      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT , RETRO_DEVICE_ID_ANALOG_Y, "Control Stick Y" },
      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "C Buttons X" },
      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "C Buttons Y" },

      { 0 },
   };
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

/* Mupen64Plus plugin functions */
EXPORT m64p_error CALL inputPluginStartup(m64p_dynlib_handle CoreLibHandle, void *Context,
                                   void (*DebugCallback)(void *, int, const char *))
{
   getKeys = inputGetKeys_default;
   inputGetKeys_default_descriptor();
   return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL inputPluginShutdown(void)
{
    abort();
    return 0;
}

EXPORT m64p_error CALL inputPluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion, int *APIVersion, const char **PluginNamePtr, int *Capabilities)
{
    // This function should never be called in libretro version
    return M64ERR_SUCCESS;
}

static unsigned char DataCRC( unsigned char *Data, int iLenght )
{
    unsigned char Remainder = Data[0];

    int iByte = 1;
    unsigned char bBit = 0;

    while( iByte <= iLenght )
    {
        int HighBit = ((Remainder & 0x80) != 0);
        Remainder = Remainder << 1;

        Remainder += ( iByte < iLenght && Data[iByte] & (0x80 >> bBit )) ? 1 : 0;

        Remainder ^= (HighBit) ? 0x85 : 0;

        bBit++;
        iByte += bBit/8;
        bBit %= 8;
    }

    return Remainder;
}

/******************************************************************
  Function: ControllerCommand
  Purpose:  To process the raw data that has just been sent to a
            specific controller.
  input:    - Controller Number (0 to 3) and -1 signalling end of
              processing the pif ram.
            - Pointer of data to be processed.
  output:   none

  note:     This function is only needed if the DLL is allowing raw
            data, or the plugin is set to raw

            the data that is being processed looks like this:
            initilize controller: 01 03 00 FF FF FF
            read controller:      01 04 01 FF FF FF FF
*******************************************************************/
EXPORT void CALL inputControllerCommand(int Control, unsigned char *Command)
{
    unsigned char *Data = &Command[5];

    if (Control == -1)
        return;

    switch (Command[2])
    {
        case RD_GETSTATUS:
            break;
        case RD_READKEYS:
            break;
        case RD_READPAK:
            if (controller[Control].control->Plugin == PLUGIN_RAW)
            {
                unsigned int dwAddress = (Command[3] << 8) + (Command[4] & 0xE0);

                if(( dwAddress >= 0x8000 ) && ( dwAddress < 0x9000 ) )
                    memset( Data, 0x80, 32 );
                else
                    memset( Data, 0x00, 32 );

                Data[32] = DataCRC( Data, 32 );
            }
            break;
        case RD_WRITEPAK:
            if (controller[Control].control->Plugin == PLUGIN_RAW)
            {
                unsigned int dwAddress = (Command[3] << 8) + (Command[4] & 0xE0);
                Data[32] = DataCRC( Data, 32 );

                if ((dwAddress == PAK_IO_RUMBLE) && (rumble.set_rumble_state))
                {
                    if (*Data)
                    {
                        rumble.set_rumble_state(Control, RETRO_RUMBLE_WEAK, 0xFFFF);
                        rumble.set_rumble_state(Control, RETRO_RUMBLE_STRONG, 0xFFFF);
                    }
                    else
                    {
                        rumble.set_rumble_state(Control, RETRO_RUMBLE_WEAK, 0);
                        rumble.set_rumble_state(Control, RETRO_RUMBLE_STRONG, 0);
                    }
                }
            }

            break;
        case RD_RESETCONTROLLER:
            break;
        case RD_READEEPROM:
            break;
        case RD_WRITEEPROM:
            break;
        }
}

/******************************************************************
  Function: GetKeys
  Purpose:  To get the current state of the controllers buttons.
  input:    - Controller Number (0 to 3)
            - A pointer to a BUTTONS structure to be filled with
            the controller state.
  output:   none
*******************************************************************/

// System analog stick range is -0x8000 to 0x8000
#define ASTICK_MAX 0x8000
#define CSTICK_DEADZONE 0x4000

#define CSTICK_RIGHT 0x200
#define CSTICK_LEFT 0x100
#define CSTICK_UP 0x800
#define CSTICK_DOWN 0x400

int timeout = 0;

extern void inputInitiateCallback(const char *headername);


static void inputGetKeys_reuse(int16_t analogX, int16_t analogY, int Control, BUTTONS* Keys)
{
   double radius, angle;
   //  Keys->Value |= input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_XX)    ? 0x4000 : 0; // Mempak switch
   //  Keys->Value |= input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_XX)    ? 0x8000 : 0; // Rumblepak switch

   analogX = input_cb(Control, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
   analogY = input_cb(Control, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

   // Convert cartesian coordinate analog stick to polar coordinates
   radius = sqrt(analogX * analogX + analogY * analogY);
   angle = atan2(analogY, analogX);

   if (radius > astick_deadzone)
   {
      // Re-scale analog stick range to negate deadzone (makes slow movements possible)
      radius = (radius - astick_deadzone)*((float)ASTICK_MAX/(ASTICK_MAX - astick_deadzone));
      // N64 Analog stick range is from -80 to 80
      radius *= 120.0 / ASTICK_MAX * (astick_sensitivity / 100.0);
      // Convert back to cartesian coordinates
      Keys->X_AXIS = +(int32_t)ROUND(radius * cos(angle));
      Keys->Y_AXIS = -(int32_t)ROUND(radius * sin(angle));
   }
   else
   {
      Keys->X_AXIS = 0;
      Keys->Y_AXIS = 0;
   }
}

static void inputGetKeys_default( int Control, BUTTONS *Keys )
{
   bool hold_cstick = false;
   int16_t analogX = 0;
   int16_t analogY = 0;
   Keys->Value = 0;

   Keys->R_DPAD = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   Keys->L_DPAD = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
   Keys->D_DPAD = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   Keys->U_DPAD = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);

   Keys->START_BUTTON = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);

   Keys->R_TRIG = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R);
   Keys->L_TRIG = input_cb(Control, RETRO_DEVICE_JOYPAD, 0,   RETRO_DEVICE_ID_JOYPAD_L);
   Keys->Z_TRIG = input_cb(Control, RETRO_DEVICE_JOYPAD, 0,   RETRO_DEVICE_ID_JOYPAD_L2);

   hold_cstick = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);
   if (hold_cstick)
   {
      Keys->R_CBUTTON = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, r_cbutton);
      Keys->L_CBUTTON = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, l_cbutton);
      Keys->D_CBUTTON = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, d_cbutton);
      Keys->U_CBUTTON = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, u_cbutton);
   }
   else
   {
      Keys->B_BUTTON = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
      Keys->A_BUTTON = input_cb(Control, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
   }

   // C buttons
   analogX = input_cb(Control, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
   analogY = input_cb(Control, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

   if (abs(analogX) > CSTICK_DEADZONE)
      Keys->Value |= (analogX < 0) ? CSTICK_RIGHT : CSTICK_LEFT;

   if (abs(analogY) > CSTICK_DEADZONE)
      Keys->Value |= (analogY < 0) ? CSTICK_UP : CSTICK_DOWN;

   inputGetKeys_reuse(analogX, analogY, Control, Keys);
}

void inputInitiateCallback(const char *headername)
{
   struct retro_message msg;
   char msg_local[256];

   if (getKeys != &inputGetKeys_default)
   {
      getKeys = inputGetKeys_default;
      inputGetKeys_default_descriptor();
      snprintf(msg_local, sizeof(msg_local), "Controls: Default");
      msg.msg = msg_local;
      msg.frames = FRAME_DURATION;
      timeout = FRAME_DURATION / 2;
      if (environ_cb)
         environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, (void*)&msg);
      return;
   }
}


/******************************************************************
  Function: InitiateControllers
  Purpose:  This function initialises how each of the controllers
            should be handled.
  input:    - The handle to the main window.
            - A controller structure that needs to be filled for
              the emulator to know how to handle each controller.
  output:   none
*******************************************************************/
EXPORT void CALL inputInitiateControllers(CONTROL_INFO ControlInfo)
{
    int i;

    for( i = 0; i < 4; i++ )
    {
       controller[i].control = ControlInfo.Controls + i;
       controller[i].control->Present = pad_present[i];
       controller[i].control->RawData = 0;

       if (pad_pak_types[i] == PLUGIN_MEMPAK)
          controller[i].control->Plugin = PLUGIN_MEMPAK;
       else if (pad_pak_types[i] == PLUGIN_RAW)
          controller[i].control->Plugin = PLUGIN_RAW;
       else
          controller[i].control->Plugin = PLUGIN_NONE;
    }

   getKeys = inputGetKeys_default;
   inputGetKeys_default_descriptor();
}

/******************************************************************
  Function: ReadController
  Purpose:  To process the raw data in the pif ram that is about to
            be read.
  input:    - Controller Number (0 to 3) and -1 signalling end of
              processing the pif ram.
            - Pointer of data to be processed.
  output:   none
  note:     This function is only needed if the DLL is allowing raw
            data.
*******************************************************************/
EXPORT void CALL inputReadController(int Control, unsigned char *Command)
{
   inputControllerCommand(Control, Command);
}

/******************************************************************
  Function: RomClosed
  Purpose:  This function is called when a rom is closed.
  input:    none
  output:   none
*******************************************************************/
EXPORT void CALL inputRomClosed(void) { }

/******************************************************************
  Function: RomOpen
  Purpose:  This function is called when a rom is open. (from the
            emulation thread)
  input:    none
  output:   none
*******************************************************************/
EXPORT int CALL inputRomOpen(void) { return 1; }


int egcvip_is_connected(void* opaque, enum pak_type* pak)
{
    int channel = *(int*)opaque;

    CONTROL* c = &Controls[channel];

    switch(c->Plugin)
    {
    case PLUGIN_NONE: *pak = PAK_NONE; break;
    case PLUGIN_MEMPAK: *pak = PAK_MEM; break;
    case PLUGIN_RUMBLE_PAK: *pak = PAK_RUMBLE; break;
    case PLUGIN_TRANSFER_PAK: *pak = PAK_TRANSFER; break;

    case PLUGIN_RAW:
        /* historically PLUGIN_RAW has been mostly (exclusively ?) used for rumble,
         * so we just reproduce that behavior */
        *pak = PAK_RUMBLE; break;
    }

    return c->Present;
}

uint32_t egcvip_get_input(void* opaque)
{
    BUTTONS keys = { 0 };
    int channel = *(int*)opaque;

    if (getKeys)
       getKeys(channel, &keys);

    return keys.Value;

}
