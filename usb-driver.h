#define VERSION			0x910
#define LICENSE			0x952
#define TRANSFER		0x98c
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

#define MAGIC 0xa410b413UL

#define WDU_GET_MAX_PACKET_SIZE(x)                ((uint16_t) (((x) & 0x7ff) * (1 + (((x) & 0x1800) >> 11))))

/* http://www.jungo.com/support/documentation/windriver/811/wdusb_man_mhtml/node78.html#SECTION001734000000000000000 */

struct header_struct {
	uint32_t magic;
	void* data;
	uint32_t size;
};

struct version_struct {
	uint32_t versionul;
	char version[128];
};

struct license_struct {
	char cLicense[128]; // Buffer with license string to put.
	// If empty string then get current license setting
	// into dwLicense.
	uint32_t dwLicense;  // Returns license settings: LICENSE_DEMO, LICENSE_WD
	// etc..., or 0 for invalid license.
	uint32_t dwLicense2; // Returns additional license settings, if dwLicense
	// could not hold all the information.
	// Then dwLicense will return 0.
};

typedef struct
{
	uint32_t dwVendorId;
	uint32_t dwDeviceId;
} WD_PCI_ID;

typedef struct
{
	uint32_t dwBus;
	uint32_t dwSlot;
	uint32_t dwFunction;
} WD_PCI_SLOT;

typedef struct
{
	uint32_t dwVendorId;
	uint32_t dwProductId;
} WD_USB_ID;

typedef struct
{
	uint16_t VendorId;
	uint16_t ProductId;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
} WDU_MATCH_TABLE;

typedef struct
{
	uint32_t dwNumber;        // Pipe 0 is the default pipe
	uint32_t dwMaximumPacketSize;
	uint32_t type;            // USB_PIPE_TYPE
	uint32_t direction;       // WDU_DIR
	// Isochronous, Bulk, Interrupt are either USB_DIR_IN or USB_DIR_OUT
	// Control are USB_DIR_IN_OUT
	uint32_t dwInterval;      // interval in ms relevant to Interrupt pipes
} WD_USB_PIPE_INFO, WD_USB_PIPE_INFO_V43, WDU_PIPE_INFO;

#define WD_USB_MAX_PIPE_NUMBER 32

typedef struct
{
	uint32_t dwPipes;
	WD_USB_PIPE_INFO Pipe[WD_USB_MAX_PIPE_NUMBER];
} WD_USB_DEVICE_INFO, WD_USB_DEVICE_INFO_V43;

struct usb_transfer
{
	uint32_t dwUniqueID;
	uint32_t dwPipeNum;    // Pipe number on device.
	uint32_t fRead;        // TRUE for read (IN) transfers; FALSE for write (OUT) transfers.
	uint32_t dwOptions;    // USB_TRANSFER options:
	// USB_ISOCH_FULL_PACKETS_ONLY - For isochronous
	// transfers only. If set, only full packets will be
	// transmitted and the transfer function will return
	// when the amount of bytes left to transfer is less
	// than the maximum packet size for the pipe (the
	// function will return without transmitting the
	// remaining bytes).
	void* pBuffer;    // Pointer to buffer to read/write.
	uint32_t dwBufferSize; // Amount of bytes to transfer.
	uint32_t dwBytesTransferred; // Returns the number of bytes actually read/written
	uint8_t SetupPacket[8];          // Setup packet for control pipe transfer.
	uint32_t dwTimeout;    // Timeout for the transfer in milliseconds. Set to 0 for infinite wait.
};




struct event {
	uint32_t handle;
	uint32_t dwAction; // WD_EVENT_ACTION
	uint32_t dwStatus; // EVENT_STATUS
	uint32_t dwEventId;
	uint32_t dwCardType; //WD_BUS_PCI, WD_BUS_USB, WD_BUS_PCMCIA
	uint32_t hKernelPlugIn;
	uint32_t dwOptions; // WD_EVENT_OPTION
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
			uint32_t dwUniqueID;
		} Usb;
	} u;
	uint32_t dwEventVer;
	uint32_t dwNumMatchTables;
	WDU_MATCH_TABLE matchTables[1];
};

typedef struct
{
	uint32_t dwBusType;        // Bus Type: ISA, EISA, PCI, PCMCIA.
	uint32_t dwBusNum;         // Bus number.
	uint32_t dwSlotFunc;       // Slot number on Bus.
} WD_BUS, WD_BUS_V30;

typedef struct
{
	uint32_t item; // ITEM_TYPE
	uint32_t fNotSharable;
	uint32_t dwReserved; // Reserved for internal use
	uint32_t dwOptions; // WD_ITEM_OPTIONS
	union
	{
		struct
		{ // ITEM_MEMORY
			uint32_t dwPhysicalAddr;     // Physical address on card.
			uint32_t dwBytes;            // Address range.
			void* dwTransAddr;         // Returns the address to pass on to transfer commands.
			void* dwUserDirectAddr;   // Returns the address for direct user read/write.
			uint32_t dwCpuPhysicalAddr;  // Returns the CPU physical address
			uint32_t dwBar;              // Base Address Register number of PCI card.
		} Mem;
		struct
		{ // ITEM_IO
			void* dwAddr;          // Beginning of io address.
			uint32_t dwBytes;        // IO range.
			uint32_t dwBar;          // Base Address Register number of PCI card.
		} IO;
		struct
		{ // ITEM_INTERRUPT
			uint32_t dwInterrupt; // Number of interrupt to install.
			uint32_t dwOptions;   // Interrupt options. For level sensitive
			// interrupts - set to: INTERRUPT_LEVEL_SENSITIVE.
			uint32_t hInterrupt;  // Returns the handle of the interrupt installed.
		} Int;
		WD_BUS Bus; // ITEM_BUS
		struct
		{
			uint32_t dw1, dw2, dw3, dw4; // Reserved for internal use
			void* dw5; // Reserved for internal use
		} Val;
	} I;
} WD_ITEMS, WD_ITEMS_V30;

#define WD_CARD_ITEMS	20

typedef struct
{
	uint32_t dwItems;
	WD_ITEMS Item[WD_CARD_ITEMS];
} WD_CARD, WD_CARD_V30;

enum { CARD_VX_NO_MMU_INIT = 0x4000000 };

struct card_register
{
	WD_CARD Card;           // Card to register.
	uint32_t fCheckLockOnly;   // Only check if card is lockable, return hCard=1 if OK.
	uint32_t hCard;            // Handle of card.
	uint32_t dwOptions;        // Should be zero.
	char cName[32];         // Name of card.
	char cDescription[100]; // Description.
};

typedef struct
{
	void* dwPort;       // IO port for transfer or kernel memory address.
	uint32_t cmdTrans;    // Transfer command WD_TRANSFER_CMD.

	// Parameters used for string transfers:
	uint32_t dwBytes;     // For string transfer.
	uint32_t fAutoinc;    // Transfer from one port/address
	// or use incremental range of addresses.
	uint32_t dwOptions;   // Must be 0.
	union
	{
		uint8_t Byte;     // Use for 8 bit transfer.
		uint16_t Word;     // Use for 16 bit transfer.
		uint32_t Dword;   // Use for 32 bit transfer.
		uint64_t Qword;  // Use for 64 bit transfer.
		void* pBuffer; // Use for string transfer.
	} Data;
} WD_TRANSFER, WD_TRANSFER_V61;

typedef struct
{
	uint32_t hKernelPlugIn;
	uint32_t dwMessage;
	void* pData;
	uint32_t dwResult;
} WD_KERNEL_PLUGIN_CALL, WD_KERNEL_PLUGIN_CALL_V40;


struct interrupt
{
	uint32_t hInterrupt;    // Handle of interrupt.
	uint32_t dwOptions;     // Interrupt options: can be INTERRUPT_CMD_COPY

	WD_TRANSFER *Cmd;    // Commands to do on interrupt.
	uint32_t dwCmds;        // Number of commands.

	// For WD_IntEnable():
	WD_KERNEL_PLUGIN_CALL kpCall; // Kernel PlugIn call.
	uint32_t fEnableOk;     // TRUE if interrupt was enabled (WD_IntEnable() succeed).

	// For WD_IntWait() and WD_IntCount():
	uint32_t dwCounter;     // Number of interrupts received.
	uint32_t dwLost;        // Number of interrupts not yet dealt with.
	uint32_t fStopped;      // Was interrupt disabled during wait.
};

struct usb_set_interface
{
	uint32_t dwUniqueID;
	uint32_t dwInterfaceNum;
	uint32_t dwAlternateSetting;
	uint32_t dwOptions;
};

struct usb_get_device_data
{
	uint32_t dwUniqueID;
	void* pBuf;
	uint32_t dwBytes;
	uint32_t dwOptions;
};

#define WD_USB_MAX_INTERFACES 30

typedef struct
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} WDU_INTERFACE_DESCRIPTOR;

typedef struct
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} WDU_ENDPOINT_DESCRIPTOR;

typedef struct
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t MaxPower;
} WDU_CONFIGURATION_DESCRIPTOR;

typedef struct
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;

	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
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
	uint32_t dwNumAltSettings;
	WDU_ALTERNATE_SETTING *pActiveAltSetting;
} WDU_INTERFACE;

typedef struct
{
	WDU_CONFIGURATION_DESCRIPTOR Descriptor;
	uint32_t dwNumInterfaces;
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
