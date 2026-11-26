// Copyright 2026 loco-pico contributors
// SPDX-License-Identifier: Apache-2.0
//
// USB CDC-ECM network bring-up.
//
// Adapted from the Zenbedded project's zenbedded_common USB support
// (Copyright 2026 Open Source Robotics Foundation, Inc., Apache-2.0) and from
// the Zephyr "sample_usbd" helper.

#include "usbd_net.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/bos.h>
#include <zephyr/usb/usbd.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#if defined(CONFIG_NET_CONFIG_SETTINGS)
#include <zephyr/net/net_config.h>
#endif
#if defined(CONFIG_NET_DHCPV4_SERVER)
#include <zephyr/net/dhcpv4_server.h>
#endif

LOG_MODULE_REGISTER(loco_usbd_net, LOG_LEVEL_INF);

/* Timeouts (ms). */
#define IPV4_ACQUIRE_TIMEOUT_MS 30000
#define CARRIER_TIMEOUT_MS      10000
#define DHCP_LEASE_TIMEOUT_MS   10000

#define DEFAULT_ZENOH_PORT "7447"

/* USB CDC-ECM Ethernet class instance, provided by the cdc-ecm snippet overlay. */
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_ecm_ethernet)
#define CDC_ECM_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_cdc_ecm_ethernet)
#endif

/* By default, do not register the USB DFU class DFU mode instance. */
static const char *const blocklist[] = {
	"dfu_dfu",
	NULL,
};

USBD_DEVICE_DEFINE(loco_pico_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   CONFIG_LOCO_PICO_USB_VID, CONFIG_LOCO_PICO_USB_PID);

USBD_DESC_LANG_DEFINE(loco_lang);
USBD_DESC_MANUFACTURER_DEFINE(loco_mfr, CONFIG_LOCO_PICO_USB_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(loco_product, CONFIG_LOCO_PICO_USB_PRODUCT);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(loco_sn)));

USBD_DESC_CONFIG_DEFINE(loco_fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(loco_hs_cfg_desc, "HS Configuration");

static const uint8_t usb_attributes =
	(IS_ENABLED(CONFIG_LOCO_PICO_USB_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0) |
	(IS_ENABLED(CONFIG_LOCO_PICO_USB_REMOTE_WAKEUP) ? USB_SCD_REMOTE_WAKEUP : 0);

USBD_CONFIGURATION_DEFINE(loco_fs_config, usb_attributes, CONFIG_LOCO_PICO_USB_MAX_POWER,
			  &loco_fs_cfg_desc);
USBD_CONFIGURATION_DEFINE(loco_hs_config, usb_attributes, CONFIG_LOCO_PICO_USB_MAX_POWER,
			  &loco_hs_cfg_desc);

static void fix_code_triple(struct usbd_context *uds_ctx, const enum usbd_speed speed)
{
	/* Classes with multiple interfaces advertise an Interface Association Descriptor. */
	if (IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) || IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS)) {
		usbd_device_set_code_triple(uds_ctx, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}
}

struct usbd_context *loco_pico_usbd_init(void)
{
	int err;

	err = usbd_add_descriptor(&loco_pico_usbd, &loco_lang);
	if (err) {
		LOG_ERR("Failed to add language descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&loco_pico_usbd, &loco_mfr);
	if (err) {
		LOG_ERR("Failed to add manufacturer descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&loco_pico_usbd, &loco_product);
	if (err) {
		LOG_ERR("Failed to add product descriptor (%d)", err);
		return NULL;
	}

	IF_ENABLED(CONFIG_HWINFO, (err = usbd_add_descriptor(&loco_pico_usbd, &loco_sn); if (err) {
					  LOG_ERR("Failed to add serial descriptor (%d)", err);
					  return NULL;
				  }));

	if (USBD_SUPPORTS_HIGH_SPEED && usbd_caps_speed(&loco_pico_usbd) == USBD_SPEED_HS) {
		err = usbd_add_configuration(&loco_pico_usbd, USBD_SPEED_HS, &loco_hs_config);
		if (err) {
			LOG_ERR("Failed to add High-Speed configuration (%d)", err);
			return NULL;
		}

		err = usbd_register_all_classes(&loco_pico_usbd, USBD_SPEED_HS, 1, blocklist);
		if (err) {
			LOG_ERR("Failed to register High-Speed classes (%d)", err);
			return NULL;
		}

		fix_code_triple(&loco_pico_usbd, USBD_SPEED_HS);
	}

	err = usbd_add_configuration(&loco_pico_usbd, USBD_SPEED_FS, &loco_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration (%d)", err);
		return NULL;
	}

	err = usbd_register_all_classes(&loco_pico_usbd, USBD_SPEED_FS, 1, blocklist);
	if (err) {
		LOG_ERR("Failed to register Full-Speed classes (%d)", err);
		return NULL;
	}

	fix_code_triple(&loco_pico_usbd, USBD_SPEED_FS);
	usbd_self_powered(&loco_pico_usbd, usb_attributes & USB_SCD_SELF_POWERED);

	err = usbd_init(&loco_pico_usbd);
	if (err) {
		LOG_ERR("Failed to initialize USB device (%d)", err);
		return NULL;
	}

	return &loco_pico_usbd;
}

#if defined(CONFIG_NET_DHCPV4_SERVER)
static void dhcpv4_lease_cb(struct net_if *iface, struct dhcpv4_addr_slot *slot,
			    void *user_data)
{
	ARG_UNUSED(iface);

	if (slot != NULL && slot->state == DHCPV4_SERVER_ADDR_ALLOCATED) {
		*(bool *)user_data = true;
	}
}
#endif

static const struct device *cdc_ecm_dev(void)
{
#if defined(CDC_ECM_NODE)
	return DEVICE_DT_GET(CDC_ECM_NODE);
#else
	return NULL;
#endif
}

static struct net_if *cdc_ecm_iface(void)
{
	const struct device *dev = cdc_ecm_dev();

	if (dev != NULL && device_is_ready(dev)) {
		return net_if_lookup_by_dev(dev);
	}
	return net_if_get_default();
}

int loco_pico_usbd_net_init(void)
{
	struct usbd_context *ctx = loco_pico_usbd_init();

	if (ctx == NULL) {
		LOG_ERR("Failed to initialise USB device context");
		return -ENODEV;
	}

	int err = usbd_enable(ctx);

	if (err != 0) {
		LOG_ERR("Failed to enable USB device (%d)", err);
		return err;
	}

	LOG_INF("USB device initialised and enabled");

	struct net_if *iface = cdc_ecm_iface();

	if (iface == NULL) {
		LOG_ERR("No network interface found for USB CDC-ECM");
		return -ENODEV;
	}

	/* Wait (best effort) for the host to attach and bring the carrier up. */
	uint32_t carrier_start = k_uptime_get_32();

	while (!net_if_is_carrier_ok(iface)) {
		if (k_uptime_get_32() - carrier_start > CARRIER_TIMEOUT_MS) {
			LOG_WRN("USB CDC-ECM carrier still down; is the host connected?");
			break;
		}
		k_sleep(K_MSEC(100));
	}

#if defined(CONFIG_NET_CONFIG_SETTINGS)
	err = net_config_init_app(cdc_ecm_dev(), "Initializing USB network");
	if (err != 0) {
		LOG_WRN("net_config_init_app returned %d for USB CDC-ECM", err);
	}
#endif

	uint32_t ipv4_start = k_uptime_get_32();

	while (net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED) == NULL) {
		if (k_uptime_get_32() - ipv4_start > IPV4_ACQUIRE_TIMEOUT_MS) {
			LOG_ERR("No IPv4 address on USB CDC-ECM within %d ms",
				IPV4_ACQUIRE_TIMEOUT_MS);
			return -ETIMEDOUT;
		}
		k_sleep(K_MSEC(100));
	}

	LOG_INF("USB CDC-ECM ready at %s", CONFIG_NET_CONFIG_MY_IPV4_ADDR);

#if defined(CONFIG_NET_DHCPV4_SERVER)
	struct net_in_addr pool_addr;

	if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_PEER_IPV4_ADDR, &pool_addr) == 0) {
		err = net_dhcpv4_server_start(iface, &pool_addr);
		if (err != 0) {
			LOG_ERR("Failed to start DHCPv4 server (%d)", err);
		} else {
			LOG_INF("DHCPv4 server leasing %s to the host",
				CONFIG_NET_CONFIG_PEER_IPV4_ADDR);

			uint32_t lease_start = k_uptime_get_32();

			while (k_uptime_get_32() - lease_start < DHCP_LEASE_TIMEOUT_MS) {
				bool lease_active = false;

				net_dhcpv4_server_foreach_lease(iface, dhcpv4_lease_cb,
								&lease_active);

				if (lease_active) {
					LOG_INF("Host acquired a DHCP lease");
					break;
				}
				k_sleep(K_MSEC(250));
			}
		}
	}
#endif

	return 0;
}

bool loco_pico_usbd_net_ready(void)
{
	struct net_if *iface = cdc_ecm_iface();

	if (iface == NULL) {
		return false;
	}

	return net_if_is_up(iface) &&
	       (net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED) != NULL);
}

int loco_pico_usbd_net_peer_locator(char *out, size_t out_len)
{
	if (out == NULL || out_len == 0) {
		return -EINVAL;
	}

#if defined(CONFIG_NET_CONFIG_PEER_IPV4_ADDR)
	const char *peer_ip = CONFIG_NET_CONFIG_PEER_IPV4_ADDR;
#else
	const char *peer_ip = "192.0.2.1";
#endif

	int written = snprintf(out, out_len, "%s:%s", peer_ip, DEFAULT_ZENOH_PORT);

	if (written < 0 || (size_t)written >= out_len) {
		return -ENOSPC;
	}

	return 0;
}
