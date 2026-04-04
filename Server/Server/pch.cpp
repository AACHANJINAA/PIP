#include "pch.h"

void PIP::print_error(const char* msg, int err_no)
{
	if (WSA_IO_PENDING == err_no)
	{
		return;
	}
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L" 에러 " << lpMsgBuf << std::endl;
#ifdef _DEBUG
	while (true){}; // 디버깅 용 그냥 죽으면 안되니까
#endif

	LocalFree(lpMsgBuf);
}

namespace PIP::SERVER
{

}
