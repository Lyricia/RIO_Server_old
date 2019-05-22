//#include "stdafx.h"
#define SERVERPORT		4000

#define MAX_BUFF_SIZE	4096
#define MAX_PACKET_SIZE 255

#define BOARD_WIDTH		100
#define BOARD_HEIGHT	100
#define SPACESIZE		10

#define VIEW_RADIUS		7

#define	MOVE_TIME		1000

#define MAX_USER		10000
#define MAX_ROOM_USER	10

#define NPC_START		100000
#define NUM_OF_NPC		100000

#define MAX_STR_SIZE	100
#define MAX_NAME_LEN	50

#define CS_LOGIN		1
#define CS_LOGOUT		2

#define CS_UP			3
#define CS_DOWN			4
#define CS_LEFT			5
#define CS_RIGHT		6

#define CS_CHAT			7

#define SC_LOGINOK			1
#define SC_LOGINFAIL		2
#define SC_POS				3
#define SC_CHAT				4
#define SC_STAT_CHANGE		5
#define SC_REMOVE_PLAYER	6
#define SC_PUT_PLAYER		7

constexpr int SPACE_X = BOARD_WIDTH / SPACESIZE;
constexpr int SPACE_Y = BOARD_HEIGHT / SPACESIZE;
constexpr int MAX_ROOM_COUNT = MAX_USER / MAX_ROOM_USER;

#pragma pack (push, 1)

struct cs_packet_login {
	BYTE size;
	BYTE type;
	WORD id;
};

struct cs_packet_move {
	BYTE size;
	BYTE type;
};

struct cs_packet_chat {
	BYTE size;
	BYTE type;
	WCHAR message[MAX_STR_SIZE];
};

struct sc_packet_loginok {
	BYTE size;
	BYTE type;
	WORD id;
	WORD x;
	WORD y;
	WORD hp;
	BYTE level;
	DWORD exp;
};

struct sc_packet_loginfail {
	BYTE size;
	BYTE type;
};

struct sc_packet_pos {
	BYTE size;
	BYTE type;
	WORD id;
	SHORT x;
	SHORT y;
};

struct sc_packet_put_player {
	BYTE size;
	BYTE type;
	WORD id;
	SHORT x;
	SHORT y;
};

struct sc_packet_remove_player {
	BYTE size;
	BYTE type;
	WORD id;
};

struct sc_packet_chat {
	BYTE size;
	BYTE type;
	WORD id;
	WCHAR message[MAX_STR_SIZE];
};
#pragma pack (pop)