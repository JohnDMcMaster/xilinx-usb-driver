#define VERSION			0x910
#define LICENSE			0x952
#define TRANSFER		0x98c
#define MULTI_TRANSFER		0x98d
#define USB_TRANSFER		0x983
#define EVENT_UNREGISTER	0x987
#define INT_DISABLE		0x91f
#define INT_WAIT		0x94b
#define CARD_REGISTER		0x9a4
#define EVENT_REGISTER		0x9a5
#define CARD_UNREGISTER		0x92b
#define USB_GET_DEVICE_DATA	0x9a7
#define INT_ENABLE		0x98e
#define EVENT_PULL		0x988
#define USB_SET_INTERFACE	0x981
#define CARD_REGISTER_OLD	0x97d
#define INT_ENABLE_OLD		0x91e
#define USB_GET_DEVICE_DATA_OLD	0x980
#define EVENT_REGISTER_OLD	0x986
#define TRANSFER_OLD		0x903
#define MULTI_TRANSFER_OLD	0x904

#define MAGIC 0xa410b413UL

#define PP_DATA			0
#define PP_STATUS		1
#define PP_CONTROL		2
#define PP_ECP_CFGA		0
#define PP_ECP_CFGB		1
#define PP_ECP_ECR		2
#define PP_READ			10
#define PP_WRITE		13

#define PP_TDI			0x01
#define PP_TDO			0x10
#define PP_PROG			0x10
#define PP_TCK			0x02
#define PP_TMS			0x04
#define PP_CTRL			0x08

#ifdef DEBUG
#define DPRINTF(format, args...) fprintf(stderr, format, ##args)
#else
#define DPRINTF(format, args...)
#endif

void hexdump(unsigned char *buf, int len);

#define WDU_GET_MAX_PACKET_SIZE(x)                ((unsigned short) (((x) & 0x7ff) * (1 + (((x) & 0x1800) >> 11))))

/* http://www.jungo.com/support/documentation/windriver/811/wdusb_man_mhtml/node78.html#SECTION001734000000000000000 */

struct header_struct {
	unsigned long magic;
	void* data;
	unsigned long size;
};

struct version_struct {
	unsigned long versionul;
	char version[128];
};

struct license_struct {
	char cLicense[128]; // Buffer with license string to put.
	// If empty string then get current license setting
	// into dwLicense.
	unsigned long dwLicense;  // Returns license settings: LICENSE_DEMO, LICENSE_WD
	// etc..., or 0 for invalid license.
	unsigned long dwLicense2; // Returns additional license settings, if dwLicense
	// could not hold all the information.
	// Then dwLicense will return 0.
};

typedef struct
{
	unsigned long dwVendorId;
	unsigned long dwDeviceId;
} WD_PCI_ID;

typedef struct
{
	unsigned long dwBus;
	unsigned long dwSlot;
	unsigned long dwFunction;
} WD_PCI_SLOT;

typedef struct
{
	unsigned long dwVendorId;
	unsigned long dwProductId;
} WD_USB_ID;

typedef struct
{
	unsigned short VendorId;
	unsigned short ProductId;
	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bInterfaceClass;
	unsigned char bInterfaceSubClass;
	unsigned char bInterfaceProtocol;
} WDU_MATCH_TABLE;

typedef struct
{
	unsigned long dwNumber;        // Pipe 0 is the default pipe
	unsigned long dwMaximumPacketSize;
	unsigned long type;            // USB_PIPE_TYPE
	unsigned long direction;       // WDU_DIR
	// Isochronous, Bulk, Interrupt are either USB_DIR_IN or USB_DIR_OUT
	// Control are USB_DIR_IN_OUT
	unsigned long dwInterval;      // interval in ms relevant to Interrupt pipes
} WD_USB_PIPE_INFO, WD_USB_PIPE_INFO_V43, WDU_PIPE_INFO;

#define WD_USB_MAX_PIPE_NUMBER 32

typedef struct
{
	unsigned long dwPipes;
	WD_USB_PIPE_INFO Pipe[WD_USB_MAX_PIPE_NUMBER];
} WD_USB_DEVICE_INFO, WD_USB_DEVICE_INFO_V43;

struct usb_transfer
{
	unsigned long dwUniqueID;
	unsigned long dwPipeNum;    // Pipe number on device.
	unsigned long fRead;        // TRUE for read (IN) transfers; FALSE for write (OUT) transfers.
	unsigned long dwOptions;    // USB_TRANSFER options:
	// USB_ISOCH_FULL_PACKETS_ONLY - For isochronous
	// transfers only. If set, only full packets will be
	// transmitted and the transfer function will return
	// when the amount of bytes left to transfer is less
	// than the maximum packet size for the pipe (the
	// function will return without transmitting the
	// remaining bytes).
	void* pBuffer;    // Pointer to buffer to read/write.
	unsigned long dwBufferSize; // Amount of bytes to transfer.
	unsigned long dwBytesTransferred; // Returns the number of bytes actually read/written
	unsigned char SetupPacket[8];          // Setup packet for control pipe transfer.
	unsigned long dwTimeout;    // Timeout for the transfer in milliseconds. Set to 0 for infinite wait.
};




struct event {
	unsigned long handle;
	unsigned long dwAction; // WD_EVENT_ACTION
	unsigned long dwStatus; // EVENT_STATUS
	unsigned long dwEventId;
	unsigned long dwCardType; //WD_BUS_PCI, WD_BUS_USB, WD_BUS_PCMCIA
	unsigned long hKernelPlugIn;
	unsigned long dwOptions; // WD_EVENT_OPTION
	union
	{
		struct
		{
			WD_PCI_ID cardId;
			WD_PCI_SLOT pciSlot;
		} Pci;
		struct
		{
			WD_USB_ID deviceId;
			unsigned long dwUniqueID;
		} Usb;
	} u;
	unsigned long dwEventVer;
	unsigned long dwNumMatchTables;
	WDU_MATCH_TABLE matchTables[1];
};

typedef struct
{
	unsigned long dwBusType;        // Bus Type: ISA, EISA, PCI, PCMCIA.
	unsigned long dwBusNum;         // Bus number.
	unsigned long dwSlotFunc;       // Slot number on Bus.
} WD_BUS, WD_BUS_V30;

typedef struct
{
	unsigned long item; // ITEM_TYPE
	unsigned long fNotSharable;
	unsigned long dwReserved; // Reserved for internal use
	unsigned long dwOptions; // WD_ITEM_OPTIONS
	union
	{
		struct
		{ // ITEM_MEMORY
			unsigned long dwPhysicalAddr;     // Physical address on card.
			unsigned long dwBytes;            // Address range.
			void* dwTransAddr;         // Returns the address to pass on to transfer commands.
			void* dwUserDirectAddr;   // Returns the address for direct user read/write.
			unsigned long dwCpuPhysicalAddr;  // Returns the CPU physical address
			unsigned long dwBar;              // Base Address Register number of PCI card.
		} Mem;
		struct
		{ // ITEM_IO
			void* dwAddr;          // Beginning of io address.
			unsigned long dwBytes;        // IO range.
			unsigned long dwBar;          // Base Address Register number of PCI card.
		} IO;
		struct
		{ // ITEM_INTERRUPT
			unsigned long dwInterrupt; // Number of interrupt to install.
			unsigned long dwOptions;   // Interrupt options. For level sensitive
			// interrupts - set to: INTERRUPT_LEVEL_SENSITIVE.
			unsigned long hInterrupt;  // Returns the handle of the interrupt installed.
		} Int;
		WD_BUS Bus; // ITEM_BUS
		struct
		{
			unsigned long dw1, dw2, dw3, dw4; // Reserved for internal use
			void* dw5; // Reserved for internal use
		} Val;
	} I;
} WD_ITEMS, WD_ITEMS_V30;

#define WD_CARD_ITEMS	20

typedef struct
{
	unsigned long dwItems;
	WD_ITEMS Item[WD_CARD_ITEMS];
} WD_CARD, WD_CARD_V30;

enum { CARD_VX_NO_MMU_INIT = 0x4000000 };

struct card_register
{
	WD_CARD Card;           // Card to register.
	unsigned long fCheckLockOnly;   // Only check if card is lockable, return hCard=1 if OK.
	unsigned long hCard;            // Handle of card.
	unsigned long dwOptions;        // Should be zero.
	char cName[32];         // Name of card.
	char cDescription[100]; // Description.
};

typedef struct
{
	void* dwPort;       // IO port for transfer or kernel memory address.
	unsigned long cmdTrans;    // Transfer command WD_TRANSFER_CMD.

	// Parameters used for string transfers:
	unsigned long dwBytes;     // For string transfer.
	unsigned long fAutoinc;    // Transfer from one port/address
	// or use incremental range of addresses.
	unsigned long dwOptions;   // Must be 0.
	union
	{
		unsigned char Byte;     // Use for 8 bit transfer.
		unsigned short Word;     // Use for 16 bit transfer.
		uint32_t Dword;   // Use for 32 bit transfer.
		uint64_t Qword;  // Use for 64 bit transfer.
		void* pBuffer; // Use for string transfer.
	} Data;
} WD_TRANSFER, WD_TRANSFER_V61;

typedef struct
{
	unsigned long hKernelPlugIn;
	unsigned long dwMessage;
	void* pData;
	unsigned long dwResult;
} WD_KERNEL_PLUGIN_CALL, WD_KERNEL_PLUGIN_CALL_V40;


struct interrupt
{
	unsigned long hInterrupt;    // Handle of interrupt.
	unsigned long dwOptions;     // Interrupt options: can be INTERRUPT_CMD_COPY

	WD_TRANSFER *Cmd;    // Commands to do on interrupt.
	unsigned long dwCmds;        // Number of commands.

	// For WD_IntEnable():
	WD_KERNEL_PLUGIN_CALL kpCall; // Kernel PlugIn call.
	unsigned long fEnableOk;     // TRUE if interrupt was enabled (WD_IntEnable() succeed).

	// For WD_IntWait() and WD_IntCount():
	unsigned long dwCounter;     // Number of interrupts received.
	unsigned long dwLost;        // Number of interrupts not yet dealt with.
	unsigned long fStopped;      // Was interrupt disabled during wait.
};

struct usb_set_interface
{
	unsigned long dwUniqueID;
	unsigned long dwInterfaceNum;
	unsigned long dwAlternateSetting;
	unsigned long dwOptions;
};

struct usb_get_device_data
{
	unsigned long dwUniqueID;
	void* pBuf;
	unsigned long dwBytes;
	unsigned long dwOptions;
};

#define WD_USB_MAX_INTERFACES 30

typedef struct
{
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned char bInterfaceNumber;
	unsigned char bAlternateSetting;
	unsigned char bNumEndpoints;
	unsigned char bInterfaceClass;
	unsigned char bInterfaceSubClass;
	unsigned char bInterfaceProtocol;
	unsigned char iInterface;
} WDU_INTERFACE_DESCRIPTOR;

typedef struct
{
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned char bEndpointAddress;
	unsigned char bmAttributes;
	unsigned short wMaxPacketSize;
	unsigned char bInterval;
} WDU_ENDPOINT_DESCRIPTOR;

typedef struct
{
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short wTotalLength;
	unsigned char bNumInterfaces;
	unsigned char bConfigurationValue;
	unsigned char iConfiguration;
	unsigned char bmAttributes;
	unsigned char MaxPower;
} WDU_CONFIGURATION_DESCRIPTOR;

typedef struct
{
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short bcdUSB;
	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bDeviceProtocol;
	unsigned char bMaxPacketSize0;

	unsigned short idVendor;
	unsigned short idProduct;
	unsigned short bcdDevice;
	unsigned char iManufacturer;
	unsigned char iProduct;
	unsigned char iSerialNumber;
	unsigned char bNumConfigurations;
} WDU_DEVICE_DESCRIPTOR;

typedef struct
{
	WDU_INTERFACE_DESCRIPTOR Descriptor;
	WDU_ENDPOINT_DESCRIPTOR *pEndpointDescriptors;
	WDU_PIPE_INFO *pPipes;
} WDU_ALTERNATE_SETTING;

typedef struct
{
	WDU_ALTERNATE_SETTING *pAlternateSettings;
	unsigned long dwNumAltSettings;
	WDU_ALTERNATE_SETTING *pActiveAltSetting;
} WDU_INTERFACE;

typedef struct
{
	WDU_CONFIGURATION_DESCRIPTOR Descriptor;
	unsigned long dwNumInterfaces;
	WDU_INTERFACE *pInterfaces;
} WDU_CONFIGURATION;

struct usb_device_info {
	WDU_DEVICE_DESCRIPTOR Descriptor;
	WDU_PIPE_INFO Pipe0;
	WDU_CONFIGURATION *pConfigs;
	WDU_CONFIGURATION *pActiveConfig;
	WDU_INTERFACE *pActiveInterface[WD_USB_MAX_INTERFACES];
};

typedef enum {
	WDU_DIR_IN     = 1,
	WDU_DIR_OUT    = 2,
	WDU_DIR_IN_OUT = 3
} WDU_DIR;

typedef enum {
	PIPE_TYPE_CONTROL     = 0,
	PIPE_TYPE_ISOCHRONOUS = 1,
	PIPE_TYPE_BULK        = 2,
	PIPE_TYPE_INTERRUPT   = 3
} USB_PIPE_TYPE;
