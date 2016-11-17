#include "NvAPI.h"
#include "pluginManager.h"

namespace nvidia {

NvAPI::~NvAPI()
{
	if (hDLL)
	{
		(*NvAPI_Unload)();
		FreeLibrary(hDLL);
	}
}

bool NvAPI::initialize()
{
	hDLL = LoadLibraryW(TEXT("nvapi.dll"));
	if (hDLL == NULL)
	{
		commondll::PluginManager::instance().displayError(L"Load NvAPI DLL failed.");

		return false;
	}

	NvAPI_QueryInterface = (NvAPI_QueryInterface_t)GetProcAddress(hDLL, "nvapi_QueryInterface");
	NvAPI_Initialize = (NvAPI_Initialize_t)(*NvAPI_QueryInterface)(0x0150E828);
	NvAPI_EnumNvidiaDisplayHandle = (NvAPI_EnumNvidiaDisplayHandle_t)(*NvAPI_QueryInterface)(0x9ABDD40D);
	NvAPI_EnumPhysicalGPUs = (NvAPI_EnumPhysicalGPUs_t)(*NvAPI_QueryInterface)(0xE5AC921F);
	NvAPI_GPU_GetFullName = (NvAPI_GPU_GetFullName_t)(*NvAPI_QueryInterface)(0xCEEE8E9F);
	NvAPI_GetAssociatedNvidiaDisplayName = (NvAPI_GetAssociatedNvidiaDisplayName_t)(*NvAPI_QueryInterface)(0x22A78B05);
	NvAPI_GetDVCInfoEx = (NvAPI_GetDVCInfoEx_t)(*NvAPI_QueryInterface)(0x0E45002D);
	NvAPI_SetDVCLevelEx = (NvAPI_SetDVCLevelEx_t)(*NvAPI_QueryInterface)(0x4A82C2B1);
	NvAPI_GPU_GetCoolerSettings = (NvAPI_GPU_GetCoolerSettings_t)(*NvAPI_QueryInterface)(0xDA141340);
	NvAPI_GPU_SetCoolerLevels = (NvAPI_GPU_SetCoolerLevels_t)(*NvAPI_QueryInterface)(0x891FA0AE);
	NvAPI_Unload = (NvAPI_Unload_t)(*NvAPI_QueryInterface)(0xD22BDD7E);

	_NvAPI_Status status = (_NvAPI_Status)(*NvAPI_Initialize)();
	if (status != NVAPI_OK)
	{
		commondll::PluginManager::instance().displayError(L"NvAPI initialization failed.");

		return false;
	}

	return true;
}

int NvAPI::enumNvidiaDisplayHandle(int thisEnum, int* pNvDispHandle)
{
	int dispCount = 0;

	int enumNvDispHandle;

	_NvAPI_Status status = NVAPI_OK;
	while (status != NVAPI_END_ENUMERATION)
	{
		status = (_NvAPI_Status)(*NvAPI_EnumNvidiaDisplayHandle)(dispCount, &enumNvDispHandle);
		if (status == NVAPI_OK)
		{
			if (dispCount == thisEnum)
				*pNvDispHandle = enumNvDispHandle;

			++dispCount;
		}
		else if (status != NVAPI_END_ENUMERATION)
			commondll::PluginManager::instance().displayError(L"NvAPI display handle enumeration failed.");
	}

	return dispCount;
}

char* NvAPI::getAssociatedNvidiaDisplayName(int nDisp, char* szDispName)
{
	int nvDispHandle;
	enumNvidiaDisplayHandle(nDisp, &nvDispHandle);

	(*NvAPI_GetAssociatedNvidiaDisplayName)(nvDispHandle, szDispName);

	return szDispName;
}

int NvAPI::enumPhysicalGPUs(int** nvGPUhandle)
{
	int gpuCount;

	_NvAPI_Status status = (_NvAPI_Status)(*NvAPI_EnumPhysicalGPUs)(nvGPUhandle, &gpuCount);
	if (status != NVAPI_OK)
		commondll::PluginManager::instance().displayError(L"NvAPI physical GPU enumeration failed.");

	return gpuCount;
}

NV_DISPLAY_DVC_INFO_EX NvAPI::getDvcInfoEx(int nDisp)
{
	NV_DISPLAY_DVC_INFO_EX info;

	int nvDispHandle;
	enumNvidiaDisplayHandle(nDisp, &nvDispHandle);

	info.version = sizeof(NV_DISPLAY_DVC_INFO_EX) | 0x10000;
	(*NvAPI_GetDVCInfoEx)(nvDispHandle, 0, &info);

	return info;
}

NV_GPU_COOLER_SETTINGS NvAPI::gpu_GetCoolerSettings(int nGPU)
{
	NV_GPU_COOLER_SETTINGS settings;

	int* nvGPUhandle[NVAPI_MAX_PHYSICAL_GPUS];
	
	enumPhysicalGPUs(nvGPUhandle);

	settings.version = sizeof(NV_GPU_COOLER_SETTINGS) | 0x30000;
	(*NvAPI_GPU_GetCoolerSettings)(nvGPUhandle[nGPU], 7, &settings);

	return settings;
}

char* NvAPI::gpu_GetFullName(int nGPU, char* szName)
{
	int* nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS];
	enumPhysicalGPUs(nvGPUHandle);

	(*NvAPI_GPU_GetFullName)(nvGPUHandle[nGPU], szName);

	return szName;
}

bool NvAPI::setDvcInfoEx(int nDisp, int level)
{
	int NvDispHandle;
	enumNvidiaDisplayHandle(nDisp, &NvDispHandle);

	NV_DISPLAY_DVC_INFO_EX oldInfo = getDvcInfoEx(nDisp);

	NV_DISPLAY_DVC_INFO_EX info;
	info.version = oldInfo.version;
	info.currentLevel = level;
	info.minLevel = oldInfo.minLevel;
	info.maxLevel = oldInfo.maxLevel;
	info.defaultLevel = oldInfo.defaultLevel;

	_NvAPI_Status status = (_NvAPI_Status)(*NvAPI_SetDVCLevelEx)(NvDispHandle, 0, &info);
	if (status != NVAPI_OK)
		return false;

	return true;
}

bool NvAPI::gpu_SetCoolerLevels(int nGPU, int nCooler, int newLevel)
{
	int* nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS];
	enumPhysicalGPUs(nvGPUHandle);

	NV_GPU_COOLER_SETTINGS gpuCoolerSettings = gpu_GetCoolerSettings(nGPU);

	NV_GPU_COOLER_LEVELS gpuCoolerLevels;
	gpuCoolerLevels.version = sizeof(NV_GPU_COOLER_LEVELS) | 0x10000;

	for (unsigned int x = 0; x < gpuCoolerSettings.count; ++x)
	{
		gpuCoolerLevels.levels[x].level = gpuCoolerSettings.coolers[x].currentLevel;
		gpuCoolerLevels.levels[x].policy = gpuCoolerSettings.coolers[x].currentPolicy;
	}

	gpuCoolerLevels.levels[nCooler].level = newLevel;
	gpuCoolerLevels.levels[nCooler].policy = 1;

	_NvAPI_Status status = (_NvAPI_Status)(*NvAPI_GPU_SetCoolerLevels)(nvGPUHandle[nGPU], 7, &gpuCoolerLevels);
	if (status != NVAPI_OK)
		return false;

	return true;
}

} // namespace nvidia