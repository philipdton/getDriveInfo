#include <QtWidgets>
#include <Windows.h>
#include <string>
#include <iostream>
#include <ntddscsi.h>
#include <strsafe.h>

#define SCSI_IOCTL_DATA_OUT             0 // Give data to SCSI device (e.g. for writing)
#define SCSI_IOCTL_DATA_IN              1 // Get data from SCSI device (e.g. for reading)
#define SCSI_IOCTL_DATA_UNSPECIFIED     2 // No data (e.g. for ejecting)
 
#define MAX_SENSE_LEN 18 //Sense data max length 
//#define IOCTL_SCSI_PASS_THROUGH_DIRECT  0x4D014

#define INQ_REPLY_LEN     96
#define INQ_CMD_LEN       6

#define READCAP_REPLY_LEN 8
#define READCAP_CMD_LEN   10

#define READBB_REPLY_LEN  1024
#define READ10_REPLY_LEN  512
#define READ10_CMD_LEN    16

#define BYTES_IN_MiB      1048576
#define BYTES_IN_MB       1000000

#define EBUFF_SZ 256
#define MAX_MU   256

enum inq_read_steps {inq_basic_info, inq_unit_serial_number};
enum read_steps {mu_and_lba, init_and_current_badblocks, current_spare_blocks_1, current_spare_blocks_2};

typedef unsigned short  USHORT;
typedef unsigned char   UCHAR;
typedef unsigned long   ULONG;
typedef void*           PVOID;
/*
typedef struct _SCSI_PASS_THROUGH_DIRECT
{    
    USHORT Length;    
    UCHAR ScsiStatus;    
    UCHAR PathId;    
    UCHAR TargetId;    
    UCHAR Lun;    
    UCHAR CdbLength;    
    UCHAR SenseInfoLength;    
    UCHAR DataIn;    
    ULONG DataTransferLength;    
    ULONG TimeOutValue;    
    PVOID DataBuffer;    
    ULONG SenseInfoOffset;    
    UCHAR Cdb[16];
}
SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT; 
*/
typedef struct _SCSI_PASS_THROUGH_DIRECT_AND_SENSE_BUFFER 
{    
    SCSI_PASS_THROUGH_DIRECT sptd;    
    UCHAR SenseBuf[MAX_SENSE_LEN];
}
T_SPDT_SBUF; 
 
typedef struct Result_Return
{
    QString strResult;
    int     iResult;
    QString VendorID, ProductID, ProductRevision, UnitSerialNumber, SMIChip;
    unsigned int BlockSize, DiskSize, Total_MU, Total_LBA, LBA_per_MU;
    int Current_BadBlock[MAX_MU], Initial_BadBlock[MAX_MU], Total_DataBlock[MAX_MU];
    int Initial_SpareBlock[MAX_MU], Current_SpareBlock[MAX_MU];

}
reResult;

reResult GetDvdStatus(QString drive)
{   
    const UCHAR cdbInq[2][INQ_CMD_LEN] =
             { {0x12, 0, 0, 0, INQ_REPLY_LEN, 0},      // INQUIRY command for Vendor ID, Product ID, Product Revision
               {0x12, 0, 0x80, 0, INQ_REPLY_LEN, 0} }; // INQUIRY command for  Unit Serial Number
    const UCHAR cdbCap[READCAP_CMD_LEN] =
             {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};        // READ CAPACITY command for Block Size and Disk Size
    unsigned char cdbR10[4][READ10_CMD_LEN] =
    { {0xF0, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}, // READ_10 command for reading basic information: total MU, LBA
      {0xF0, 0x0A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0}, // READ_10 command to get Initial and Current BadBlock numbers for each MU
      {0x28, 0x00, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}, // READ_10 command to get Current Spare Numbers for each MU step 1
      {0xF0, 0xAA, 0, 0, 0, 0, 0, 0x10, 0, 0, 0, 1, 0, 0, 0, 0} }; // READ_10 command to get Current Spare Numbers for each MU step 2
    const QString dvdDriveLetter = drive.left(2);
    Result_Return reResult;
    unsigned char FourBytes[4], TwoBytes[2];
    unsigned int SLBA;

    if ( dvdDriveLetter.isEmpty() )
    {
        reResult.iResult = -1;
        reResult.strResult = "Drive letter is empty";
        return reResult;
    }
     
    const QString strDvdPath =  "\\\\.\\"
            + dvdDriveLetter;
 
    HANDLE hDevice;               // handle to the drive to be examined     
    int iResult = -1;             // results flag
    QString strResult = "";
//    ULONG ulChanges = 0;
    DWORD dwBytesReturned;  
    T_SPDT_SBUF sptd_sb;          //SCSI Pass Through Direct variable.  
    unsigned char DataBuf[ INQ_REPLY_LEN ];            //Buffer for holding data to/from drive.
    unsigned char ReadDataBuf[ READ10_REPLY_LEN ];     //Buffer for holding data to/from drive for Read 10.
    unsigned char RdBBDataBuf[ READBB_REPLY_LEN ];     //Buffer for holding data to/from drive for Bad Block.
    unsigned char *ptr2Buffer;
    QString qDataBuf;

/*
    hDevice = CreateFile( (LPCWSTR)strDvdPath.c_str(),      // drive
                          0,                                // no access to the drive                        
                          FILE_SHARE_READ,                  // share mode                        
                          NULL,                             // default security attributes                        
                          OPEN_EXISTING,                    // disposition                        
                          FILE_ATTRIBUTE_READONLY,          // file attributes                        
                          NULL);
     
    // If we cannot access the DVD drive 
    if (hDevice == INVALID_HANDLE_VALUE)                    
    {           
        reResult.iResult = -1;
        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

        // Display the error message and exit the process

        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)CreateFile) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("%s failed with error %d: %s"),
            CreateFile, dw, lpMsgBuf);
        QMessageBox msgBox;
        msgBox.setText(QString::fromWCharArray((LPCTSTR)lpDisplayBuf));
        msgBox.exec();
        reResult.strResult = QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
        return reResult;
    }
     
    // A check to see determine if a DVD has been inserted into the drive only when iResult = 1.  
    // This will do it more quickly than by sending target commands to the SCSI
    iResult = DeviceIoControl((HANDLE) hDevice,              // handle to device                             
                                IOCTL_STORAGE_CHECK_VERIFY2, // dwIoControlCode                             
                                NULL,                        // lpInBuffer                             
                                0,                           // nInBufferSize                             
                                &ulChanges,                  // lpOutBuffer                             
                                sizeof(ULONG),               // nOutBufferSize                             
                                &dwBytesReturned ,           // number of bytes returned                             
                                NULL );                      // OVERLAPPED structure   
     
    CloseHandle( hDevice );   
     
    // Don't request the tray status as we often don't need it      
//    if( iResult == 1 )  return 2;   
*/

    /* 1 - INQUIRY command for Vendor ID, Product ID, Product Revision */
    hDevice = CreateFile( (LPCWSTR)strDvdPath.utf16(),
                          GENERIC_READ | GENERIC_WRITE,                        
                          FILE_SHARE_READ | FILE_SHARE_WRITE,                        
                          NULL,                        
                          OPEN_EXISTING,                        
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);
     
    if (hDevice == INVALID_HANDLE_VALUE)  
    {           
        reResult.iResult = -1;
        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

        // Display the error message and exit the process
        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)CreateFile) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("CreateFile failed with error %d: %sFor drive %s\n"), dw, lpMsgBuf, (LPCWSTR)strDvdPath.utf16());
//        QMessageBox msgBox;
//        msgBox.setText(QString::fromWCharArray((LPCTSTR)lpDisplayBuf));
//        msgBox.exec();
        reResult.strResult = QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
        return reResult;
    }
     
    sptd_sb.sptd.Length = sizeof( SCSI_PASS_THROUGH_DIRECT );  
    sptd_sb.sptd.PathId = 0;  
    sptd_sb.sptd.TargetId = 0;  
    sptd_sb.sptd.Lun = 0;  
    sptd_sb.sptd.CdbLength = INQ_CMD_LEN;
    sptd_sb.sptd.SenseInfoLength = MAX_SENSE_LEN;  
    sptd_sb.sptd.DataIn = SCSI_IOCTL_DATA_IN;  
    sptd_sb.sptd.DataTransferLength = sizeof(DataBuf);  
    sptd_sb.sptd.TimeOutValue = 2;  
    sptd_sb.sptd.DataBuffer = (PVOID) &( DataBuf );  
    sptd_sb.sptd.SenseInfoOffset = sizeof( SCSI_PASS_THROUGH_DIRECT );   
    memcpy(sptd_sb.sptd.Cdb, cdbInq[inq_basic_info], sizeof(cdbInq[inq_basic_info]));

    ZeroMemory(DataBuf, 8);  
    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);   
     
    //Send the command to drive - request tray status for drive 
    iResult = DeviceIoControl((HANDLE) hDevice,                  // device to be queried
                               IOCTL_SCSI_PASS_THROUGH_DIRECT,   // operation to perform
                               (PVOID)&sptd_sb,
                               (DWORD)sizeof(sptd_sb),           // input buffer
                               (PVOID)&sptd_sb, 
                               (DWORD)sizeof(sptd_sb),           // output buffer
                               &dwBytesReturned,                 // # bytes returned
                               NULL);                            // synchronous I/O
     
    memcpy(DataBuf, sptd_sb.sptd.DataBuffer, sizeof(DataBuf));

    if(iResult)
    {     
        if (DataBuf[5] == 0) iResult = 0;         // DVD tray closed
        else if( DataBuf[5] == 1 ) iResult = 1;   // DVD tray open  
        else {                                    // DVD tray closed, media present
            reResult.iResult = 2;
            reResult.strResult = "   Information for drive " + drive;
            qDataBuf = QString::fromLatin1((const char *)&DataBuf[0], INQ_REPLY_LEN);
            reResult.VendorID  = qDataBuf.mid(8,8);
            reResult.ProductID = qDataBuf.mid(16,16);
            reResult.ProductRevision = qDataBuf.mid(32,4);
            reResult.strResult += "\nVendor ID:        \t" + reResult.VendorID;
            reResult.strResult += "\nProduct ID:       \t" + reResult.ProductID;
            reResult.strResult += "\nProduct Revision: \t" + reResult.ProductRevision;
        }

        strResult = dvdDriveLetter + (char)DataBuf;
	}
    else  // There's a error
    {
        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

        // Display the error message and exit the process
        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)DeviceIoControl) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("DeviceIoControl failed with error %d: %sFor drive %s\n"), dw, lpMsgBuf, (LPCWSTR)strDvdPath.utf16());
        reResult.strResult = QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
        return reResult;
    }

    /* 2 - INQUIRY command for Unit Serial Number */
    memcpy(sptd_sb.sptd.Cdb, cdbInq[inq_unit_serial_number], sizeof(cdbInq[inq_unit_serial_number]));

    ZeroMemory(DataBuf, 8);
    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

    //Send the command to drive - request tray status for drive
    iResult = DeviceIoControl((HANDLE) hDevice,                  // device to be queried
                               IOCTL_SCSI_PASS_THROUGH_DIRECT,   // operation to perform
                               (PVOID)&sptd_sb,
                               (DWORD)sizeof(sptd_sb),           // input buffer
                               (PVOID)&sptd_sb,
                               (DWORD)sizeof(sptd_sb),           // output buffer
                               &dwBytesReturned,                 // # bytes returned
                               NULL);                            // synchronous I/O

    memcpy(DataBuf, sptd_sb.sptd.DataBuffer, sizeof(DataBuf));

    if(iResult)
    {
        if (DataBuf[5] == 0) iResult = 0;         // DVD tray closed
        else if( DataBuf[5] == 1 ) iResult = 1;   // DVD tray open
        else {                                    // DVD tray closed, media present
            reResult.iResult = 2;
            qDataBuf = QString::fromLatin1((const char *)&DataBuf[0], INQ_REPLY_LEN);
            reResult.UnitSerialNumber = qDataBuf.mid(4,16);
            reResult.strResult += "\nUnit Serial Number: \t" + reResult.UnitSerialNumber;
        }

        strResult = dvdDriveLetter + (char)DataBuf;
    }
    else  // There's a error
    {
        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

        // Display the error message and exit the process
        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)DeviceIoControl) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("DeviceIoControl failed with error %d: %sFor drive %s\n"), dw, lpMsgBuf, (LPCWSTR)strDvdPath.utf16());
        reResult.strResult = QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
        return reResult;
    }

    /* 3 - READ CAPACITY command for Block Size and Disk Size */
    sptd_sb.sptd.CdbLength = READCAP_CMD_LEN;
    sptd_sb.sptd.DataTransferLength = READCAP_REPLY_LEN;
    sptd_sb.sptd.DataBuffer = (PVOID) &( DataBuf );
    memcpy(sptd_sb.sptd.Cdb, cdbCap, sizeof(cdbCap));

    ZeroMemory(DataBuf, READCAP_REPLY_LEN);
    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

    //Send the command to drive - request tray status for drive
    iResult = DeviceIoControl((HANDLE) hDevice,                  // device to be queried
                               IOCTL_SCSI_PASS_THROUGH_DIRECT,   // operation to perform
                               (PVOID)&sptd_sb,
                               (DWORD)sizeof(sptd_sb),           // input buffer
                               (PVOID)&sptd_sb,
                               (DWORD)sizeof(sptd_sb),           // output buffer
                               &dwBytesReturned,                 // # bytes returned
                               NULL);                            // synchronous I/O

    memcpy(DataBuf, sptd_sb.sptd.DataBuffer, sizeof(DataBuf));

    if(iResult)
    {
        reResult.iResult = 2;
        qDataBuf = QString::fromLatin1((const char *)&DataBuf[0], READCAP_REPLY_LEN);
        FourBytes[0] = DataBuf[7];
        FourBytes[1] = DataBuf[6];
        FourBytes[2] = DataBuf[5];
        FourBytes[3] = DataBuf[4];
        reResult.BlockSize  = *(int *)FourBytes;
        FourBytes[0] = DataBuf[3];
        FourBytes[1] = DataBuf[2];
        FourBytes[2] = DataBuf[1];
        FourBytes[3] = DataBuf[0];
        reResult.DiskSize  = ((*(int *)FourBytes) + 1) * reResult.BlockSize;
        reResult.strResult += "\nBlock Size:         \t" + QString::number(reResult.BlockSize);
        reResult.strResult += "\nDisk Size:          \t" + QString::number(reResult.DiskSize / BYTES_IN_MB) + " MB";
    }
    else  // There's a error
    {
        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

        // Display the error message and exit the process
        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)DeviceIoControl) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("DeviceIoControl failed with error %d: %sFor drive %s\n"), dw, lpMsgBuf, (LPCWSTR)strDvdPath.utf16());
        reResult.strResult = QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
        return reResult;
    }

    /* 4 - READ_10 command for reading basic information: Total MU, LBA */
    sptd_sb.sptd.CdbLength = READ10_CMD_LEN;
    sptd_sb.sptd.DataTransferLength = READ10_REPLY_LEN;
    sptd_sb.sptd.DataBuffer = (PVOID) &( ReadDataBuf );
    memcpy(sptd_sb.sptd.Cdb, cdbR10[mu_and_lba], sizeof(cdbR10[mu_and_lba]));

    ZeroMemory(ReadDataBuf, READ10_REPLY_LEN);
    ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

    //Send the command to drive - request tray status for drive
    iResult = DeviceIoControl((HANDLE) hDevice,                  // device to be queried
                               IOCTL_SCSI_PASS_THROUGH_DIRECT,   // operation to perform
                               (PVOID)&sptd_sb,
                               (DWORD)sizeof(sptd_sb),           // input buffer
                               (PVOID)&sptd_sb,
                               (DWORD)sizeof(sptd_sb),           // output buffer
                               &dwBytesReturned,                 // # bytes returned
                               NULL);                            // synchronous I/O

    memcpy(ReadDataBuf, sptd_sb.sptd.DataBuffer, sizeof(ReadDataBuf));

    if(iResult)
    {
        reResult.iResult = 2;
        qDataBuf = QString::fromLatin1((const char *)&ReadDataBuf[0], READ10_REPLY_LEN);
        reResult.Total_MU  = ReadDataBuf[1];
        FourBytes[0] = ReadDataBuf[0x17];
        FourBytes[1] = ReadDataBuf[0x16];
        FourBytes[2] = ReadDataBuf[0x15];
        FourBytes[3] = ReadDataBuf[0x14];
        reResult.Total_LBA  = *(int *)FourBytes;
        reResult.LBA_per_MU = reResult.Total_LBA / reResult.Total_MU;
        reResult.strResult += "\nTotal MU:         \t" + QString::number(reResult.Total_MU);
        reResult.strResult += "\nTotal LBA:        \t" + QString::number(reResult.Total_LBA);
        reResult.strResult += " (0x" + QString::number(reResult.Total_LBA, 16).toUpper() + ")";
        reResult.strResult += "\nLBA per MU:       \t" + QString::number(reResult.LBA_per_MU);
    }
    else  // There's a error
    {
        LPVOID lpMsgBuf;
        LPVOID lpDisplayBuf;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

        // Display the error message and exit the process
        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)DeviceIoControl) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("DeviceIoControl failed with error %d: %sFor drive %s\n"), dw, lpMsgBuf, (LPCWSTR)strDvdPath.utf16());
        reResult.strResult = QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
        return reResult;
    }

    /* 5 - READ_10 command to get Initial and Current BadBlock numbers for each MU */
    sptd_sb.sptd.CdbLength = READ10_CMD_LEN;
    sptd_sb.sptd.DataTransferLength = READBB_REPLY_LEN;
    sptd_sb.sptd.DataBuffer = (PVOID) &( RdBBDataBuf );

    /* Loop through each MU */
    for (unsigned int mu=0; mu<reResult.Total_MU; mu++)
    {
        /* Loop through each FBlk */
        for (int FBlk=0x3FF; FBlk>=0; FBlk--)
        {
            TwoBytes[0] = (FBlk >> 8) & 0x03;
            TwoBytes[1] = FBlk & 0xFF;
            ptr2Buffer = &cdbR10[init_and_current_badblocks][2];
            memcpy( ptr2Buffer, TwoBytes, 2);
            TwoBytes[0] = ((unsigned int)mu) & 0xFF;
            ptr2Buffer = &cdbR10[init_and_current_badblocks][6];
            memcpy( ptr2Buffer, TwoBytes, 1);

            memcpy(sptd_sb.sptd.Cdb, cdbR10[init_and_current_badblocks], sizeof(cdbR10[init_and_current_badblocks]));

            ZeroMemory(RdBBDataBuf, READBB_REPLY_LEN);
            ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

            //Send the command to drive - request tray status for drive
            iResult = DeviceIoControl((HANDLE) hDevice,                  // device to be queried
                                       IOCTL_SCSI_PASS_THROUGH_DIRECT,   // operation to perform
                                       (PVOID)&sptd_sb,
                                       (DWORD)sizeof(sptd_sb),           // input buffer
                                       (PVOID)&sptd_sb,
                                       (DWORD)sizeof(sptd_sb),           // output buffer
                                       &dwBytesReturned,                 // # bytes returned
                                       NULL);                            // synchronous I/O

            memcpy(RdBBDataBuf, sptd_sb.sptd.DataBuffer, sizeof(RdBBDataBuf));

            if(iResult)
            {
                reResult.iResult = 2;
                qDataBuf = QString::fromLatin1((const char *)&RdBBDataBuf[0], READBB_REPLY_LEN);

                if ((RdBBDataBuf[0x114] == 0x53) &&  /* "S" */
                    (RdBBDataBuf[0x115] == 0x4D) &&  /* "M" */
                    (RdBBDataBuf[0x116] == 0x33) &&  /* "3" */
                    (RdBBDataBuf[0x117] == 0x32) &&  /* "2" */
                    (RdBBDataBuf[0x118] == 0x35) &&  /* "5" */
                    (RdBBDataBuf[0x200] == 0xE1) &&
                    ((RdBBDataBuf[0x210] & 0x48) == 0))
                {
                    TwoBytes[0] = RdBBDataBuf[0x101];
                    TwoBytes[1] = RdBBDataBuf[0x100];
                    reResult.Current_BadBlock[mu] = *(int *)TwoBytes;
                    TwoBytes[0] = RdBBDataBuf[0x105];
                    TwoBytes[1] = RdBBDataBuf[0x104];
                    reResult.Initial_BadBlock[mu] = reResult.Current_BadBlock[mu] - (*(int *)TwoBytes);
                    TwoBytes[0] = RdBBDataBuf[0x113];
                    TwoBytes[1] = RdBBDataBuf[0x112];
                    reResult.Total_DataBlock[mu] = *(int *)TwoBytes;
                    reResult.SMIChip = qDataBuf.mid(0x114, 8);

                    reResult.strResult += "\n\nFor MU:         \t" + QString::number(mu);
                    reResult.strResult += "\nCurrent BadBlock: \t" + QString::number(reResult.Current_BadBlock[mu]);
                    reResult.strResult += " (0x" + QString::number(reResult.Current_BadBlock[mu], 16).toUpper() + ")";
                    reResult.strResult += "\nInitial  BadBlock:   \t" + QString::number(reResult.Initial_BadBlock[mu]);
                    reResult.strResult += " (0x" + QString::number(reResult.Initial_BadBlock[mu], 16).toUpper() + ")";
                    reResult.strResult += "\nTotal DataBlock:  \t" + QString::number(reResult.Total_DataBlock[mu]);
                    reResult.strResult += " (0x" + QString::number(reResult.Total_DataBlock[mu], 16).toUpper() + ")";

                    break;  // break out of for loop FBlk
                }
            }
            else  // There's a error
            {
                LPVOID lpMsgBuf;
                LPVOID lpDisplayBuf;
                DWORD dw = GetLastError();

                FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    dw,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR) &lpMsgBuf,
                    0, NULL );

                // Display the error message and exit the process
                lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
                    (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)DeviceIoControl) + 40) * sizeof(TCHAR));
                StringCchPrintf((LPTSTR)lpDisplayBuf,
                    LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                    TEXT("DeviceIoControl failed with error %d: %sFor drive %s\n"), dw, lpMsgBuf, (LPCWSTR)strDvdPath.utf16());
                reResult.strResult = QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
                return reResult;
            }

        }  /* end of for loop each FBlk */

        /* 3. Calculate Initial Spare Numbers for each MU */
        /**************************************************/
        if (mu == 0)
        {
            reResult.Initial_SpareBlock[mu] = 1014 - reResult.Total_DataBlock[mu] - reResult.Initial_BadBlock[mu];
        }
        else
        {
            reResult.Initial_SpareBlock[mu] = 1020 - reResult.Total_DataBlock[mu] - reResult.Initial_BadBlock[mu];
        }

    }  /* end of for loop each mu */

    /* 6 - READ_10 command to get Current Spare Numbers for each MU step 1 */
    sptd_sb.sptd.CdbLength = READ10_CMD_LEN;
    sptd_sb.sptd.DataTransferLength = READ10_REPLY_LEN;
    sptd_sb.sptd.DataBuffer = (PVOID) &( ReadDataBuf );

    /* Loop through each MU */
    for (unsigned int mu=0; mu<reResult.Total_MU; mu++)
    {
        SLBA = (reResult.LBA_per_MU * mu) + (reResult.LBA_per_MU / 2);

        FourBytes[0] = (SLBA >> 24) & 0xFF;
        FourBytes[1] = (SLBA >> 16) & 0xFF;
        FourBytes[2] = (SLBA >> 8) & 0xFF;
        FourBytes[3] = SLBA & 0xFF;
        ptr2Buffer = &cdbR10[current_spare_blocks_1][2];
        memcpy( ptr2Buffer, FourBytes, 4);

        memcpy(sptd_sb.sptd.Cdb, cdbR10[init_and_current_badblocks], sizeof(cdbR10[init_and_current_badblocks]));

        ZeroMemory(ReadDataBuf, READ10_REPLY_LEN);
        ZeroMemory(sptd_sb.SenseBuf, MAX_SENSE_LEN);

        //Send the command to drive - request tray status for drive
        iResult = DeviceIoControl((HANDLE) hDevice,                  // device to be queried
                                   IOCTL_SCSI_PASS_THROUGH_DIRECT,   // operation to perform
                                   (PVOID)&sptd_sb,
                                   (DWORD)sizeof(sptd_sb),           // input buffer
                                   (PVOID)&sptd_sb,
                                   (DWORD)sizeof(sptd_sb),           // output buffer
                                   &dwBytesReturned,                 // # bytes returned
                                   NULL);                            // synchronous I/O

        memcpy(ReadDataBuf, sptd_sb.sptd.DataBuffer, sizeof(ReadDataBuf));

        if(iResult)
        {
            reResult.iResult = 2;
            qDataBuf = QString::fromLatin1((const char *)&ReadDataBuf[0], READ10_REPLY_LEN);

            reResult.Current_SpareBlock[mu] = qDataBuf.mid(0x114, 1).toInt(0, 16);

            reResult.strResult += "\n\nFor MU:         \t" + QString::number(mu);
            reResult.strResult += "\nInitial SpareBlock: \t" + QString::number(reResult.Initial_SpareBlock[mu]);
            reResult.strResult += " (0x" + QString::number(reResult.Initial_SpareBlock[mu], 16).toUpper() + ")";
            reResult.strResult += "\nCurrent SpareBlock: \t" + QString::number(reResult.Current_SpareBlock[mu]);
            reResult.strResult += " (0x" + QString::number(reResult.Current_SpareBlock[mu], 16).toUpper() + ")";
        }
        else  // There's a error
        {
            LPVOID lpMsgBuf;
            LPVOID lpDisplayBuf;
            DWORD dw = GetLastError();

            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dw,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0, NULL );

            // Display the error message and exit the process
            lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
                (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)DeviceIoControl) + 40) * sizeof(TCHAR));
            StringCchPrintf((LPTSTR)lpDisplayBuf,
                LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                TEXT("\nERROR: DeviceIoControl failed with error %d: %sFor drive %s\n"), dw, lpMsgBuf, (LPCWSTR)strDvdPath.utf16());
            reResult.strResult += QString::fromWCharArray((LPCTSTR)lpDisplayBuf);
            reResult.strResult += "\nAt MU:         \t" + QString::number(mu);
            reResult.strResult += "\nSLBA :         \t" + QString::number(SLBA);
            reResult.strResult += " (0x" + QString::number(SLBA, 16).toUpper() + ")";
            reResult.strResult += "\nCDB command:   \t";
            QByteArray data = QByteArray::fromRawData((const char *)&cdbR10[init_and_current_badblocks][0], READ10_CMD_LEN);
            reResult.strResult += data.toHex() + "\n";
            CloseHandle(hDevice);
            return reResult;
        }

    }  /* end of for loop each mu */

    CloseHandle(hDevice);

    reResult.strResult += "\nSMI Chip:         \t" + reResult.SMIChip;
    reResult.strResult += "\n";
    return reResult;
}
 
QString start_main(QString drive)
{
    Result_Return returnResult;
    QString strResult = "";
    // Uses the following information to obtain the status of a DVD/CD-ROM drive:
    // 1. GetLogicalDriveStrings() to list all logical drives
    // 2. GetDriveType() to obtain the type of drive
    // 3. DeviceIoControl() to obtain the device status
    //
    returnResult = GetDvdStatus(drive);
    switch( returnResult.iResult )
    {
    case 0:
        std::cout << "DVD tray closed, no media" << std::endl;
        strResult = returnResult.strResult + "No media";
        break;
    case 1:
        std::cout << "DVD tray open" << std::endl;
        strResult = returnResult.strResult + "DVD tray open";
        break;
    case 2:
        std::cout << "DVD tray closed, media present" << std::endl;
        strResult = returnResult.strResult + "   Done";
        break;
    default:
        std::cout << "Drive not ready" << std::endl;
        strResult = returnResult.strResult + "Drive not ready\nPlease select a different drive\n";
        break;
    }
             
    return strResult;
}
