#define CONFIG_FILE       "/etc/fw_env.config"
#define DEVICE1_NAME      "/dev/mtd1"
#define DEVICE1_OFFSET    0x0000
#define DEVICE2_NAME      "/dev/mtd2"
#define DEVICE2_OFFSET    0x0000
#define CFG_ENV_SIZE      0x20000
#define ENV_HEADER_SIZE   (sizeof(ulong) + 1)
#define ENV_SIZE          (CFG_ENV_SIZE - ENV_HEADER_SIZE)
#define DEVICE_ESIZE      0x20000

typedef struct environment_s {
	ulong crc;						/* CRC32 over data bytes */
	unsigned char flags;			/* index flags */
	unsigned char data[ENV_SIZE];	/* environment data */
} env_t;

extern int  print_env(int argc, char *argv[]);
//extern char *get_env (char *name);
extern int  set_env  (int argc, char *argv[]);
extern unsigned	long  crc32	 (unsigned long, const unsigned char *, unsigned);
