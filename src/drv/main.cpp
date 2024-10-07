#include <ntifs.h>
#include <kioctl.hpp>

kstd::Device* g_dev = nullptr;

constexpr auto CTL_KRWPA = CTL_CODE(0x8000, 0x0823, METHOD_BUFFERED, FILE_ANY_ACCESS);

// read write phisics address struct
struct RWPA
{
	ULONG64 va;
	ULONG64 pa;
	ULONG64 size;
	ULONG64 is_write;
};
static_assert(sizeof(RWPA) == 32, "RWPA size is not 32 bytes");
// assert the struct size is 32 bytes in compile time

void DriverUnload(PDRIVER_OBJECT drv)
{
	UNREFERENCED_PARAMETER(drv);
	if (g_dev)
	{
		delete g_dev;
		g_dev = nullptr;
	}
}

NTSTATUS ioctl(PDEVICE_OBJECT dev, PIRP irp)
{
	UNREFERENCED_PARAMETER(dev);

	auto stack          = IoGetCurrentIrpStackLocation(irp);
	auto infomation     = 0ull;
	auto status         = STATUS_SUCCESS;
	auto map_addr_by_pa = (void*)nullptr;
	auto map_size       = 0ull;
	do
	{
		if (*KdDebuggerEnabled)
		{
			__debugbreak();
		}

		switch (stack->Parameters.DeviceIoControl.IoControlCode)
		{
		case CTL_KRWPA:
		{
			// args check
			if (irp->AssociatedIrp.SystemBuffer == nullptr || stack->Parameters.DeviceIoControl.InputBufferLength != sizeof(RWPA))
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			auto arg = reinterpret_cast<RWPA*>(irp->AssociatedIrp.SystemBuffer);
			// anyway,this method to check the address is valid is not good
			if (!MmIsAddressValid((PCHAR)arg->va))
			{
				status = STATUS_INVALID_ADDRESS;
				break;
			}
			// and map the pa to va
			map_size       = arg->size;
			map_addr_by_pa = MmMapIoSpace({ .QuadPart = (long long)arg->pa }, map_size, MmNonCached);
			if (!map_addr_by_pa)
			{
				DbgPrintEx(77, 0, "[RWPA]:MmMapIoSpace failed,input address is %llx\n", arg->pa);
				break;
			}
			// then check is write or read
			auto copy_to   = arg->is_write ? map_addr_by_pa : (void*)arg->va;
			auto copy_from = arg->is_write ? (void*)arg->va : map_addr_by_pa;

			// it's seems like can not copy with xmm register?
			// test

			// copy by byte sequence
			for (auto i = 0ull; i < map_size; i++)
			{
				((unsigned char*)copy_to)[i] = ((unsigned char*)copy_from)[i];
			}

			// copy by memcpy with xmm register
			memcpy(copy_to, copy_from, map_size);

			DbgPrintEx(77, 0, "[RWPA]:%s physical address %llx success!\n", arg->is_write ? "write" : "read", arg->pa);
			break;
		}
		default:
		{
			status = STATUS_NOT_IMPLEMENTED;
			break;
		}
		}
	} while (false);

	// clean up
	if (map_addr_by_pa)
	{
		MmUnmapIoSpace(map_addr_by_pa, map_size);
	}
	irp->IoStatus.Information = infomation;
	irp->IoStatus.Status      = status;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return status;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING)
{
	NTSTATUS status   = STATUS_SUCCESS;
	drv->DriverUnload = DriverUnload;
	do
	{
		g_dev = new kstd::Device(drv);
		if (!g_dev)
		{
			DbgPrintEx(77, 0, "[RWPA]:new kstd::Device failed\n");
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		status = g_dev->create();
		if (!NT_SUCCESS(status))
		{
			DbgPrintEx(77, 0, "[RWPA]:g_dev->create failed\n");
			break;
		}

		g_dev->set_callback(IRP_MJ_DEVICE_CONTROL, &ioctl);
	} while (false);

	// clean up
	if (!NT_SUCCESS(status))
	{
		if (g_dev)
		{
			delete g_dev;
			g_dev = nullptr;
		}
	}
	return status;
}