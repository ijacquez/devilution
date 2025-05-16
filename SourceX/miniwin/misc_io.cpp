#include <sys/stat.h>
#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>

#include "devilution.h"
#include "stubs.h"

namespace dvl {

struct memfile {
	char path[DVL_MAX_PATH]{'\0'};
	char* buf = nullptr;
	char* buf_top = nullptr;
	size_t buf_size = 0;
	size_t pos = 0;

	struct memfile* next = nullptr;
};

static memfile* files;
static memfile* files_tail;

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	UNIMPLEMENTED_UNLESS(!(dwDesiredAccess & ~(DVL_GENERIC_READ | DVL_GENERIC_WRITE)));
	memfile* file = static_cast<memfile*>(malloc(sizeof(memfile)));
	// Can't call memfile constructor without depending on using placement new
	memset(static_cast<void*>(file), 0, sizeof(memfile));
	assert(file != nullptr);

	TranslateFileName(file->path, sizeof(file->path), lpFileName);
	DUMMY_PRINT("file: %s (%s)", lpFileName, file->path);

	if (dwCreationDisposition == DVL_OPEN_EXISTING) {
		// read contents of existing file into buffer
		FILE* fp = fopen(file->path, "rb");
		assert(fp != nullptr);

		const int fd = fileno(fp);
		struct stat stat_buffer;
		fstat(fd, &stat_buffer);

		file->buf = static_cast<char*>(malloc(stat_buffer.st_size));
		file->buf_top = file->buf;
		assert(file->buf != nullptr);

		file->buf_size = stat_buffer.st_size;

		const size_t bytes_read = fread(file->buf, 1, file->buf_size, fp);

		assert(bytes_read == file->buf_size);
		fclose(fp);
	} else if (dwCreationDisposition == DVL_CREATE_ALWAYS) {
		// start with empty file
	} else {
		UNIMPLEMENTED();
	}
	if (files == nullptr) {
		files = file;
		files_tail = file;
	} else {
		files_tail->next = file;
		files_tail = file;
	}
	return file;
}

WINBOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
                 LPOVERLAPPED lpOverlapped)
{
	memfile* file = static_cast<memfile*>(hFile);
	UNIMPLEMENTED_UNLESS(!lpOverlapped);
	const size_t len = MIN(file->buf_size - file->pos, nNumberOfBytesToRead);

	const char * const read_buffer = file->buf + file->pos;

	memcpy(lpBuffer, read_buffer, len);
	file->pos += len;

	*lpNumberOfBytesRead = len;
	return true;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
	memfile* file = static_cast<memfile*>(hFile);
	return file->buf_size;
}

WINBOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                  LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	memfile* file = static_cast<memfile*>(hFile);
	UNIMPLEMENTED_UNLESS(!lpOverlapped);
	if(!nNumberOfBytesToWrite)
		return true;

	if(file->buf_size < file->pos + nNumberOfBytesToWrite) {
		const size_t buffer_size = file->pos + nNumberOfBytesToWrite;
		file->buf = static_cast<char*>(realloc(file->buf_top, buffer_size));
		file->buf_top = file->buf;
		assert(file->buf != nullptr);

		file->buf_size = buffer_size;
	}

	void* const write_buf = file->buf + file->pos;
	memcpy(write_buf, lpBuffer, nNumberOfBytesToWrite);

	file->pos += nNumberOfBytesToWrite;
	*lpNumberOfBytesWritten = nNumberOfBytesToWrite;
	return true;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	memfile* file = static_cast<memfile*>(hFile);
	UNIMPLEMENTED_UNLESS(!lpDistanceToMoveHigh);
	if (dwMoveMethod == DVL_FILE_BEGIN) {
		file->pos = lDistanceToMove;
	} else if (dwMoveMethod == DVL_FILE_CURRENT) {
		file->pos += lDistanceToMove;
	} else {
		UNIMPLEMENTED();
	}

	if(file->buf_size < file->pos + 1) {
		const size_t buffer_size = file->pos + 1;

		file->buf = static_cast<char*>(realloc(file->buf_top, buffer_size));
		assert(file->buf != nullptr);

		file->buf_top = file->buf;
		file->buf_size = buffer_size;
	}

	return file->pos;
}

WINBOOL SetEndOfFile(HANDLE hFile)
{
	memfile* file = static_cast<memfile*>(hFile);
	file->buf = file->buf + file->pos;

	return true;
}

DWORD GetFileAttributesA(LPCSTR lpFileName)
{
	char name[DVL_MAX_PATH];
	TranslateFileName(name, sizeof(name), lpFileName);

	if (access(name, F_OK) != 0) {
		SetLastError(DVL_ERROR_FILE_NOT_FOUND);
		return (DWORD)-1;
	}
	return 0x80;
}

WINBOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes)
{
	return true;
}

WINBOOL CloseHandle(HANDLE hObject)
{
	memfile* file = static_cast<memfile*>(hObject);

	memfile* p = files;
	memfile* p_prev = nullptr;
	bool found_handle = false;
	while (p != nullptr) {
		if (p == file) {
			found_handle = true;

			if (p_prev == nullptr) {
				files = p->next;

				if ((files != nullptr) && (files->next == nullptr)) {
					files_tail = files;
				}
			} else {
				p_prev->next = p->next;

				if (p_prev->next == nullptr) {
					files_tail = p_prev;
				}
			}
			break;
		}

		p_prev = p;
		p = p->next;
	}

	if (!found_handle)
		return true;

	//svcOutputDebugString(file->path.c_str(),200);
	truncate(file->path, 0);
	FILE* fp = fopen(file->path, "wb");
	if (fp == nullptr) {
		DialogBoxParam(ghInst, DVL_MAKEINTRESOURCE(IDD_DIALOG7), ghMainWnd, (DLGPROC)FuncDlg, (LPARAM)file->path);
	}
	assert(fp != nullptr);

	const size_t write_size = fwrite(file->buf, 1, file->buf_size, fp);
	fclose(fp);

	if (write_size != file->buf_size) {
		DialogBoxParam(ghInst, DVL_MAKEINTRESOURCE(IDD_DIALOG7), ghMainWnd, (DLGPROC)FuncDlg, (LPARAM)file->path);
	}
	assert(write_size == file->buf_size);

	free(file->buf_top);

	file->buf_top = nullptr;
	file->buf = nullptr;

	//std::remove(file->path.c_str());
	//svcOutputDebugString(file->path.c_str(),200);
	//if (std::rename((file->path + ".tmp").c_str(), file->path.c_str()))
	//	throw std::runtime_error("rename");
	free(file);

	return true;
}

}  // namespace dvl
