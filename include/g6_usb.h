#ifndef G6_USB_H
#define G6_USB_H

#include <zephyr/usb/usbd.h>

struct usbd_context *g6_usb_init(void);

#endif
