#define MMC_NO_ERR	0
#define MMC_READ_GEN_ERR	0x10
#define MMC_READ_INVALID_ERR	0x11	//Invalid sector address
#define MMC_READ_TOKEN_ERR	0x12	
#define MMC_WRITE_GEN_ERR	0x20
#define MMC_WRITE_SEC0_ERR	0x21	//Attempt to write sector #0
#define MMC_WRITE_INVALID_ERR	0x22	//Invalid sector address
#define MMC_INIT_RESET_ERR	0x30
#define MMC_INIT_IDLE_ERR	0x31

char Init_MMC(int max_tries); 
int ReadSector(int32 sector, char *buff);	//Read 512 bytes to buff
int WriteSector(int32 sector, char *buff);	//Write 512 bytes from buff