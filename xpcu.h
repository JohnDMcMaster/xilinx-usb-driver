#define XPCU_CLAIM	1
#define XPCU_RELEASE	0

#define ENABLE_INTERRUPT	1
#define DISABLE_INTERRUPT	0

int __attribute__ ((visibility ("hidden"))) xpcu_deviceinfo(struct usb_get_device_data *ugdd);
int __attribute__ ((visibility ("hidden"))) xpcu_transfer(struct usb_transfer *ut);
int __attribute__ ((visibility ("hidden"))) xpcu_set_interface(struct usb_set_interface *usi);
int __attribute__ ((visibility ("hidden"))) xpcu_find(struct event *e);
int __attribute__ ((visibility ("hidden"))) xpcu_found(struct event *e);
int __attribute__ ((visibility ("hidden"))) xpcu_close(struct event *e);
int __attribute__ ((visibility ("hidden"))) xpcu_int_state(struct interrupt *it, int enable);
int __attribute__ ((visibility ("hidden"))) xpcu_int_wait(struct interrupt *it);
