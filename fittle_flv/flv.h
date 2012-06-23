typedef void *FLVFILE;
FLVFILE FlvOpenFile(LPWSTR filename);
void FlvCloseFile(FLVFILE flvfile);
BOOL FlvHasAudio(FLVFILE flvfile);
BOOL FlvHasVideo(FLVFILE flvfile);
BOOL FlvReadTag(FLVFILE flvfile,void* buffer,DWORD bufsize,LPDWORD read,LPDWORD TagType);
BOOL FlvSeekNextTag(FLVFILE flvfile);
BOOL FlvSeekPrevTag(FLVFILE flvfile);
BOOL FlvSeekHeadTag(FLVFILE flvfile);
BOOL FlvSeekForcePos(FLVFILE flvfile,DWORD pos);
DWORD FlvGetPos(FLVFILE flvfile);
DWORD FlvGetFileSize(FLVFILE flvfile,LPDWORD lpFileSizeHigh);
BOOL FlvGetFiletime(FLVFILE flvfile,FILETIME* ft);
void FlvExtendBufferSize(FLVFILE flvfile);
void FlvShrinkBufferSize(FLVFILE flvfile);

#define BUFSIZE 1024

// 値渡しではなく、ポインタ渡しであることに注意
// 間違えそう
#define UI24TOLONG(pui24) ((*(pui24)<<16)+(*(pui24+1)<<8)+(*(pui24+2)))
#define UI32TOLONG(pui32) ((*(pui32)<<24)+(*(pui32+1)<<16)+(*(pui32+2)<<8)+(*(pui32+3)))

#define FLV_SIGNATURE "FLV"

#define FLV_HEADER_SIZE 9
#define FLV_HEADER_VERSION 1
#define FLV_HEADER_TYPE_FLAG_AUDIO 0x04
#define FLV_HEADER_TYPE_FLAG_VIDEO 0x01

#define FLV_BODY_TAG_TYPE_AUDIO 8
#define FLV_BODY_TAG_TYPE_VIDEO 9
#define FLV_BODY_TAG_TYPE_SCRIPT 18 // ? ただの18かも

#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MASK    0xF0
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_LPCM    0x00
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_ADPCM   0x10
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP3     0x20
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_LPCMLE  0x30
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_NEL16K  0x40
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_NEL8K   0x50
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_NEL     0x60
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_G711A   0x70
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_G711MU  0x80
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_AAC     0xA0
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_MP38K   0xD0
#define FLV_BODY_TAG_AUDIO_SOUND_FORMAT_DEVSPEC 0xE0

#define FLV_BODY_TAG_AUDIO_SOUND_RATE_MASK      0x0C
#define FLV_BODY_TAG_AUDIO_SOUND_RATE_55        0x00
#define FLV_BODY_TAG_AUDIO_SOUND_RATE_11        0x04
#define FLV_BODY_TAG_AUDIO_SOUND_RATE_22        0x08
#define FLV_BODY_TAG_AUDIO_SOUND_RATE_44        0x0C

#define FLV_BODY_TAG_AUDIO_SOUND_SIZE_MASK      0x02
#define FLV_BODY_TAG_AUDIO_SOUND_SIZE_8BIT      0x00
#define FLV_BODY_TAG_AUDIO_SOUND_SIZE_16BIT     0x02

#define FLV_BODY_TAG_AUDIO_SOUND_TYPE_MASK      0x01
#define FLV_BODY_TAG_AUDIO_SOUND_TYPE_MONO      0x00
#define FLV_BODY_TAG_AUDIO_SOUND_TYPE_STEREO    0x01

typedef struct{
	unsigned char Version;
	unsigned char TypeFlags;
	unsigned long DataOffset;
} TFlvHeader;

typedef BYTE* TFlvData;

typedef TFlvData TFlvAudioData;
typedef TFlvData TFlvVideoData;

typedef struct _TFlvTag{
	unsigned char TagType;
	unsigned long DataSize;
	unsigned long Timestamp;
	unsigned char TimestampExtended;
	unsigned long StreamID;
	TFlvData Data;
} TFlvTag;

typedef struct{
	TFlvTag Tag;
	unsigned long TagSize;

} TFlvBody;

typedef struct{
	HANDLE hFile;
	// pos are always offset Head-Byte of PreviousTagLength field.
	DWORD Apos; // absolute pos
	DWORD Bpos; // pos in buffer

	DWORD buflen; // buffer length in used
	DWORD bufsize; // buffer length allocated
	BYTE* buffer;

	TFlvHeader Header;
//	long TagsCount;
//	TFlvTag* BodyTags;
} TFlvFile;