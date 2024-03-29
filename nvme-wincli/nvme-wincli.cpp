// nvme-wincli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <winioctl.h>
#include <nvme.h>
#include <stdio.h>
#include <tchar.h>

/*Read at most buffernLength bytes from filePath into buffer. File at filePath must have at least
bufferLength bytes for this function to succeed */
static DWORD ReadFileIntoBuffer(const TCHAR *filePath, PVOID buffer, ULONG bufferLength)
{
    HANDLE hFile = NULL;
    DWORD dwBytesRead = 0;
    BOOL bErrorFlag = FALSE;
    DWORD ret = -1;

    hFile = CreateFile(filePath,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("ReadFileIntoBuffer: Terminal failure: Unable to open file \"%s\" for read.\n"), filePath);
        return -1;
    }
    _tprintf(_T("Reading %d bytes from %s.\n"), bufferLength, filePath);

    bErrorFlag = ReadFile(hFile,
        buffer,
        bufferLength,
        &dwBytesRead,
        NULL
    );

    if (FALSE == bErrorFlag)
    {
        printf("ReadFileIntoBuffer: Terminal failure: Unable to read from file.\n");
        ret = -2;
    } 
    else if (dwBytesRead != bufferLength)
    {
        printf("ReadFileIntoBuffer: Error. File must have at least %d bytes.\n", bufferLength);
        ret = dwBytesRead;
    }
    else
    {
        _tprintf(TEXT("Read %d bytes from %s, and wrote to buffer successfully.\n"), 
            dwBytesRead, 
            filePath);
        ret = bufferLength;
    }

    CloseHandle(hFile);
    return ret;
}

static DWORD WriteBufferToFile(const TCHAR *filePath, PVOID buffer, ULONG bufferLength)
{

    HANDLE hFile = NULL;
    DWORD dwBytesWritten = 0;
    BOOL bErrorFlag = FALSE;
    DWORD ret = -1;

    hFile = CreateFile(filePath,  // name of the write
        GENERIC_WRITE,          // open for writing
        0,                      // do not share
        NULL,                   // default security
        CREATE_ALWAYS,             // create new file only
        FILE_ATTRIBUTE_NORMAL,  // normal file
        NULL);                  // no attr. template

    if (hFile == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("WriteBufferToFile: Terminal failure: Unable to open file \"%s\" for write.\n"), filePath);
        return -1;
    }

    _tprintf(_T("Writing %d bytes to %s.\n"), bufferLength, filePath);

    bErrorFlag = WriteFile(
        hFile,           // open file handle
        buffer,      // start of data to write
        bufferLength,  // number of bytes to write
        &dwBytesWritten, // number of bytes that were written
        NULL);            // no overlapped structure


    if (FALSE == bErrorFlag)
    {
        printf("WriteBufferToFile: Terminal failure: Unable to write to file.\n");
        ret = -1;
    }
    else
    {
        if (dwBytesWritten != bufferLength)
        {
            // This is an error because a synchronous write that results in
            // success (WriteFile returns TRUE) should write all data as
            // requested. This would not necessarily be the case for
            // asynchronous writes.
            printf("Error: dwBytesWritten != bufferLength\n");
            ret = -1;
        }
        else
        {
            _tprintf(TEXT("Wrote %d bytes to %s successfully.\n"), dwBytesWritten, filePath);
            ret = bufferLength;
        }
    }

    CloseHandle(hFile);
    return ret;

}

#define ROGUE_SSD_TOKEN_SIZE 132 // bytes
/* Send CDW10.OPC 0xC1 to device, with input buffer containing auth token */
int doNvmeAdminPassthru(const TCHAR *drivePath)
{
    BOOL result;
    DWORD status = ERROR_SUCCESS;
    PVOID buffer = NULL;
    ULONG bufferLength = 0;
    ULONG returnedLength = 0;
    PSTORAGE_PROTOCOL_COMMAND protocolCommand = NULL;
    PNVME_COMMAND command = NULL;
    HANDLE hIoCtrl = NULL;
    const TCHAR * tokenFilePath = _T("authentication_token.key");

    bufferLength = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) +
        STORAGE_PROTOCOL_COMMAND_LENGTH_NVME + ROGUE_SSD_TOKEN_SIZE +
        sizeof(NVME_ERROR_INFO_LOG);

    printf("bufferLength: %d\n", bufferLength);

    //buffer = malloc(bufferLength);
    buffer = VirtualAlloc(NULL, bufferLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
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
    protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
    //protocolCommand->DataFromDeviceTransferLength = 4096;
    protocolCommand->DataToDeviceTransferLength = ROGUE_SSD_TOKEN_SIZE;
    protocolCommand->TimeOutValue = 10;
    protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) +
        STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
    //protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset +
    //    protocolCommand->ErrorInfoLength;
    protocolCommand->DataToDeviceBufferOffset = protocolCommand->ErrorInfoOffset +
        protocolCommand->ErrorInfoLength;
    protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

    // TODO read from file into buffer
    PVOID tokenBuffer = ((unsigned char *)buffer + protocolCommand->DataToDeviceBufferOffset);
    if (-1 == ReadFileIntoBuffer(tokenFilePath, tokenBuffer, ROGUE_SSD_TOKEN_SIZE))
    {
        printf("doNvmeAdminPassthru: ReadFileIntoBuffer failure.\n");
        status = ERROR_READ_FAULT;
        goto exit;
    }

    command = (PNVME_COMMAND)protocolCommand->Command;

    printf("*********Debug info*********\n");
    printf("Address of buffer: 0x%p\n", buffer);
    printf("Address of command: 0x%p\n", command);
    printf("Address of ErrorInfoOffset: 0x%x\n", (DWORD) protocolCommand->ErrorInfoOffset);
    printf("Address of DataFromDeviceBufferOffset: 0x%x\n", (DWORD) protocolCommand->DataFromDeviceBufferOffset);
    printf("*********End debug info*********\n");

    command->CDW0.OPC = 0xC1;
    command->NSID = 0x1;
    command->u.GENERAL.CDW10 = 0x8;
    //command->u.GENERAL.CDW15 = 0xDEADBEEF; // Backdoor, YUCK!
    // This for debugging
    // command->PRP1 = 0xDEADBEEF0BADCAFE;

    hIoCtrl = CreateFile(drivePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
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

    if (!result)
    {
        _tprintf(_T("DeviceIoControl failed, error code:%d exit.\n"), GetLastError());
        printf("returnedLength: %d\n", returnedLength);
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
    if (hIoCtrl)
    {
        CloseHandle(hIoCtrl);
    }
    
    //free(buffer);
    VirtualFree(buffer, bufferLength, MEM_DECOMMIT | MEM_RELEASE);
    return status;
}

#define NVME_MAX_LOG_SIZE 4096

int doNvmeIdentifyQuery(const TCHAR *drivePath)
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;
    DWORD status = 0;

    PSTORAGE_PROPERTY_QUERY query = NULL;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = NULL;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = NULL;

    //
    // Allocate buffer for use.
    //
    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + NVME_MAX_LOG_SIZE;
    buffer = malloc(bufferLength);

    if (buffer == NULL) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: allocate buffer failed, exit.\n"));
        return -1;
    }

    //
    // Initialize query data structure to get Identify Controller Data.
    //
    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageAdapterProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeIdentify;
    protocolData->ProtocolDataRequestValue = NVME_IDENTIFY_CNS_CONTROLLER;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = NVME_MAX_LOG_SIZE;


    HANDLE hIoCtrl = CreateFile(drivePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        0);
    //
    // Send request down.
    //
    result = DeviceIoControl(hIoCtrl,
        IOCTL_STORAGE_QUERY_PROPERTY,
        buffer,
        bufferLength,
        buffer,
        bufferLength,
        &returnedLength,
        NULL
    );


    if (!result)
    {
        _tprintf(_T("DeviceIoControl failed, error code:%d exit.\n"), GetLastError());
        status = GetLastError();
        goto exit;
    }
    printf("returnedLength: %d\n", returnedLength);

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - data descriptor header not valid.\n"));
        return -1;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength > NVME_MAX_LOG_SIZE)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Get Identify Controller Data - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Identify Controller Data 
    //
    {
        PNVME_IDENTIFY_CONTROLLER_DATA identifyControllerData = (PNVME_IDENTIFY_CONTROLLER_DATA)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        if ((identifyControllerData->VID == 0) ||
            (identifyControllerData->NN == 0)) {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: Identify Controller Data not valid.\n"));
            goto exit;
        }
        else {
            _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***Identify Controller Data succeeded***.\n"));
        }
    }

exit:
    _tprintf(_T("Au revoir, Kun, you are seeing the exit message.\n"));
    CloseHandle(hIoCtrl);
    free(buffer);
    return 0;

}

int doNvmeGetLogPagesHealthInfo(const TCHAR *drivePath)
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;
    DWORD status = 0;
    PSTORAGE_PROPERTY_QUERY query = NULL;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = NULL;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = NULL;

    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + sizeof(NVME_HEALTH_INFO_LOG);
    printf("***Debug: bufferLength = %d***\n", bufferLength);
    buffer = malloc(bufferLength);
    if (buffer == NULL) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: allocate buffer failed, exit.\n"));
        return -1;
    }

    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_HEALTH_INFO;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = sizeof(NVME_HEALTH_INFO_LOG);

    HANDLE hIoCtrl = CreateFile(drivePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        0);

    if (hIoCtrl == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Obtaining handle failed, error code:%d exit.\n"), GetLastError());
        status = GetLastError();
        goto exit;
    }
    //  
    // Send request down.  
    //  
    result = DeviceIoControl(hIoCtrl,
        IOCTL_STORAGE_QUERY_PROPERTY,
        buffer,
        bufferLength,
        buffer,
        bufferLength,
        &returnedLength,
        NULL
    );

    if (!result || (returnedLength == 0)) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log - data descriptor header not valid.\n"));
        return -1;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < sizeof(NVME_HEALTH_INFO_LOG))) {
        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // SMART/Health Information Log Data 
    //
    {
        PNVME_HEALTH_INFO_LOG smartInfo = (PNVME_HEALTH_INFO_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log Data - Temperature %d.\n"), ((ULONG)smartInfo->Temperature[1] << 8 | smartInfo->Temperature[0]) - 273);

        _tprintf(_T("DeviceNVMeQueryProtocolDataTest: ***SMART/Health Information Log succeeded***.\n"));
    }

exit:
    _tprintf(_T("Au revoir, Kun, you are seeing the exit message.\n"));
    CloseHandle(hIoCtrl);
    free(buffer);
    return 0;

}

/*
get-log command effects log (CEL) 
*/
#define NVME_LOG_PAGE_ROGUE_CEL 0x06
#define CEL_FILE_PATH _T("cel.bin")

int doNvmeGetLogPagesCEL(const TCHAR *drivePath)
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;
    DWORD status = 0;
    PSTORAGE_PROPERTY_QUERY query = NULL;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = NULL;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = NULL;

    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) + 
        sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + sizeof(NVME_COMMAND_EFFECTS_LOG);
    printf("***Debug: bufferLength = %d***\n", bufferLength);
    buffer = malloc(bufferLength);
    if (buffer == NULL) {
        _tprintf(_T("doNvmeGetLogPagesCEL: allocate buffer failed, exit.\n"));
        return -1;
    }


    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeLogPage;
    //protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_COMMAND_EFFECTS;
    protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_ROGUE_CEL;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = sizeof(NVME_COMMAND_EFFECTS_LOG);

    HANDLE hIoCtrl = CreateFile(drivePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        0);

    if (hIoCtrl == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Obtaining handle failed, error code:%d exit.\n"), GetLastError());
        status = GetLastError();
        goto exit;
    }
    //  
    // Send request down.  
    //  
    result = DeviceIoControl(hIoCtrl,
        IOCTL_STORAGE_QUERY_PROPERTY,
        buffer,
        bufferLength,
        buffer,
        bufferLength,
        &returnedLength,
        NULL
    );

//    if (!result || (returnedLength == 0)) {
      if (!result) {

        _tprintf(_T("doNvmeGetLogPagesCEL: Command Effects Log failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("doNvmeGetLogPagesCEL: SMART/Health Information Log - data descriptor header not valid.\n"));
        return -1;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < sizeof(NVME_COMMAND_EFFECTS_LOG))) {
        _tprintf(_T("doNvmeGetLogPagesCEL: Command Effects Log - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Command Effects Log Data 
    //
    {
        PNVME_COMMAND_EFFECTS_LOG celInfo = (PNVME_COMMAND_EFFECTS_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        WriteBufferToFile(CEL_FILE_PATH, celInfo, sizeof(NVME_COMMAND_EFFECTS_LOG));
        //_tprintf(_T("DeviceNVMeQueryProtocolDataTest: SMART/Health Information Log Data - Temperature %d.\n"), ((ULONG)smartInfo->Temperature[1] << 8 | smartInfo->Temperature[0]) - 273);

        _tprintf(_T("doNvmeGetLogPagesCEL: ***Command Effects Log succeeded***.\n"));
    }

exit:
    _tprintf(_T("Au revoir, Kun, you are seeing the exit message.\n"));
    CloseHandle(hIoCtrl);
    free(buffer);
    return 0;

}

/*
Get-log backdoor for entering recovery mode
*/
#define GET_LOG_BACKDOOR_RECOVERY 0x42

int doNvmeEnterRecovery(const TCHAR *drivePath)
{
    BOOL    result;
    PVOID   buffer = NULL;
    ULONG   bufferLength = 0;
    ULONG   returnedLength = 0;
    DWORD status = 0;
    PSTORAGE_PROPERTY_QUERY query = NULL;
    PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = NULL;
    PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = NULL;

    bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) +
        sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + sizeof(NVME_COMMAND_EFFECTS_LOG);
    printf("***Debug: bufferLength = %d***\n", bufferLength);
    buffer = malloc(bufferLength);
    if (buffer == NULL) {
        _tprintf(_T("doNvmeEnterRecovery: allocate buffer failed, exit.\n"));
        return -1;
    }


    ZeroMemory(buffer, bufferLength);

    query = (PSTORAGE_PROPERTY_QUERY)buffer;
    protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer;
    protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;

    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeLogPage;
    protocolData->ProtocolDataRequestValue = GET_LOG_BACKDOOR_RECOVERY;
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = sizeof(NVME_COMMAND_EFFECTS_LOG);

    HANDLE hIoCtrl = CreateFile(drivePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        0);

    if (hIoCtrl == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("Obtaining handle failed, error code:%d exit.\n"), GetLastError());
        status = GetLastError();
        goto exit;
    }
    //  
    // Send request down.  
    //  
    result = DeviceIoControl(hIoCtrl,
        IOCTL_STORAGE_QUERY_PROPERTY,
        buffer,
        bufferLength,
        buffer,
        bufferLength,
        &returnedLength,
        NULL
    );

    //    if (!result || (returnedLength == 0)) {
    if (!result) {

        _tprintf(_T("doNvmeEnterReovery: Command Effects Log failed. Error Code %d.\n"), GetLastError());
        goto exit;
    }

    //
    // Validate the returned data.
    //
    if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
        (protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) {
        _tprintf(_T("doNvmeEnterRecovery: Command Effects Log - data descriptor header not valid.\n"));
        return -1;
    }

    protocolData = &protocolDataDescr->ProtocolSpecificData;

    if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
        (protocolData->ProtocolDataLength < sizeof(NVME_COMMAND_EFFECTS_LOG))) {
        _tprintf(_T("doNvmeEnterReovery: Command Effects Log - ProtocolData Offset/Length not valid.\n"));
        goto exit;
    }

    //
    // Command Effects Log Data 
    //
    {
        PNVME_COMMAND_EFFECTS_LOG celInfo = (PNVME_COMMAND_EFFECTS_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

        _tprintf(_T("doNvmeEnterRecovery: ***Entering recovery mode succeeded***.\n"));
    }

exit:
    _tprintf(_T("Au revoir, Kun, you are seeing the exit message.\n"));
    CloseHandle(hIoCtrl);
    free(buffer);
    return 0;

}

int _tmain(int argc, TCHAR *argv[])
{
    if (argc != 3)
    {
        _tprintf(_T("Usage: %s <DEVICE_PATH> <CMD_TYPE>\n")
            _T("DEVICE_PATH example: \\\\.\\G:\n")
            _T("CMD_TYPE:\n")
            _T("0 - Enter recovery mode through the 0x42 backdoor\n")
            _T("1 - NVMe admin-passthru with auth token\n")
            _T("2 - NVMe identify query\n")
            _T("3 - NVMe get-log health-info query\n")
            _T("4 - NVMe get-log command effects log query, cel will be written to file\n"), argv[0]);
        return -1;
    }

    int status = ERROR_SUCCESS;
    const TCHAR* testDrive = argv[1];
    const TCHAR* cmdType = argv[2];
    _tprintf(_T("Drive path: %s\n"), testDrive);

    switch (*cmdType)
    {
    case '0':
        status = doNvmeEnterRecovery(testDrive);
        break;
    case '1':
        status = doNvmeAdminPassthru(testDrive);
        break;
    case '2':
        status = doNvmeIdentifyQuery(testDrive);
        break;
    case '3':
        status = doNvmeGetLogPagesHealthInfo(testDrive);
        break;
    case '4':
        status = doNvmeGetLogPagesCEL(testDrive);
        break;
    default:
        _tprintf(_T("Unrecoganized command type %s\n"), cmdType);
        exit(-1);
    }

    return status;
}
