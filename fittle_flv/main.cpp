
#include "aplugin.h"
#include "flv.h"
#include <shlwapi.h>

#if defined(_MSC_VER)
#ifdef UNICODE
#pragma comment(linker, "/EXPORT:GetAPluginInfoW=_GetAPluginInfoW@0")
#else
#pragma comment(linker, "/EXPORT:GetAPluginInfo=_GetAPluginInfo@0")
#endif
#endif
#ifdef UNICODE
#define GetAPluginInfo GetAPluginInfoW
#else
#endif

#define EXT_MP3 TEXT(".mp3")
#define EXT_AAC TEXT(".aac")

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
	lstrcpy(virtualFilename,pszFilePath);
	if(PathFindExtension(virtualFilename)){ // 拡張子の直前でカット 後でmp3かaacを付ける
		*(PathFindExtension(virtualFilename)) = '\0';
	}
#else
	wchar_t nameW[MAX_PATH + 1];
	MultiByteToWideChar(CP_ACP,0,pszFilePath,-1,nameW,MAX_PATH+1);
	file = FlvOpenFile(nameW);
	lstrcpy(virtualFilename,pszFilePath);
	if(PathFindExtension(virtualFilename)){
		*(PathFindExtension(virtualFilename)) = '\0';
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
		FlvCloseFile(file);

		switch(audioTagFlag){
			case FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC:
				StrCat(virtualFilename,EXT_AAC);
				break;
			case FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3:
				StrCat(virtualFilename,EXT_MP3);
				break;
			default:
				return FALSE; // failed.
				break;
		}
		lpfnProc(virtualFilename,FlvGetFileSize(file,NULL),ft,pData); // success
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
	BYTE* TagBuf;
	BYTE* bufPtr;
	DWORD SoundFormat = 0;
	DWORD SoundRate = 0;
	DWORD SoundType = 0;
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
	if(!FlvHasAudio(file)){
		FlvCloseFile(file);
		return FALSE;
	}
	dwBufferSize = FlvGetFileSize(file,NULL);
	*ppBuf = bufPtr = (LPBYTE)HeapAlloc(GetProcessHeap(), /*HEAP_ZERO_MEMORY*/0, dwBufferSize);
	if(bufPtr == NULL){
		FlvCloseFile(file);
		return FALSE;
	}
	*pSize = 0;

	while(1){
		//タグサイズとタグタイプを取得
		dwTagBodySize=0;
		dwTagType=0;
		if(FlvReadTag(file,NULL,0,&dwTagBodySize,&dwTagType) == FALSE) // get tag size and tag type
			break;
		if(dwTagType != FLV_BODY_TAG_TYPE_AUDIO){ // not audio tag
			if(FlvSeekNextTag(file) == FALSE)
				break;
			continue;
		}
		TagBuf = (BYTE*)HeapAlloc(GetProcessHeap(),0,dwTagBodySize);
		if(TagBuf == NULL){
			HeapFree(GetProcessHeap(),0,*ppBuf);
			*ppBuf = NULL;
			*pSize = 0;
			FlvCloseFile(file);
			return FALSE;
		}
		//タグを取得
		if(FlvReadTag(file,TagBuf,dwTagBodySize,&dwTagBodySize,&dwTagType) == FALSE){ // read tag
			HeapFree(GetProcessHeap(),0,TagBuf);
			TagBuf = NULL;
			break;
		}
		//SoundFormat、SoundRate、SoundTypeを取得
		if(SoundFormat==0){
			SoundFormat = TagBuf[0] & FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MASK;
			SoundRate   = TagBuf[0] & FLV_BODY_TAG_AUDIO_SOUND_RATE_MASK;
			SoundType   = TagBuf[0] & FLV_BODY_TAG_AUDIO_SOUND_TYPE_MASK;
/*
			if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3\n"));
			else if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC\n"));
			else
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_FORMAT_???\n"));

			if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_55)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_RATE_55\n"));
			else if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_11)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_RATE_11\n"));
			else if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_22)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_RATE_22\n"));
			else if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_44)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_RATE_44\n"));

			if(SoundType==FLV_BODY_TAG_AUDIO_SOUND_TYPE_MONO)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_TYPE_MONO\n"));
			else if(SoundType==FLV_BODY_TAG_AUDIO_SOUND_TYPE_STEREO)
				OutputDebugStr(TEXT("FLV_BODY_TAG_AUDIO_SOUND_TYPE_STEREO\n"));
*/
		}

		if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3 && dwTagBodySize>0){
			int offset=1;
			memcpy(bufPtr,TagBuf+offset,dwTagBodySize-offset);
			*pSize = *pSize + dwTagBodySize -offset;
			bufPtr += dwTagBodySize -offset;
		}
		else if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC && dwTagBodySize!=4){
			BYTE adts[7] = {0};
			int id;
			int layer;
			int protection;
			int profile;
			int samplingrateindex = 0;
			int privatebit = 0;
			int channels = 0;
			int length;
			int offset=2;

			//ADTS作成
			//参考 "ADTS aac","WriteADTSHeader"でググる
			id = 0;	//0:mpeg4 1:mpeg2
			layer = 0;	//allways 0
			protection = 1;
			profile = 0x01;	//0x00:MAIN 0x01:LC 0x10:SSR 0x11:(reserved)
			if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_55) samplingrateindex = 11;	//800   hz
			if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_11) samplingrateindex = 10;	//11025 hz
			if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_22) samplingrateindex = 7;	//22050 hz
			if(SoundRate==FLV_BODY_TAG_AUDIO_SOUND_RATE_44) samplingrateindex = 4;	//44100 hz
			if(SoundType==FLV_BODY_TAG_AUDIO_SOUND_TYPE_MONO) channels = 1;
			if(SoundType==FLV_BODY_TAG_AUDIO_SOUND_TYPE_STEREO) channels = 2;
			length = 7+dwTagBodySize-offset;

			adts[0] = 0xff;
			adts[1] = 0xf0 | ((id&0x01)<<3) | ((layer&0x03)<<1) | (protection&0x01);
			adts[2] = ((profile&0x03)<<6) | ((samplingrateindex&0x0F)<<2) | ((privatebit&0x01)<<1) | ((channels&0x07)>>2);
			adts[3] = ((channels&0x07)<<6) | (length>>11)&0xFF;
			adts[4] = (length>>3)&0xFF;
			adts[5] = (length<<5)&0xFF | 0x1f;
			adts[6] = 0xfc;

			//ADTS
			memcpy(bufPtr,adts,7);
			*pSize = *pSize + 7;
			bufPtr += 7;

			//タグ
			memcpy(bufPtr,TagBuf+offset,dwTagBodySize-offset);
			*pSize = *pSize + dwTagBodySize -offset;
			bufPtr += dwTagBodySize -offset;
		}
		HeapFree(GetProcessHeap(),0,TagBuf);
	}
	FlvCloseFile(file);
	return TRUE;
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
