
#include "aplugin.h"
#include "flv.h"
#include <shlwapi.h>

#if defined(_MSC_VER)
#pragma comment(lib,"kernel32.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"shlwapi.lib")
#ifdef UNICODE
#pragma comment(linker, "/EXPORT:GetAPluginInfoW=_GetAPluginInfoW@0")
#define GetAPluginInfo GetAPluginInfoW
#define UNICODE_POSTFIX "W"
#else
#pragma comment(linker, "/EXPORT:GetAPluginInfo=_GetAPluginInfo@0")
#define UNICODE_POSTFIX
#endif
#endif
#if defined(_MSC_VER) && !defined(_DEBUG)
#pragma comment(linker,"/MERGE:.rdata=.text")
#pragma comment(linker,"/ENTRY:DllMain")
#pragma comment(linker,"/OPT:NOWIN98")
#endif

#define EXT_MP3 TEXT(".mp3")
#define EXT_AAC TEXT(".mp4")

static HMODULE hDLL = 0;


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void)lpvReserved;
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		hDLL = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL);
	}
	return TRUE;
}

static BOOL CALLBACK IsArchiveExt(LPTSTR pszExt){
	if(lstrcmpi(pszExt, TEXT("flv"))==0) return TRUE;
	return FALSE;
}

static LPTSTR CALLBACK CheckArchivePath(LPTSTR pszFilePath){
	return StrStrI(pszFilePath, TEXT(".flv/"));
}

static BOOL CALLBACK EnumArchive(LPTSTR pszFilePath,LPFNARCHIVEENUMPROC lpfnProc,void *pData){
	FLVFILE file;
	DWORD tagType;
	DWORD read;
	BYTE* tagBuf;
	BYTE audioTagFlag;
	TCHAR virtualFilename[MAX_PATH+1];
	int cnt = 0;
	// アーカイブをオープン
#ifdef UNICODE
	file = FlvOpenFile(pszFilePath);
	StrCpyW(virtualFilename,pszFilePath);
	if(wcschr(virtualFilename,L'.')){ // 拡張子の直前でカット 後でmp3かaacを付ける
		*(wcschr(virtualFilename,L'.')) = L'\0';
	}
#else
	wchar_t nameW[MAX_PATH + 1];
	MultiByteToWideChar(CP_ACP,0,pszFilePath,-1,nameW,MAX_PATH+1);
	file = FlvOpenFile(nameW);
	strcpy(virtualFilename,pszFilePath);
	if(strchr(virtualFilename,'.')){
		*(strchr(virtualFilename,'.')) = '\0';
	}
#endif
	if(!file){
		return FALSE;
	}
	if(!FlvHasAudio(file)){FlvCloseFile(file);return FALSE;} // NOT include audio track
	

	FILETIME ft = {0,0};
	FlvGetFiletime(file,&ft);
	while(1){ // audio tagが見つかるまで探す
		cnt++;
		if(cnt == 100){Sleep(1);cnt=0;}
		if(FlvReadTag(file,NULL,0,&read,&tagType) == FALSE){break;}
		if(tagType != FLV_BODY_TAG_TYPE_AUDIO){
			if(FlvSeekNextTag(file)==FALSE){break;}
			continue;
		}

		// on found audio tag
		tagBuf = (BYTE*)HeapAlloc(GetProcessHeap(),0,read);
		if(tagBuf == NULL){break;}
		FlvReadTag(file,tagBuf,read,&read,&tagType);

		audioTagFlag = tagBuf[0] & FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MASK;
		HeapFree(GetProcessHeap(),0,tagBuf);

		switch(audioTagFlag){
			case FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC:
				StrCat(virtualFilename,EXT_AAC);
				break;
			case FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3:
				StrCat(virtualFilename,EXT_MP3);
				break;
			default:
				FlvCloseFile(file);
				return FALSE;
				break;
		}
		lpfnProc(virtualFilename,FlvGetFileSize(file,NULL),ft,pData); // success
		FlvCloseFile(file);
		return TRUE;
	}
	// failed.
	FlvCloseFile(file);
	return FALSE;
}

static BOOL CALLBACK ExtractArchive(LPTSTR pszArchivePath, LPTSTR pszFileName, void **ppBuf, DWORD *pSize){
	FLVFILE file;
	DWORD dwBufferSize;
	DWORD dwTagBodySize;
	DWORD dwTagType;
	DWORD dwTagBufSize = BUFSIZE;
	BYTE* TagBuf;
	BYTE* bufPtr;
	int offset = 0;
	// アーカイブをオープン
#ifdef UNICODE
	file = FlvOpenFile(pszArchivePath);
#else
	wchar_t nameW[MAX_PATH + 1];
	MultiByteToWideChar(CP_ACP,0,pszArchivePath,-1,nameW,MAX_PATH+1);
	file = FlvOpenFile(nameW);
#endif
	if(!file){
		return FALSE;
	}
	if(!FlvHasAudio(file)){goto failed;}
	dwBufferSize = FlvGetFileSize(file,NULL);
	*ppBuf = bufPtr = (LPBYTE)HeapAlloc(GetProcessHeap(), /*HEAP_ZERO_MEMORY*/0, dwBufferSize);
	if(bufPtr == NULL){goto failed;}
	*pSize = 0;

	TagBuf = (BYTE*)HeapAlloc(GetProcessHeap(),0,dwTagBufSize); //dwTagBodySize
	while(1){
		if(FlvReadTag(file,NULL,0,&dwTagBodySize,&dwTagType) == FALSE)break; // get tag size and tag type
		if(dwTagBodySize > dwTagBufSize){
			TagBuf = (BYTE*)HeapReAlloc(GetProcessHeap(),0,TagBuf,dwTagBodySize);
			dwTagBufSize = dwTagBodySize;
		}
		if(TagBuf == NULL){goto failed;}
		if(dwTagType != FLV_BODY_TAG_TYPE_AUDIO){ // not audio tag
			if(FlvSeekNextTag(file) == FALSE){
				break;
			}
			continue;
		}
		if(FlvReadTag(file,TagBuf,dwTagBodySize,&dwTagBodySize,&dwTagType) == FALSE){// read tag
			break;
		}
		if(!offset){
			if(TagBuf[0] & FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC)offset=2;
			offset=1;
		}
		//memcpy(bufPtr,TagBuf+offset,dwTagBodySize-offset);
		char mes[10];
		wsprintfA(mes,"%d",dwTagBodySize-offset);
//		MessageBoxA(NULL,mes,"",MB_OK);
		if(dwTagBodySize > offset){
			DWORD copyLength = dwTagBodySize - offset;
			memcpy(bufPtr,TagBuf+offset,copyLength);
			*pSize = *pSize + copyLength;
			bufPtr += copyLength;
		}
	}
	HeapFree(GetProcessHeap(),0,TagBuf);
	FlvCloseFile(file);
	return TRUE;

failed:
	FlvCloseFile(file);
	return FALSE;
}


static ARCHIVE_PLUGIN_INFO apinfo = {
	0,
	APDK_VER,
	IsArchiveExt,
	CheckArchivePath,
	EnumArchive,
	ExtractArchive
};

#ifdef __cplusplus
extern "C"
#endif
ARCHIVE_PLUGIN_INFO * CALLBACK GetAPluginInfo(void)
{
	return &apinfo;
}
