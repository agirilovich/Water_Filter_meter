#define MEMORY_SIZE 0x8000
#define PAGE_SIZE 64
#define HAL_RTC_MODULE_ENABLED


void BackupInit();
void RTCInit(unsigned long EpochTime);
void BackupEEPROMPut();
void BackupRTCPut();
void BackupGet();
