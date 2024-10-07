
#pragma once
#ifndef _KIOCTL_HPP_
#define _KIOCTL_HPP_

///
/// @file kioctl.hpp
/// @brief KIOCTL & UIOCTL 
/// @author oxygen
/// @version 1.0
/// @date 2024-08-17

#ifdef _KERNEL_MODE
#include <fltKernel.h>
#include <ntstrsafe.h>
#else
#include <windows.h>
#include <string>
#include <filesystem>
#include <chrono>
#include <random>
#endif


namespace kstd {

#define DEFAULT_DEV_NAME L"\\Device\\kioct1_3e87d"
#define DEFAULT_SYB_NAME L"\\??\\kioct1_3e87d"
#define DEFAULT_SERVICE_NAME L"kioct1_" //服务名称是随机的
#define DEFAULT_R3_DEV_NAME L"\\\\.\\kioct1_3e87d"

#ifdef _KERNEL_MODE


	const unsigned long IOCTL_POOL_TAG = 'ioc1';
	/*可以不报错误地 可以用kalloc<char> 替换ExAllocatePoolWithTag*/
	template<typename T>
	static inline T* kalloc(POOL_TYPE pool_type, SIZE_T size = 0, ULONG tag = IOCTL_POOL_TAG) {

		auto func_name = UNICODE_STRING{};
		auto func = (void*)(nullptr);
		auto ret = (void*)(nullptr);

		RtlInitUnicodeString(&func_name, L"ExAllocatePoolZero");
		func = MmGetSystemRoutineAddress(&func_name);
		if (size == 0) {
			size = sizeof(T);
		}

		if (func) {

			auto f = reinterpret_cast<void* (*)(POOL_TYPE, SIZE_T, ULONG)>(func);

			ret = f(pool_type, size, tag);
		}
		else {
			/*低版本windows*/
			RtlInitUnicodeString(&func_name, L"ExAllocatePoolWithTag");
			func = MmGetSystemRoutineAddress(&func_name);
			
			auto f = reinterpret_cast<void* (*)(POOL_TYPE, SIZE_T, ULONG)>(func);
			
			/*如果连这个函数找不到蓝屏是正常的*/
			ret = f(pool_type, size, tag);
		}

		return reinterpret_cast<T*>(ret);
	}


	template<POOL_TYPE pool_type, ULONG pool_tag>
	class DeviceBase {
	public:
		void* operator new(size_t size) {
			return reinterpret_cast<void*>(kalloc<char>(pool_type, size));
		}
		void operator delete(void* p, size_t size) {
			UNREFERENCED_PARAMETER(size);
			ExFreePool(p);
		}
	};

	/*单例模式*/
	class Device : public DeviceBase<NonPagedPool, IOCTL_POOL_TAG> {
	public:
		Device(PDRIVER_OBJECT drv);
		~Device();
		//static Device* get_instance(PDRIVER_OBJECT drv);
		//static void destory();
		Device(const Device& rhs) = delete;
		Device& operator= (const Device& rhs) = delete;

		NTSTATUS create(const wchar_t* dev_name = DEFAULT_DEV_NAME, const wchar_t* syb_name = DEFAULT_SYB_NAME);
		void set_callback(unsigned long mj_func_code, NTSTATUS(*callback)(PDEVICE_OBJECT dev, PIRP irp));
		
		PDEVICE_OBJECT get() const { return __dev; }
	private:
		static NTSTATUS default_dispatcher(PDEVICE_OBJECT dev, PIRP irp);
	private:
		static Device* __instance;
		PDRIVER_OBJECT __drv = nullptr;
		PDEVICE_OBJECT __dev = nullptr;
		PUNICODE_STRING __syb_name = nullptr;

		//static PDRIVER_DISPATCH __my_dispatcher[IRP_MJ_MAXIMUM_FUNCTION];
		//static void* __args[IRP_MJ_MAXIMUM_FUNCTION];
	};

	//inline void Device::destory()
	//{
	//	if (__instance) {
	//		__instance->~Device();
	//	}
	//}

	//inline Device* Device::get_instance(PDRIVER_OBJECT drv)
	//{
	//	if (!__instance) {
	//		__instance = new Device(drv);
	//	}
	//	return __instance;
	//}

	inline kstd::Device::Device(PDRIVER_OBJECT drv) :__drv(drv), __dev(nullptr), __syb_name(nullptr)
	{
	}

	inline Device::~Device()
	{
		if (__drv) {
			if (__syb_name) {

				IoDeleteSymbolicLink(__syb_name);
				ExFreePool(__syb_name);
				__syb_name = nullptr;
			}

			if (__dev) {
				IoDeleteDevice(__dev);
			}
		}

		__drv = nullptr;
		__syb_name = nullptr;
		__dev = nullptr;
	}

	inline NTSTATUS Device::create(const wchar_t* dev_name, const wchar_t* syb_name)
	{
		auto status = STATUS_UNSUCCESSFUL;
		auto u_dev_name = UNICODE_STRING{};
		auto u_syb_name = UNICODE_STRING{};
		do {

			if (!dev_name || !syb_name || !__drv) {

				status = STATUS_INVALID_PARAMETER;
				break;
			}

			RtlInitUnicodeString(&u_dev_name, dev_name);
			status = IoCreateDevice(__drv, 0, &u_dev_name, FILE_DEVICE_UNKNOWN, 0, 0, &__dev);
			if (!NT_SUCCESS(status)) {
				break;
			}

			/*自己持有symbol_name,需要申请内存,这个内存前面是UNICODE_STRING 后面是字符串*/
			__syb_name = reinterpret_cast<PUNICODE_STRING>(kalloc<char>(NonPagedPool, \
				(wcslen(syb_name) + 1) * sizeof(wchar_t) + sizeof(UNICODE_STRING) + 10/*多出来点字节*/));
			if (!__syb_name) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			wcscpy((wchar_t*)((ULONG_PTR)__syb_name + sizeof(UNICODE_STRING)), syb_name);
			RtlInitUnicodeString(__syb_name, reinterpret_cast<wchar_t*>((char*)__syb_name + sizeof(UNICODE_STRING)));
			status = IoCreateSymbolicLink(__syb_name, &u_dev_name);
			
			if (!NT_SUCCESS(status)) {
				break;
			}

			/*success*/
			__drv->MajorFunction[IRP_MJ_CREATE] = __drv->MajorFunction[IRP_MJ_CLOSE] = default_dispatcher;

		} while (false);

		/*失败 clean up*/
		if (!NT_SUCCESS(status)) {

			if (__dev) {
				IoDeleteDevice(__dev);
				__dev = nullptr;
			}


			if (__syb_name) {
				IoDeleteSymbolicLink(__syb_name);
				ExFreePool(__syb_name);
				__syb_name = nullptr;
			}
		}

		return status;
	}

	inline void Device::set_callback(unsigned long mj_func_code, NTSTATUS(*callback)(PDEVICE_OBJECT dev, PIRP irp))
	{

		if (__drv && mj_func_code < IRP_MJ_MAXIMUM_FUNCTION) {
			__drv->MajorFunction[mj_func_code] = callback;
		}

	}

	inline NTSTATUS Device::default_dispatcher(PDEVICE_OBJECT dev, PIRP irp)
	{
		UNREFERENCED_PARAMETER(dev);

		irp->IoStatus.Information = 0;
		irp->IoStatus.Status = 0;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}





#else

	class Driver {

	public:
		Driver(const std::wstring& driver_path, const std::wstring& service_name = DEFAULT_SERVICE_NAME, const std::wstring& dev_name = DEFAULT_R3_DEV_NAME);
		int start(); /*启动驱动*/
		bool stop(); /*Stop驱动*/
		bool ioctl(ULONG ctl_code, void* inbuf, ULONG isize, void* outbuf, ULONG osize);

		~Driver() = default;
	private:
		void enable_privileges() {

			HANDLE hToken;
			LUID sedebugnameValue;
			TOKEN_PRIVILEGES tkp;
			if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
				return;
			}
			if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue))
			{
				CloseHandle(hToken);
				return;
			}
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Luid = sedebugnameValue;
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL)) {
				CloseHandle(hToken);
				return;
			}

			return;
			
		}
	private:
		HANDLE __hdev;
		SC_HANDLE __hsrc = nullptr;
		SC_HANDLE __hmgr = nullptr;
		std::wstring __driver_path;/*驱动的完整路径*/
		std::wstring __service_name;/*驱动服务名称*/
		std::wstring __dev_name;

	};

	/*负责转换成绝对路径*/
	inline kstd::Driver::Driver(const std::wstring& driver_path, const std::wstring& service_name, const std::wstring& dev_name)
		: __driver_path(driver_path), __dev_name(dev_name), __hdev(INVALID_HANDLE_VALUE), __service_name(service_name)
	{
		std::filesystem::path  path(driver_path);
		std::filesystem::path asb_path;
		if (!path.is_absolute()) {
			asb_path = std::filesystem::absolute(path);
			__driver_path = asb_path.c_str();
		}

		/*随便生成几个字符，确保启动的时候不一样即可*/
		if (__service_name == DEFAULT_SERVICE_NAME) {
			std::wstring result;
			std::wstring characters = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
			std::random_device rd;
			std::mt19937 generator(rd());
			std::uniform_int_distribution<int> distribution(0, characters.size() - 1);

			for (int i = 0; i < 8; ++i) {
				result += characters[distribution(generator)];
			}

			__service_name += result;
		}
	}



	inline int Driver::start()
	{
		auto err_code = 0l;

		/*提升管理员权限*/
		enable_privileges();

		do {

			__hmgr = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS); //SC_MANAGER_CREATE_SERVICE
			if (__hmgr == INVALID_HANDLE_VALUE || __hmgr == 0) {
				err_code = GetLastError();
				break;
			}

			__hsrc = CreateServiceW(__hmgr,
				__service_name.c_str(), 
				__service_name.c_str(), 
				SERVICE_ALL_ACCESS, // 加载驱动程序的访问权限  SERVICE_START 或者 SERVICE_ALL_ACCESS
				SERVICE_KERNEL_DRIVER,// 表示加载的服务是驱动程序  
				SERVICE_DEMAND_START, // 注册表驱动程序的 Start 值   //指定当进程调用StartService函数时由服务控制管理器启动的服务。
				SERVICE_ERROR_NORMAL,//SERVICE_ERROR_IGNORE, // 注册表驱动程序的 ErrorControl 值  
				__driver_path.c_str(), // GetFullPathNameA szDriverImagePath 注册表驱动程序的路径 如: C:\\222\1.sys
				NULL,  //GroupOrder HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GroupOrderList
				NULL,
				NULL,
				NULL,
				NULL);


			if (GetLastError() == ERROR_SERVICE_EXISTS) //ERROR_SERVICE_EXISTS 1073 //服务已经存在
			{
				__hsrc = OpenServiceW(__hmgr, __service_name.c_str(), SERVICE_ALL_ACCESS);////或者 SERVICE_ALL_ACCESS //

			}


			if (__hsrc == INVALID_HANDLE_VALUE || __hsrc == 0) {
				err_code = GetLastError();
				break;
			}

			if (!StartServiceW(__hsrc, NULL, NULL)) {
				break;
			}

		} while (false);

		if (err_code != ERROR_SUCCESS) {
			/*fail*/
			if (__hsrc != INVALID_HANDLE_VALUE && __hsrc != 0) CloseServiceHandle(__hsrc);
			if (__hmgr != INVALID_HANDLE_VALUE && __hmgr != 0) CloseServiceHandle(__hmgr);
		}

		return err_code;
	}

	inline bool Driver::stop()
	{
		BOOL bRet = FALSE;
		SERVICE_STATUS SvrSta;

		/*关闭句柄*/
		if (__hdev != INVALID_HANDLE_VALUE && __hdev != 0) {
			CloseHandle(__hdev);
			__hdev = nullptr;
		}

		if (!ControlService(__hsrc, SERVICE_CONTROL_STOP, &SvrSta))
		{
			//printf("[-]stop error:%d\r\n", GetLastError());
			bRet = FALSE;
		}

		//动态卸载驱动程序,删除服务  
		if (!DeleteService(__hsrc))
		{
			bRet = FALSE;
		}

		
		if (__hsrc)
		{
			CloseServiceHandle(__hsrc);
			__hsrc = nullptr;
		}
		if (__hmgr)
		{
			CloseServiceHandle(__hmgr);
			__hmgr = nullptr;
		}

		bRet = TRUE;

		return bRet;
	}

	inline bool Driver::ioctl(ULONG ctl_code, void* inbuf, ULONG isize, void* outbuf, ULONG osize)
	{
		if (__hdev == INVALID_HANDLE_VALUE) {
			
			__hdev = CreateFileW(__dev_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
			
		}

		return DeviceIoControl(__hdev, (ULONG)ctl_code, inbuf, isize, outbuf, osize, nullptr, 0);
	}





#endif






}

#endif
