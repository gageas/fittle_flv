#include <windows.h>
#include "flv.h"

FLVFILE FlvOpenFile(LPWSTR filename){
	HANDLE hFile;
	BYTE FlvHeaderBuf[FLV_HEADER_SIZE];
	DWORD readed;

	hFile = CreateFileW(filename,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,0);
	if(hFile == INVALID_HANDLE_VALUE){return NULL;}
	if(ReadFile(hFile,FlvHeaderBuf,FLV_HEADER_SIZE,&readed,NULL) == FALSE){
		return NULL;
	}
	if(memcmp(FlvHeaderBuf,FLV_SIGNATURE,3) != 0){return NULL;}
	if(FlvHeaderBuf[3] != FLV_HEADER_VERSION){return NULL;}
	// End of Validation.

	TFlvFile* flvfile = (TFlvFile*)HeapAlloc(GetProcessHeap(),0,sizeof(TFlvFile));
	if(flvfile == NULL){
		return NULL;
	}

	flvfile->buffer = (BYTE*)HeapAlloc(GetProcessHeap(),0,BUFSIZE);
	if(flvfile->buffer == NULL){
		HeapFree(GetProcessHeap(),0,flvfile);
		return NULL;
	}

	flvfile->Header.Version = FlvHeaderBuf[3];
	flvfile->Header.TypeFlags = FlvHeaderBuf[4];
	flvfile->Header.DataOffset = UI32TOLONG(FlvHeaderBuf+5);
	flvfile->Apos = flvfile->Header.DataOffset;
	flvfile->Bpos = 0;
	flvfile->hFile = hFile;
	flvfile->bufsize = BUFSIZE;
	return (FLVFILE)flvfile;
}
void FlvCloseFile(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	CloseHandle(file->hFile);
	file->hFile = NULL;
	file->buffer = NULL;
	HeapFree(GetProcessHeap(),0,file->buffer);
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
	TFlvFile* tflvfile;
	tflvfile = (TFlvFile*)flvfile;
	return (tflvfile->Header.TypeFlags & FLV_HEADER_TYPE_FLAG_AUDIO);
}
BOOL FlvHasVideo(FLVFILE flvfile){
	TFlvFile* tflvfile;
	tflvfile = (TFlvFile*)flvfile;
	return (tflvfile->Header.TypeFlags & FLV_HEADER_TYPE_FLAG_VIDEO);
}

void UpdateBuffer(TFlvFile* file,DWORD Apos){
	file->Apos = SetFilePointer(file->hFile,Apos,0,FILE_BEGIN);
	DWORD read=0;
	if(ReadFile(file->hFile,file->buffer,file->bufsize,&read,NULL) == FALSE){
		file->buflen = 0;
	}else{
		file->buflen = read;
	}
	file->Bpos = 0;
}

// if buffer == NULL, only get TagSize
BOOL FlvReadTag(FLVFILE flvfile,void* buffer,DWORD bufsize,LPDWORD read,LPDWORD TagType){
	TFlvFile* file = (TFlvFile*)flvfile;
	unsigned long previousTagSize;
	unsigned long currentDataSize;
	if(file->buflen <= (file->Bpos+4+11)){ // PreviousTagSizeが4バイト、TagHeaderが11バイト。それより大きいバッファがなければうｐだて
		UpdateBuffer(file,file->Apos);
		if(file->buflen <= (file->Bpos+4+11)){read=0;
		return FALSE;} // failed.またはファイル終端
	}
	previousTagSize = UI32TOLONG(file->buffer+file->Bpos);
	currentDataSize = UI24TOLONG(file->buffer+file->Bpos+5);

	if(currentDataSize + 4 + 11 > file->bufsize){
		DWORD newBufSize = currentDataSize+4+11+BUFSIZE;
		file->buffer = (BYTE*)HeapReAlloc(GetProcessHeap(),0,file->buffer,newBufSize);
		file->bufsize = newBufSize;
		if(file->buffer == NULL){
			return FALSE;
		}
		UpdateBuffer(file,file->Apos);
	}

	if(TagType){*TagType = *(file->buffer+file->Bpos+4);}
	if(read){*read = currentDataSize;}

	if(buffer == NULL){ // only get size
		return TRUE;
	}

	if(bufsize < currentDataSize){if(read){*read = 0;}return FALSE;}
	if(file->buflen <= (file->Bpos+4+11+currentDataSize)){ // no enought prebuf 
		UpdateBuffer(file,file->Apos);
		if(file->buflen <= (file->Bpos+4+11+currentDataSize)){read=0;
		return FALSE;} // cannot prebuf
	}
	memcpy(buffer,file->buffer+file->Bpos+4+11,currentDataSize);
	file->Bpos+=4+11+currentDataSize;
	file->Apos+=4+11+currentDataSize;
	return TRUE;
}
BOOL FlvSeekNextTag(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	unsigned long currentDataSize;
	if(file->buflen <= (file->Bpos+4+11)){ // PreviousTagSizeが4バイト、TagHeaderが11バイト。それより大きいバッファがなければうｐだて
		UpdateBuffer(file,file->Apos);
		if(file->buflen <= (file->Bpos+4+11)){return FALSE;} // failed.またはファイル終端
	}
	currentDataSize = UI24TOLONG(file->buffer+file->Bpos+5);

	if(file->buflen <= (file->Bpos+4+11+currentDataSize)){ // no enought prebuf 
		UpdateBuffer(file,file->Apos);
		if(file->buflen <= (file->Bpos+4+11+currentDataSize)){return FALSE;} // cannot prebuf
	}
	file->Bpos+=4+11+currentDataSize;
	file->Apos+=4+11+currentDataSize;

	return TRUE;
}

BOOL FlvSeekPrevTag(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	unsigned long previousTagSize;
	previousTagSize = UI32TOLONG(file->buffer+file->Bpos);
	if(previousTagSize == 0)return FALSE; // already on head of file
	if(file->Bpos >= previousTagSize){
		file->Bpos -= previousTagSize;
		file->Apos -= previousTagSize;
	}else{
		file->Apos -= previousTagSize;
		UpdateBuffer(file,file->Apos);
	}
	return TRUE;
}
BOOL FlvSeekHeadTag(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	UpdateBuffer(file,FLV_HEADER_SIZE);
	return TRUE;
}
DWORD FlvGetPos(FLVFILE flvfile){
	TFlvFile* file = (TFlvFile*)flvfile;
	return file->Apos+file->Bpos;
}