#ifndef __FTP_H__
#define __FTP_H__
#include "core/net.h"

#define FTP_FIRMWARE_MAX_SIZE		983040
#define FTP_FIRMWARE_BUFFER_SIZE	32768 // firmware buffer size must be even divided by firmware max size

typedef struct stFtpServerInfo {
	char* serverIp;
	char* fileName;
	size_t fileSize;
} FtpServerInfo_t;


#endif