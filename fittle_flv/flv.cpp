
#include <windows.h>

#include "flv.h"

FLVFILE FlvOpenFile(LPWSTR filename){
	HANDLE hFile;
	BYTE FlvHeaderBuf[FLV_HEADER_SIZE];
	DWORD readed;
	DWORD dwFileSize;
	HANDLE hMapFile;
	BYTE *lpMapViewOfFile;

	hFile = CreateFileW(filename,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,0);
	if(hFile == INVALID_HANDLE_VALUE){return NULL;}
	if(ReadFile(hFile,FlvHeaderBuf,FLV_HEADER_SIZE,&readed,NULL) == FALSE){
		CloseHandle(hFile);
		return NULL;
	}
	if(readed != FLV_HEADER_SIZE){
		CloseHandle(hFile);
		return NULL;
	}
	if(memcmp(FlvHeaderBuf,FLV_SIGNATURE,3) != 0){
		CloseHandle(hFile);
		return NULL;
	}
	if(FlvHeaderBuf[3] != FLV_HEADER_VERSION){
		CloseHandle(hFile);
		return NULL;
	}// End of Validation.

	dwFileSize = GetFileSize(hFile, NULL);
	if(dwFileSize<=FLV_HEADER_SIZE+sizeof(DWORD)){
		//ファイルサイズが13以下のときFlvSeekHeadTagが失敗する
		CloseHandle(hFile);
		return NULL;
	}
	hMapFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if(hMapFile==NULL){
		CloseHandle(hFile);
		return NULL;
	}
	lpMapViewOfFile = (BYTE*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
	if(lpMapViewOfFile==NULL){
		CloseHandle(hMapFile);
		CloseHandle(hFile);
		return NULL;
	}

	TFlvFile* flvfile = (TFlvFile*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(TFlvFile));
	if(flvfile == NULL){
		CloseHandle(hFile);
		return NULL;
	}

	flvfile->hFile = hFile;
	flvfile->dwFileSize = dwFileSize;
	flvfile->hMapFile = hMapFile;
	flvfile->lpMapViewOfFile = lpMapViewOfFile;
	flvfile->Header.Version = FlvHeaderBuf[3];
	flvfile->Header.TypeFlags = FlvHeaderBuf[4];
	flvfile->Header.DataOffset = UI32TOLONG(FlvHeaderBuf+5);

	FlvSeekHeadTag(flvfile);

	return (FLVFILE)flvfile;
}

void FlvCloseFile(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	UnmapViewOfFile(file->lpMapViewOfFile);
	CloseHandle(file->hMapFile);
	CloseHandle(file->hFile);
	file->hFile = NULL;
	HeapFree(GetProcessHeap(),0,file);
}

DWORD FlvGetFileSize(FLVFILE flvfile,LPDWORD lpFileSizeHigh){
	TFlvFile* file = (TFlvFile*)flvfile;
	return GetFileSize(file->hFile,lpFileSizeHigh);
}

BOOL FlvGetFiletime(FLVFILE flvfile,FILETIME* ft){
	TFlvFile* file = (TFlvFile*)flvfile;
	return GetFileTime(file->hFile,NULL,NULL,ft);
}

BOOL FlvHasAudio(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	return (file->Header.TypeFlags & FLV_HEADER_TYPE_FLAG_AUDIO);
}

BOOL FlvHasVideo(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	return (file->Header.TypeFlags & FLV_HEADER_TYPE_FLAG_VIDEO);
}

TFlvTag* _ReadTag(TFlvFile* flvfile)
{
	if(flvfile->pos+sizeof(TFlvTag) > flvfile->dwFileSize)
		return NULL;
	return (TFlvTag*)(flvfile->lpMapViewOfFile+flvfile->pos);
}

// if buffer == NULL, only get TagSize(in `read') and TagType
//FlvSeekNextTagが失敗したときFALSEを返すので、最後のTagのDataが取得できない
BOOL FlvReadTag(FLVFILE flvfile,void* buffer,DWORD bufsize,LPDWORD read,LPDWORD TagType){
	TFlvFile* file = (TFlvFile*)flvfile;
	TFlvTag *Tag;
	Tag = _ReadTag(file);
	if(Tag==NULL)
		return FALSE;
	DWORD DataSize = Tag->DataSize[0]<<16|Tag->DataSize[1]<<8|Tag->DataSize[2];
	*read = DataSize;
	*TagType = Tag->TagType;
	if(buffer){
		memcpy(buffer, file->lpMapViewOfFile+file->pos+sizeof(TFlvTag), bufsize);
		if(!FlvSeekNextTag(file))
			return FALSE;
	}
	return TRUE;
}

TFlvTag* FlvGetFlvTag(FLVFILE flvfile)
{
	TFlvFile* file = (TFlvFile*)flvfile;
	return _ReadTag(file);
}

BOOL FlvGetTagDataSize(FLVFILE flvfile, LPDWORD DataSize){
	TFlvFile* file = (TFlvFile*)flvfile;
	TFlvTag *Tag;
	Tag = _ReadTag(file);
	if(Tag==NULL)
		return FALSE;
	*DataSize = Tag->DataSize[0]<<16|Tag->DataSize[1]<<8|Tag->DataSize[2];
	return TRUE;
}

BYTE* FlvGetTagData(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	return file->lpMapViewOfFile+file->pos+sizeof(TFlvTag);
}

BOOL FlvSeekNextTag(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	TFlvTag *Tag;
	Tag = _ReadTag(file);
	if(Tag==NULL)
		return FALSE;
	DWORD DataSize = Tag->DataSize[0]<<16|Tag->DataSize[1]<<8|Tag->DataSize[2];
	if(file->pos+sizeof(TFlvTag)+DataSize+sizeof(DWORD) > file->dwFileSize)
		return FALSE;
	file->pos += sizeof(TFlvTag)+DataSize+sizeof(DWORD);
	return TRUE;
}

BOOL FlvSeekForcePos(FLVFILE flvfile,DWORD pos){
	TFlvFile* file = (TFlvFile*)flvfile;
	file->pos = pos;
	return true;
}

BOOL FlvSeekPrevTag(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	file->pos -= *(LPDWORD)(file->lpMapViewOfFile+file->pos-sizeof(DWORD));
	return TRUE;
}

BOOL FlvSeekHeadTag(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	if(file->dwFileSize <= FLV_HEADER_SIZE+sizeof(DWORD))
		return FALSE;
	file->pos = FLV_HEADER_SIZE+sizeof(DWORD);	//9+4
	return TRUE;
}

DWORD FlvGetPos(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	return file->pos;
}
