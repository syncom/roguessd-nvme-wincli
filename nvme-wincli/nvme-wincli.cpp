// nvme-wincli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <winioctl.h>
#include <nvme.h>
#include <stdio.h>
#include <tchar.h>

const TCHAR* testDrive = _T("\\\\.\\C:");


int main()
{
	BOOL result;
    DWORD status = 0;
	PVOID buffer = NULL;
	ULONG bufferLength = 0;
	ULONG returnedLength = 0;
	PSTORAGE_PROTOCOL_COMMAND protocolCommand = NULL;
	PNVME_COMMAND command = NULL;

	bufferLength = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) +
		STORAGE_PROTOCOL_COMMAND_LENGTH_NVME + 4096 +
		sizeof(NVME_ERROR_INFO_LOG);
	
	buffer = malloc(bufferLength);
	if (NULL == buffer)
	{
		_tprintf(_T("GetPanicLog: allocate buffer failed, error code:%d exit.\n"), GetLastError());
		return GetLastError();
	}

	ZeroMemory(buffer, bufferLength);
	protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)buffer;

	protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
	protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    protocolCommand->ProtocolType = ProtocolTypeNvme;
	protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
    protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
    protocolCommand->DataFromDeviceTransferLength = 4096;
    protocolCommand->TimeOutValue = 10;
    protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) +
        STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset +
        protocolCommand->ErrorInfoLength;
    protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

    command = (PNVME_COMMAND)protocolCommand->Command;

    command->CDW0.OPC = 0xC2;
    command->NSID = 0x1;
    command->u.GENERAL.CDW10 = 0x8;
    command->u.GENERAL.CDW15 = 0xDEADBEEF; // Backdoor, YUCK!

    HANDLE hIoCtrl = CreateFile(testDrive, 
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL, 
        0);

    if (hIoCtrl == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Obtaining handle failed, error code:%d exit.\n"), GetLastError());
        status = GetLastError();
        goto exit;
    }

    result = DeviceIoControl(hIoCtrl,
        IOCTL_STORAGE_PROTOCOL_COMMAND,
        buffer,
        bufferLength,
        buffer,
        bufferLength,
        &returnedLength,
        NULL
    );

    if (result == FALSE)
    {
        _tprintf(_T("DeviceIoControl failed, error code:%d exit.\n"), GetLastError());
        status = GetLastError();
        goto exit;
    }

    if (protocolCommand->ReturnStatus != STORAGE_PROTOCOL_STATUS_SUCCESS)
    {
        _tprintf(_T("NMVE command did not succeed to the device, error status:%d exit.\n"), protocolCommand->ReturnStatus);
        status = protocolCommand->ReturnStatus;
        goto exit;
    }
    else
    {
        _tprintf(_T("NVME command succeeded to device!\n"));
    }

exit:
    _tprintf(_T("Au revoir, Kun, you are seeing the exit message.\n"));
    CloseHandle(hIoCtrl);
    free(buffer);
    return status;
}

