/*
 * Copyright 2007      by Sascha Hlusiak. <saschahlusiak@freedesktop.org>     
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Sascha   Hlusiak  not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Sascha   Hlusiak   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * SASCHA  HLUSIAK  DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL SASCHA  HLUSIAK  BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xf86Xinput.h>
#include <xf86_OSproc.h>
#include <math.h>
#include <stdlib.h>
/* #include <xf86.h> */

#include "jstk.h"
#include "jstk_axis.h"


/***********************************************************************
 *
 * jstkAxisTimer --
 *
 * The timer that will generate PointerMove-events. Checks every axis
 * and every button for it's mapping.
 * Return 0, when timer can be stopped, because there is no active
 * movement
 *
 ***********************************************************************
 */
static CARD32
jstkAxisTimer(OsTimerPtr        timer,
              CARD32            atime,
              pointer           arg)
{
#define NEXTTIMER 15
  DeviceIntPtr          device = (DeviceIntPtr)arg;
  JoystickDevPtr        priv = (JoystickDevPtr)XI_PRIVATE(device);

  int sigstate, i;
  int nexttimer;
  nexttimer = 0;

  sigstate = xf86BlockSIGIO();

  for (i=0; i<MAXAXES; i++) if ((priv->axis[i].value != 0) &&
                                (priv->axis[i].type != TYPE_NONE)) {
    float p1 = 0.0f;     /* Pixels to move cursor */
    float p2 = 0.0f;     /* Pixels to scroll */
    float scale;
    AXIS *axis;
    axis = &priv->axis[i];

    nexttimer = NEXTTIMER;

    if (priv->axis[i].type == TYPE_BYVALUE) {
      /* Calculate scale value, so we still get a range from 0 to 32768 */
      scale = (32768.0f / (float)(32768 - axis->deadzone));

      /* How many pixels should this axis move the cursor */
      p1 = ((pow((abs((float)axis->value) - (float)axis->deadzone) *
             scale / 1700.0f, 3.4f)) + 100.0f) *
            ((float)NEXTTIMER / 40000.0f);
      /* How many "pixels" should this axis scroll */
      p2 = ((pow((abs((float)axis->value) - (float)axis->deadzone) *
             scale / 1000.0f, 2.5f)) + 200.0f) *
            ((float)NEXTTIMER / 200000.0f);


    } else if (axis->type == TYPE_ACCELERATED) {
      /* Stop to accelerate at a certain speed */
      if (axis->currentspeed < 100.0f) axis->currentspeed *= 1.15f;

      p1 = (axis->currentspeed - 0.1f) * (float)NEXTTIMER / 180.0f;
      p2 = p1 / 8.0f;
    }
    if (axis->value < 0) {
      p1 = -p1;
      p2 = -p2;
    }
    p1 *= axis->amplify * priv->amplify;
    p2 *= axis->amplify * priv->amplify;

    /* Apply movement to global amount of pixels to move */
    switch (axis->mapping) {
      case MAPPING_X:
        priv->x += p1;
        break;
      case MAPPING_Y:
        priv->y += p1;
        break;
      case MAPPING_ZX:
        priv->zx += p2;
        break;
      case MAPPING_ZY:
        priv->zy += p2;
        break;
      default:
        break;
    }
  }

  for (i=0; i<MAXBUTTONS; i++) if (priv->button[i].pressed == 1) {
    float p1;
    float p2;

    if (priv->button[i].currentspeed < 100.0f) priv->button[i].currentspeed *= 1.15f;
    p1 = (priv->button[i].currentspeed - 0.1) * (float)NEXTTIMER / 180.0f *
         priv->button[i].amplify;
    p1 *= priv->amplify;
    p2 = p1 / 8.0f;

    /* Apply movement to global amount of pixels to move */
    switch (priv->button[i].mapping) {
      case MAPPING_X:
        priv->x += p1;
        nexttimer = NEXTTIMER;
        break;
      case MAPPING_Y:
        priv->y += p1;
        nexttimer = NEXTTIMER;
        break;
      case MAPPING_ZX:
        priv->zx += p2;
        nexttimer = NEXTTIMER;
        break;
      case MAPPING_ZY:
        priv->zy += p2;
        nexttimer = NEXTTIMER;
        break;
      default:
        break;
    }
  }

  /* Actually move the cursor, if there is enough movement in the buffer */
  if (((int)priv->x != 0)||((int)priv->y != 0)) {
    xf86PostMotionEvent(device, 0, 0, 2, (int)priv->x, (int)priv->y);
    priv->x = priv->x - (int)priv->x;
    priv->y = priv->y - (int)priv->y;
  }

  /* Generate scrolling events */
  while (priv->zy >= 1.0) {  /* down */
    xf86PostButtonEvent(device, 0, 5, 1, 0, 0);
    xf86PostButtonEvent(device, 0, 5, 0, 0, 0);
    priv->zy-=1.0;
  }
  while (priv->zy <= -1.0) { /* up */
    xf86PostButtonEvent(device, 0, 4, 1, 0, 0);
    xf86PostButtonEvent(device, 0, 4, 0, 0, 0);
    priv->zy+=1.0;
  }

  while (priv->zx >= 1.0) {  /* right */
    xf86PostButtonEvent(device, 0, 7, 1, 0, 0);
    xf86PostButtonEvent(device, 0, 7, 0, 0, 0);
    priv->zx-=1.0;
  }
  while (priv->zx <= -1.0) { /* left */
    xf86PostButtonEvent(device, 0, 6, 1, 0, 0);
    xf86PostButtonEvent(device, 0, 6, 0, 0, 0);
    priv->zx+=1.0;
  }

  if (priv->mouse_enabled == FALSE) nexttimer = 0;
  if (nexttimer == 0) {
    priv->timerrunning = FALSE;
    priv->x  = 0.0;
    priv->y  = 0.0;
    priv->zx = 0.0;
    priv->zy = 0.0;
    DBG(2, ErrorF("Stopping Axis Timer\n"));
  }
  xf86UnblockSIGIO (sigstate);
  return nexttimer;
}


/***********************************************************************
 *
 * jstkStartAxisTimer --
 *
 * Starts the timer for the movement.
 * Will already prepare for moving one pixel, for "tipping" the stick
 *
 ***********************************************************************
 */
void
jstkStartAxisTimer(LocalDevicePtr device, int number) 
{
  int pixel;
  JoystickDevPtr priv = device->private;

  if (priv->timerrunning) return;
  priv->timerrunning = TRUE;

  pixel = 1;
  if (priv->axis[number].value < 0) pixel = -1;
  switch (priv->axis[number].mapping) {
    case MAPPING_X:
      priv->x += pixel;
      break;
    case MAPPING_Y:
      priv->y += pixel;
      break;
    case MAPPING_ZX:
      priv->zx += pixel;
      break;
    case MAPPING_ZY:
      priv->zy += pixel;
      break;
    default:
      break;
  }

  DBG(2, ErrorF("Starting Axis Timer (triggered by axis %d)\n", number));
  priv->timer = TimerSet(
    priv->timer, 
    0,         /* Relative */
    1,         /* What about NOW? */
    jstkAxisTimer,
    device->dev);
}

/***********************************************************************
 *
 * jstkStartButtonAxisTimer --
 *
 * Starts the timer for the movement.
 * Will already prepare for moving one pixel, for "tipping" the stick
 *
 ***********************************************************************
 */

void
jstkStartButtonAxisTimer(LocalDevicePtr device, int number) 
{
  int pixel;
  JoystickDevPtr priv = device->private;

  if (priv->timerrunning) return;
  priv->timerrunning = TRUE;

  pixel = 1;
  if (priv->button[number].amplify < 0) pixel = -1;
  switch (priv->button[number].mapping) {
    case MAPPING_X:
      priv->x += pixel;
      break;
    case MAPPING_Y:
      priv->y += pixel;
      break;
    case MAPPING_ZX:
      priv->zx += pixel;
      break;
    case MAPPING_ZY:
      priv->zy += pixel;
      break;
    default:
      break;
  }

  DBG(2, ErrorF("Starting Axis Timer (triggered by button %d)\n", number));
  priv->timer = TimerSet(
    priv->timer, 
    0,         /* Relative */
    1,         /* What about NOW? */
    jstkAxisTimer,
    device->dev);
}

/***********************************************************************
 *
 * jstkHandleAbsoluteAxis --
 *
 * Sums up absolute movement of all axes and sets the cursor to the
 * desired Position on the screen. 
 *
 ***********************************************************************
 */
void
jstkHandleAbsoluteAxis(LocalDevicePtr device, int number) 
{
  JoystickDevPtr priv = device->private;
  int i,x,y;

  x=0;
  y=0;

  for (i=0; i<MAXAXES; i++) 
    if (priv->axis[i].type == TYPE_ABSOLUTE)
  {
    float rel;
    int dif;
    if (priv->axis[i].value >= priv->axis[i].deadzone)
      rel = (priv->axis[i].value - priv->axis[i].deadzone);
    if (priv->axis[i].value <= -priv->axis[i].deadzone)
      rel = (priv->axis[i].value + priv->axis[i].deadzone);

    rel = (rel) / (2.0f * (float)(32768 - priv->axis[i].deadzone));
    /* rel contains numbers between -0.5 and +0.5 now */

    rel *= priv->axis[i].amplify;

    DBG(5, ErrorF("Relative Position of axis %d: %.2f\n", i, rel));

    /* Calculate difference to previous position on screen in pixels */
    dif = (int)(rel - priv->axis[i].previousposition + 0.5f);
    if ((dif >= 1)||(dif <= -1)) {
      if (priv->axis[i].mapping == MAPPING_X) {
        x += (dif);
        priv->axis[i].previousposition += (float)dif;
      }
      if (priv->axis[i].mapping == MAPPING_Y) {
        y += (int)(dif);
        priv->axis[i].previousposition += (float)dif;
      }
    }
  }
  /* Still move relative, but relative to previous position of the axis */
  if ((x != 0) || (y != 0)) {
    DBG(4, ErrorF("Moving mouse by %dx%d pixels\n", x, y));
    xf86PostMotionEvent(device->dev, 0, 0, 2, x, y);
  }
}
