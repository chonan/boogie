/*
 *  Copyright (c) 2012      Hiroshi Chonan      <chonan@pid0.org>
 *
 *  USB "Boogie Board RIP" tablet support
 *
 *  Changelog:
 *      v0.1 - Initial experimental release
 *      v0.2 - Migrate to Kernel v3.5
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.2"
#define DRIVER_DESC    "USB Boogie Board tablet driver"
#define DRIVER_LICENSE "GPL"
#define DRIVER_AUTHOR  "Hiroshi Chonan <chonan@pid0.org>"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define USB_VENDOR_ID_TEXAS_INSTRUMENTS 	0x2047
#define USB_DEVICE_ID_BOOGIE			0xffe7

struct usb_boogie {
	char name[128];
	char phys[64];
	struct usb_device *usbdev;
	struct usb_interface *intf;
	struct input_dev *input;
	struct urb *irq;

	unsigned char *data;
	dma_addr_t data_dma;
};

static void usb_boogie_irq(struct urb *urb)
{
	struct usb_boogie *boogie = urb->context;
	unsigned char *data = boogie->data;
	struct input_dev *dev = boogie->input;
	struct usb_interface *intf = boogie->intf;
	int status;

	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dbg("%s - urb shutting down with status: %d", __func__, urb->status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d", __func__, urb->status);
		goto resubmit;
	}

	input_report_abs(dev, ABS_X, (data[4] | (data[5]<<8)));
	input_report_abs(dev, ABS_Y, (data[6] | (data[7]<<8)));
	input_report_abs(dev, ABS_PRESSURE, (data[8] | (data[9]<<8) ));
	input_report_key(dev, BTN_TOUCH, data[3] & 0x01);
	input_report_key(dev, BTN_STYLUS, (data[10] & 0x04)>>2);
	input_report_key(dev, BTN_STYLUS2, (data[10] & 0x02)>>1);

	/* event termination */
	input_sync(dev);

resubmit:
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status)
		dev_err(&intf->dev,
			"can't resubmit intr, %s-%s/input0, status %d",
			boogie->usbdev->bus->bus_name, boogie->usbdev->devpath, status);

}

static int usb_boogie_open(struct input_dev *dev)
{
	struct usb_boogie *boogie = input_get_drvdata(dev);

	boogie->irq->dev = boogie->usbdev;
	if (usb_submit_urb(boogie->irq, GFP_KERNEL))
		return -EIO;

	return 0;
}

static void usb_boogie_close(struct input_dev *dev)
{
	struct usb_boogie *boogie = input_get_drvdata(dev);

	usb_kill_urb(boogie->irq);
}

static int usb_boogie_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *interface = intf->cur_altsetting;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_boogie *boogie;
	struct input_dev *input_dev;
	int pipe, maxp;
	int err;

	if (interface->desc.bNumEndpoints != 1)
		return -ENODEV;

	endpoint = &interface->endpoint[0].desc;

	if (!usb_endpoint_is_int_in(endpoint))
		return -ENODEV;

	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

	boogie = kzalloc(sizeof(struct usb_boogie), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!boogie || !input_dev) {
		err = -ENOMEM;
		goto fail1;
	}


	boogie->data = usb_alloc_coherent(dev, 64, GFP_KERNEL, &boogie->data_dma);
	if (!boogie->data) {
		err= -ENOMEM;
		goto fail1;
	}

	boogie->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!boogie->irq) {
		err = -ENOMEM;
		goto fail2;
	}

	boogie->usbdev = dev;
	boogie->intf = intf;
	boogie->input = input_dev;

	if (dev->manufacturer) {
		strlcpy(boogie->name, dev->manufacturer, sizeof(boogie->name));
	}
	if (dev->product) {
		if (dev->manufacturer)
			strlcat(boogie->name, " ", sizeof(boogie->name));
		strlcat(boogie->name, dev->product, sizeof(boogie->name));
	}

	usb_make_path(dev, boogie->phys, sizeof(boogie->phys));
	strlcat(boogie->phys, "/input0", sizeof(boogie->phys));

	input_dev->name = boogie->name;
	input_dev->phys = boogie->phys;
	usb_to_input_id(dev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;

	input_set_drvdata(input_dev, boogie);

	input_dev->open = usb_boogie_open;
	input_dev->close = usb_boogie_close;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_DIGI)] = BIT_MASK(BTN_TOOL_PEN) |
		BIT_MASK(BTN_TOUCH) | BIT_MASK(BTN_STYLUS) |
		BIT_MASK(BTN_STYLUS2);

	switch (id->driver_info) {
	case 0:
                input_set_abs_params(input_dev, ABS_X, 100, 20000, 0, 0);
                input_set_abs_params(input_dev, ABS_Y, 200, 14000, 0, 0);
                input_set_abs_params(input_dev, ABS_PRESSURE, 0, 255, 0, 0);

		if (!strlen(boogie->name))
			snprintf(boogie->name, sizeof(boogie->name),
				"Improv USB Device %04x:%04x",
				le16_to_cpu(dev->descriptor.idVendor),
				le16_to_cpu(dev->descriptor.idProduct));
		break;
	default:
		return -ENODEV;
		break;
	}

	usb_fill_int_urb(boogie->irq, dev, pipe,
			boogie->data, maxp > 64 ? 64 : maxp,
			usb_boogie_irq, boogie, endpoint->bInterval);
	boogie->irq->transfer_dma = boogie->data_dma;
	boogie->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	err = input_register_device(boogie->input);
	if (err)
		goto fail3;

	usb_set_intfdata(intf, boogie);

	return 0;

 fail3:	usb_free_urb(boogie->irq);
 fail2:	usb_free_coherent(dev, 64, boogie->data, boogie->data_dma);
 fail1: input_free_device(input_dev);
	kfree(boogie);
	return err;
}

static void usb_boogie_disconnect(struct usb_interface *intf)
{
	struct usb_boogie *boogie = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);

	input_unregister_device(boogie->input);
	usb_free_urb(boogie->irq);
	usb_free_coherent(boogie->usbdev, 64, boogie->data, boogie->data_dma);
	kfree(boogie);
}

static struct usb_device_id usb_boogie_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_TEXAS_INSTRUMENTS, USB_DEVICE_ID_BOOGIE), .driver_info = 0 },
	{ }
};

MODULE_DEVICE_TABLE(usb, usb_boogie_id_table);

static struct usb_driver usb_boogie_driver = {
	.name =		"usb_boogie",
	.probe =	usb_boogie_probe,
	.disconnect =	usb_boogie_disconnect,
	.id_table =	usb_boogie_id_table,
};

static int __init usb_boogie_init(void)
{
	int result = usb_register(&usb_boogie_driver);
	if (result == 0)
		printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
		       DRIVER_DESC "\n");
	return result;
}

static void __exit usb_boogie_exit(void)
{
	usb_deregister(&usb_boogie_driver);
}

module_init(usb_boogie_init);
module_exit(usb_boogie_exit);
