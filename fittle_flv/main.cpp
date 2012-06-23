
#include <windows.h>
#include <shlwapi.h>

#include "aplugin.h"
#include "flv.h"

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

typedef struct tagFLVFILEINFO{
	DWORD dwAudioLength;	//audio合計サイズ
	DWORD dwVideoLength;	//video合計サイズ
}FLVFILEINFO,*LPFLVFILEINFO;
void flvGetInfo(FLVFILE file, FLVFILEINFO *lpInfo)
{
	FlvSeekHeadTag(file);
	do{
		TFlvTag *FlvTag;
		FlvTag = FlvGetFlvTag(file);
		if(FlvTag==NULL)
			break;
		switch(FlvTag->TagType){
			case FLV_BODY_TAG_TYPE_AUDIO:
			{
				BYTE *data;
				BYTE AudioTagHeader;
				BYTE SoundFormat;
				DWORD DataSize = 0;
				data = FlvGetTagData(file);
				AudioTagHeader = data[0];
				SoundFormat = AudioTagHeader & FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MASK;
				if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3){
					if(FlvGetTagDataSize(file, &DataSize))
						DataSize = DataSize-1;	//1=AudioTagHeader
				}
				else if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC){
					BYTE AACPacketType;
					AACPacketType = data[1];
					if(AACPacketType==0){	//AACPacketType
						//AAC sequence header
						DataSize = 0;
					}
					else{
						//AAC raw
						if(FlvGetTagDataSize(file, &DataSize))
							DataSize = 7+DataSize-1-1; //7=sizeof(adts) 1=AudioTagHeader 1=AACPacketType
					}
				}
				else{
					//?
				}
				lpInfo->dwAudioLength+=DataSize;
				break;
			}
/*必要ないからコメントアウトしておく
			case FLV_BODY_TAG_TYPE_VIDEO:
			{
				BYTE *data;
				BYTE VideoTagHeader;
				DWORD DataSize = 0;
				data = FlvGetTagData(file);
				VideoTagHeader = data[0];
				FlvGetTagDataSize(file, &DataSize);
				lpInfo->dwVideoLength+=DataSize;
				break;
			}
			case FLV_BODY_TAG_TYPE_SCRIPT:
				break;
			default:
				break;
*/
		}
	}while(FlvSeekNextTag(file));
}

static BOOL CALLBACK EnumArchive(LPTSTR pszFilePath,LPFNARCHIVEENUMPROC lpfnProc,void *pData){
	FLVFILE file;
	// アーカイブをオープン
#ifdef UNICODE
	file = FlvOpenFile(pszFilePath);
#else
	wchar_t nameW[MAX_PATH + 1];
	MultiByteToWideChar(CP_ACP, 0, pszFilePath, -1, nameW, MAX_PATH+1);
	file = FlvOpenFile(nameW);
#endif
	if(!file){
		return FALSE;
	}
	if(!FlvHasAudio(file)){
		FlvCloseFile(file);
		return FALSE;
	}
	//最初のAudioTagHeaderを取得する
	BYTE AudioTagHeader = 0;
	FlvSeekHeadTag(file);
	do{
		TFlvTag *FlvTag;
		FlvTag = FlvGetFlvTag(file);
		if(FlvTag==NULL)
			break;
		if(FlvTag->TagType==FLV_BODY_TAG_TYPE_AUDIO){
			BYTE *data;
			data = FlvGetTagData(file);
			AudioTagHeader = data[0];
			break;
		}
	}while(FlvSeekNextTag(file));
	if(AudioTagHeader==0){
		FlvCloseFile(file);
		return FALSE;
	}
	//サウンドフォーマット
	DWORD SoundFormat = 0;
	SoundFormat = AudioTagHeader & FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MASK;
	//更新日時
	FILETIME ft = {0,0};
	FlvGetFiletime(file, &ft);

	FlvCloseFile(file);

	TCHAR virtualFilename[MAX_PATH+1];
	lstrcpy(virtualFilename, PathFindFileName(pszFilePath));
	if(PathFindExtension(virtualFilename))
		*(PathFindExtension(virtualFilename)) = '\0';

	switch(SoundFormat){
		case FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC:
			lstrcat(virtualFilename, EXT_AAC);
			break;
		case FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3:
			lstrcat(virtualFilename, EXT_MP3);
			break;
		default:
			return FALSE;
	}
	lpfnProc(virtualFilename, FlvGetFileSize(file, NULL), ft, pData);
	return TRUE;
}

static BOOL CALLBACK ExtractArchive(LPTSTR pszArchivePath, LPTSTR pszFileName, void **ppBuf, DWORD *pSize){
	FLVFILE file;
	// アーカイブをオープン
#ifdef UNICODE
	file = FlvOpenFile(pszArchivePath);
#else
	wchar_t nameW[MAX_PATH + 1];
	MultiByteToWideChar(CP_ACP, 0, pszArchivePath, -1, nameW, MAX_PATH+1);
	file = FlvOpenFile(nameW);
#endif
	if(!file){
		return FALSE;
	}
	if(!FlvHasAudio(file)){
		FlvCloseFile(file);
		return FALSE;
	}
	//最初のAudioTagHeaderを取得する
	BYTE AudioTagHeader = 0;
	FlvSeekHeadTag(file);
	do{
		TFlvTag *FlvTag;
		FlvTag = FlvGetFlvTag(file);
		if(FlvTag==NULL)
			break;
		if(FlvTag->TagType==FLV_BODY_TAG_TYPE_AUDIO){
			BYTE *data;
			data = FlvGetTagData(file);
			AudioTagHeader = data[0];
			break;
		}
	}while(FlvSeekNextTag(file));
	if(AudioTagHeader==0){
		FlvCloseFile(file);
		return FALSE;
	}
	//サウンドフォーマット
	DWORD SoundFormat = 0;
	SoundFormat = AudioTagHeader & FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MASK;
	if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3 ||
		SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC){
		//ok
	}
	else{
		FlvCloseFile(file);
		return FALSE;
	}

	FLVFILEINFO info = {0};
	flvGetInfo(file, &info);

	BYTE* bufPtr;
	*ppBuf = bufPtr = (LPBYTE)HeapAlloc(GetProcessHeap(), /*HEAP_ZERO_MEMORY*/0, info.dwAudioLength);
	if(bufPtr == NULL){
		FlvCloseFile(file);
		return FALSE;
	}
	*pSize = 0;

	int profile = 0;
	int samplingrateindex = 0;
	int channels = 0;

	FlvSeekHeadTag(file);
	do{
		TFlvTag *FlvTag;
		FlvTag = FlvGetFlvTag(file);
		if(FlvTag==NULL)
			break;
		if(FlvTag->TagType==FLV_BODY_TAG_TYPE_AUDIO){
			BYTE *data;
			BYTE AudioTagHeader;
			DWORD SoundFormat = 0;
			DWORD DataSize;
			data = FlvGetTagData(file);
			AudioTagHeader = data[0];
			SoundFormat = AudioTagHeader & FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MASK;
			FlvGetTagDataSize(file, &DataSize);
			if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3){
				int offset = 1;	//1=AudioTagHeader
				memcpy(bufPtr, data+offset, DataSize-offset);
				*pSize = *pSize + DataSize -offset;
				bufPtr += DataSize -offset;
			}
			else if(SoundFormat==FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC){
				BYTE AACPacketType;
				AACPacketType = data[1];
				if(AACPacketType==0){
					//AAC sequence header
					WORD flag;
					flag = data[2]<<8|data[3];
					profile = ((flag&0xF800)>>11)-1;	//0x00:MAIN 0x01:LC 0x10:SSR 0x11:(reserved)
					samplingrateindex = ((flag&0x0780)>>7);
					channels = ((flag&0x0078)>>3);
				}
				else{
					//AAC raw
					BYTE adts[7] = {0};
					int id;
					int layer;
					int protection;
					int privatebit = 0;
					int length;
					int offset = 2;	//2=AudioTagHeader+AACPacketType

					//ADTS作成
					//参考 "ADTS aac","WriteADTSHeader"でググる
					id = 0;	//0:mpeg4 1:mpeg2
					layer = 0;	//allways 0
					protection = 1;
					length = 7+DataSize-offset;

					adts[0] = 0xff;
					adts[1] = 0xf0 | ((id&0x01)<<3) | ((layer&0x03)<<1) | (protection&0x01);
					adts[2] = ((profile&0x03)<<6) | ((samplingrateindex&0x0F)<<2) | ((privatebit&0x01)<<1) | ((channels&0x07)>>2);
					adts[3] = ((channels&0x07)<<6) | (length>>11)&0xFF;
					adts[4] = (length>>3)&0xFF;
					adts[5] = (length<<5)&0xFF | 0x1f;
					adts[6] = 0xfc;

					//ADTS
					memcpy(bufPtr, adts, sizeof(adts));
					*pSize = *pSize + sizeof(adts);
					bufPtr += sizeof(adts);

					//タグ
					memcpy(bufPtr, data+offset, DataSize-offset);
					*pSize = *pSize + DataSize -offset;
					bufPtr += DataSize -offset;
				}
			}
		}
	}while(FlvSeekNextTag(file));
	FlvCloseFile(file);
	if(0){
		//抽出した音声データをファイルに保存してみる
		HANDLE hFile;
		DWORD cbWrite = 0;
		hFile = CreateFile(TEXT("plugins\\fap\\output.mp3"), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(hFile, *ppBuf, *pSize, &cbWrite, NULL);
		CloseHandle(hFile);
	}
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
#ifdef __MINGW32__
__declspec(dllexport)
#endif
ARCHIVE_PLUGIN_INFO * CALLBACK GetAPluginInfo(void)
{
	return &apinfo;
}
