#include <efi.h>
#include <efilib.h>

#define LINUX_PATH L"hello.efi"
#define ARGS L" root=PARTUUID=36e564d3-ebe9-e64c-b366-7091842906c7 resume=PARTUUID=3edb89e3-e195-1e49-a0af-582b2852e391 rw initrd=\\intel-ucode.img initrd=\\initramfs-linux-zen.img"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable){
    //Defining global variables and the status variable to re-use when calling functions
    EFI_GUID fsproto = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID dpproto = EFI_DEVICE_PATH_PROTOCOL_GUID;
    EFI_GUID strtodpproto = EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID;
    EFI_GUID dputilsproto = EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID;
    EFI_GUID liproto = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_STATUS status = EFI_SUCCESS;
    EFI_DEVICE_PATH* filepath = NULL;
    EFI_DEVICE_PATH* fullpath = NULL;
    InitializeLib(ImageHandle, SystemTable);

    //Creating the device path for the file path of linux
    EFI_HANDLE* strtodphandles = NULL;
    UINTN strtodphandle_num = 0;
    EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL* strtodp;
    status = uefi_call_wrapper(BS->LocateHandleBuffer,5,ByProtocol,&strtodpproto,NULL,&strtodphandle_num,&strtodphandles);
    if(status != EFI_SUCCESS) Print(L"Failed to get strtodp: %u\n",status);
    status = uefi_call_wrapper(BS->HandleProtocol,3,strtodphandles[0],&strtodpproto,&strtodp);
    if(status != EFI_SUCCESS) Print(L"Failed to get strtodp: %u\n",status);
    filepath = (void*)uefi_call_wrapper(strtodp->ConvertTextToDevicePath,1,LINUX_PATH);
    if(filepath == NULL) Print(L"Out of memory\n");
    Print(L"Filepath: %s\n",DevicePathToStr(filepath));
    uefi_call_wrapper(BS->FreePool,1,strtodphandles);
    

    //Locating all handles supporting the fs protocol, assuming one of them is the ESP
    EFI_HANDLE* fshandles = NULL;
    UINTN fshandle_num = 0;
    status = uefi_call_wrapper(BS->LocateHandleBuffer,5,ByProtocol,&fsproto,NULL,&fshandle_num,&fshandles);
    if(status != EFI_SUCCESS) Print(L"Failed to locate filesystems: %u\n",status);
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = NULL;
    EFI_DEVICE_PATH* fspath = NULL;
    EFI_FILE_PROTOCOL* rootdir = NULL;
    BOOLEAN found = FALSE;
    for(UINTN i = 0; i<fshandle_num; i++){
        status = uefi_call_wrapper(BS->HandleProtocol,3,fshandles[i],&fsproto,&fs);
        if(status != EFI_SUCCESS) Print(L"Failed to open fs%u: %u\n",i,status);
        status = uefi_call_wrapper(BS->HandleProtocol,3,fshandles[i],&dpproto,&fspath);
        if(status != EFI_SUCCESS) Print(L"Failed to get fs%u device path: %u\n",i,status);
        Print(L"Found fs%u: %s\n",i,DevicePathToStr(fspath));
        status = uefi_call_wrapper(fs->OpenVolume,2,fs,&rootdir);
        if(status != EFI_SUCCESS) Print(L"Failed to open fs%u root: %u\n",i,status);
        EFI_FILE_PROTOCOL* linuxfile;
        status = uefi_call_wrapper(rootdir->Open,5,rootdir,&linuxfile,LINUX_PATH,EFI_FILE_MODE_READ,EFI_FILE_READ_ONLY);
        if(status != EFI_SUCCESS){
            if(status == EFI_NOT_FOUND) continue;
            Print(L"Couldn't open linux file: %u\n",status);
        } else if(status == EFI_SUCCESS){
            status = uefi_call_wrapper(linuxfile->Close,1,linuxfile);
            if(status != EFI_SUCCESS) Print(L"Failed to close linux image: %u\n");
            found = TRUE;
            break;
        }

    }
    uefi_call_wrapper(BS->FreePool,1,fshandles);
    if(found == FALSE) Print(L"Couldn't find linux binary\n");

    //Create full device path
    EFI_HANDLE* dputilshandles = NULL;
    UINTN dputilshandle_num = 0;
    status = uefi_call_wrapper(BS->LocateHandleBuffer,5,ByProtocol,&dputilsproto,NULL,&dputilshandle_num,&dputilshandles);
    if(status != EFI_SUCCESS) Print(L"Failed to get dp utils: %u\n",status);
    EFI_DEVICE_PATH_UTILITIES_PROTOCOL* dputils;
    status = uefi_call_wrapper(BS->HandleProtocol,3,dputilshandles[0],&dputilsproto,&dputils);
    if(status != EFI_SUCCESS) Print(L"Failed to get dp utils: %u\n",status);
    uefi_call_wrapper(BS->FreePool,1,dputilshandles);
    fullpath = (void*)uefi_call_wrapper(dputils->AppendDevicePath,2,fspath,filepath);
    if(fullpath == NULL) Print(L"Out of memory");
    Print(L"Full path: %s\n",DevicePathToStr(fullpath));
   
    //Chainload
    UINTN exitdatasize = 0;
    EFI_HANDLE linuxhandle;
    EFI_LOADED_IMAGE_PROTOCOL* linuxli;
    status = uefi_call_wrapper(BS->LoadImage,6,FALSE,ImageHandle,fullpath,NULL,NULL,&linuxhandle);
    if(status != EFI_SUCCESS) Print(L"Failed to load linux image: %u\n",status);
    status = uefi_call_wrapper(BS->HandleProtocol,3,linuxhandle,&liproto,&linuxli);
    if(status != EFI_SUCCESS) Print(L"Failed to get loaded image protocol: %u\n",status);
    UINTN optionslen = StrLen(LINUX_PATH ARGS)*sizeof(CHAR16)+2;
    CHAR16* options = AllocateZeroPool(optionslen);
    StrCpy(options,LINUX_PATH ARGS);
    linuxli->LoadOptions = options;
    linuxli->LoadOptionsSize = optionslen;
    status = uefi_call_wrapper(BS->StartImage,3,linuxhandle,&exitdatasize,NULL);
    if(status != EFI_SUCCESS) Print(L"Failed to start linux image: %u\n",status);
    FreePool(linuxhandle);

    return EFI_SUCCESS;
}
