/*
 * All the USB notify logic
 *
 * (C) Copyright 2005 Greg Kroah-Hartman <gregkh@suse.de>
 *
 * notifier functions originally based on those in kernel/sys.c
 * but fixed up to not be so broken.
 *
 */


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include "usb.h"

#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
#include <asm/arch/w55fa93_sysmgr.h>
extern void sysmgr_report(unsigned status);
#endif

static BLOCKING_NOTIFIER_HEAD(usb_notifier_list);

/**
 * usb_register_notify - register a notifier callback whenever a usb change happens
 * @nb: pointer to the notifier block for the callback events.
 *
 * These changes are either USB devices or busses being added or removed.
 */
void usb_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&usb_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(usb_register_notify);

/**
 * usb_unregister_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * usb_register_notifier() must have been previously called for this function
 * to work properly.
 */
void usb_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&usb_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(usb_unregister_notify);


void usb_notify_add_device(struct usb_device *udev)
{
	printk("USB device plug in\n"); 
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
        sysmgr_report(SYSMGR_STATUS_USBH_PLUG);
#endif
	blocking_notifier_call_chain(&usb_notifier_list, USB_DEVICE_ADD, udev);
}

void usb_notify_remove_device(struct usb_device *udev)
{
	printk("USB device plug out\n");
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
       sysmgr_report(SYSMGR_STATUS_USBH_UNPLUG);
#endif
	blocking_notifier_call_chain(&usb_notifier_list,
			USB_DEVICE_REMOVE, udev);
}

void usb_notify_add_bus(struct usb_bus *ubus)
{
	blocking_notifier_call_chain(&usb_notifier_list, USB_BUS_ADD, ubus);
}

void usb_notify_remove_bus(struct usb_bus *ubus)
{
	blocking_notifier_call_chain(&usb_notifier_list, USB_BUS_REMOVE, ubus);
}
