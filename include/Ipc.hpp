#define NOMINMAX
#include <windows.h>
#include <iostream>

constexpr int BUFF_SIZE = 256;
TCHAR szName[] = TEXT("Global\\CompositorRegistry");

bool AccessShaderMemory()
{
	HANDLE hMapFile;
	LPCTSTR pBuf;

	hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, szName);

	if (hMapFile)
	{
		std::cerr << "Failed to acquire shader memory access." << std::endl;
		return false;
	}

	pBuf = (LPCTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, BUFF_SIZE);

	if (pBuf == NULL)
	{
		std::cerr << "Failed to map shader memory." << std::endl;
		CloseHandle(hMapFile);
		return false;
	}

	UnmapViewOfFile(pBuf);

	CloseHandle(hMapFile);

	return true;
}