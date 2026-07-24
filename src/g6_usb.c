#include "g6_usb.h"

#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/bos.h>

#define G6_USB_VID 0x3434
#define G6_USB_PID 0xD687

USBD_DEVICE_DEFINE(g6_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   G6_USB_VID, G6_USB_PID);

USBD_DESC_LANG_DEFINE(g6_lang);
USBD_DESC_MANUFACTURER_DEFINE(g6_mfr, "G6 DIY Lab");
USBD_DESC_PRODUCT_DEFINE(g6_product, "G6 HE DIY DVT Fix v0.2");
USBD_DESC_SERIAL_NUMBER_DEFINE(g6_serial);

USBD_DESC_CONFIG_DEFINE(g6_fs_desc, "G6 HE FS");
USBD_DESC_CONFIG_DEFINE(g6_hs_desc, "G6 HE HS");

USBD_CONFIGURATION_DEFINE(g6_fs_config, USB_SCD_REMOTE_WAKEUP, 250U,
			  &g6_fs_desc);
USBD_CONFIGURATION_DEFINE(g6_hs_config, USB_SCD_REMOTE_WAKEUP, 250U,
			  &g6_hs_desc);

static const char *const class_blocklist[] = {
	"dfu_dfu",
	NULL,
};

static int add_descriptors(void)
{
	int ret;

	ret = usbd_add_descriptor(&g6_usbd, &g6_lang);
	if (ret != 0) {
		return ret;
	}
	ret = usbd_add_descriptor(&g6_usbd, &g6_mfr);
	if (ret != 0) {
		return ret;
	}
	ret = usbd_add_descriptor(&g6_usbd, &g6_product);
	if (ret != 0) {
		return ret;
	}
	return usbd_add_descriptor(&g6_usbd, &g6_serial);
}

static int add_configuration(enum usbd_speed speed,
			     struct usbd_config_node *config)
{
	int ret = usbd_add_configuration(&g6_usbd, speed, config);

	if (ret != 0) {
		return ret;
	}
	ret = usbd_register_all_classes(&g6_usbd, speed, 1,
					class_blocklist);
	if (ret != 0) {
		return ret;
	}
	usbd_device_set_code_triple(&g6_usbd, speed, 0, 0, 0);
	return 0;
}

struct usbd_context *g6_usb_init(void)
{
	int ret = add_descriptors();

	if (ret != 0) {
		return NULL;
	}

	if (USBD_SUPPORTS_HIGH_SPEED &&
	    usbd_caps_speed(&g6_usbd) == USBD_SPEED_HS) {
		ret = add_configuration(USBD_SPEED_HS, &g6_hs_config);
		if (ret != 0) {
			return NULL;
		}
	}

	ret = add_configuration(USBD_SPEED_FS, &g6_fs_config);
	if (ret != 0) {
		return NULL;
	}

	usbd_self_powered(&g6_usbd, false);
	ret = usbd_init(&g6_usbd);
	return ret == 0 ? &g6_usbd : NULL;
}
