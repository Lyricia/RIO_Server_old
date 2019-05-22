#pragma once

#include "targetver.h"

#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <tchar.h>
#include <chrono>
#include <vector>
#include <thread>
#include <list>
#include <mutex>
#include <map>
#include <WinSock2.h>
#include <MSWSock.h>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <locale>
#include <iostream>
#include <sqlext.h> 
#include <atomic>

#include "Vector3D.h"
#include "protocol.h"

extern "C" {
#include "Lua\lua.h"
#include "Lua\lauxlib.h"
#include "Lua\lualib.h"
}

#pragma comment(lib, "Lua/lua53.lib")
#pragma comment(lib, "ws2_32")

#define EPSILON			0.00001f

using std::thread;
using std::list;
using std::vector;
using std::cout;
using std::endl;

enum GAMESTATUS {
	STOP
	, RUNNING
	, PAUSE
	, BLACKWIN
	, WHITEWIN	
};

enum RioConfig
{
	SESSION_BUFFER_SIZE = 65532,
	BUFFER_OFFSET = 32766,
	MAX_RIO_THREAD = 8,
	MAX_RIO_RESULT = 1024,
	MAX_SEND_RQ_SIZE_PER_SOCKET = 1024,
	MAX_RECV_RQ_SIZE_PER_SOCKET = 1024,
	MAX_CLIENT_PER_RIO_THREAD = 2560,
	MAX_CQ_SIZE_PER_RIO_THREAD = (MAX_SEND_RQ_SIZE_PER_SOCKET + MAX_RECV_RQ_SIZE_PER_SOCKET) * MAX_CLIENT_PER_RIO_THREAD,

};

inline Vector3D<float> Vec3i_to_Vec3f(Vector3D<int>& ivec)
{
	return Vector3D<float>{ static_cast<float>(ivec.x), static_cast<float>(ivec.y) ,static_cast<float>(ivec.z) };
}

inline Vector3D<int> Vec3f_to_Vec3i(Vector3D<float>& fvec)
{
	return Vector3D<int>{ static_cast<int>(fvec.x), static_cast<int>(fvec.y), static_cast<int>(fvec.z) };
}