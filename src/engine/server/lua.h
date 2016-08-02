#ifndef ENGINE_SERVER_LUA_H
#define ENGINE_SERVER_LUA_H

#include <lua.hpp>
#include <base/tl/array.h>
#include <engine/external/luabridge/LuaBridge.h>
#include <engine/external/luabridge/RefCountedPtr.h>

#define LUA_FIRE_EVENT(EVENTNAME, ...) \
	{ \
		if(g_Config.m_ClLua) \
			for(int ijdfg = 0; ijdfg < Server()->Lua()->GetLuaFiles().size(); ijdfg++) \
			{ \
				if(Server()->Lua()->GetLuaFiles()[ijdfg]->State() != CLuaFile::LUAFILE_STATE_LOADED) \
					continue; \
				LuaRef lfunc = Server()->Lua()->GetLuaFiles()[ijdfg]->GetFunc(EVENTNAME); \
				if(lfunc) try { lfunc(__VA_ARGS__); } catch(std::exception &e) { Server()->Lua()->HandleException(e, Server()->Lua()->GetLuaFiles()[ijdfg]); } \
			} \
			LuaRef confunc = getGlobal(CGameConsole::m_pStatLuaConsole->m_LuaHandler.m_pLuaState, EVENTNAME); \
			if(confunc) try { confunc(__VA_ARGS__); } catch(std::exception &e) { printf("LUA EXCEPTION: %s\n", e.what()); } \
	}

class IServer;
class CServer;
class IStorage;
class IGameServer;
class CGameServer;
class CLuaFile;

using namespace luabridge;

class CLua
{
	array<CLuaFile*> m_pLuaFiles;
	array<std::string> m_aAutoloadFiles;
	IStorage *m_pStorage;
	class IConsole *m_pConsole;

	struct LuaErrorCounter
	{
		CLuaFile* culprit;
		int count;
	};
	array<LuaErrorCounter> m_ErrorCounter;

public:
	CLua();
	~CLua();
	
	void Init(IServer *pServer, IStorage *pStorage, IConsole *pConsole);
	void Shutdown();
	void SaveAutoloads();
	void AddUserscript(const char *pFilename);
	void LoadFolder();
	void LoadFolder(const char *pFolder);
	void SortLuaFiles();


	static int ErrorFunc(lua_State *L);
	static int Panic(lua_State *L);
	int HandleException(std::exception &e, CLuaFile*);

	static CServer * m_pCServer;
	static IServer *m_pServer;
	static IGameServer *m_pGameServer;
	static IServer *Server() { return m_pServer; }
	static IGameServer *GameServer() { return m_pGameServer; }
	static CGameServer * m_pCGameServer;
	
	void SetGameServer(IGameServer *pGameServer);
	array<CLuaFile*> &GetLuaFiles() { return m_pLuaFiles; }

	IStorage *Storage() const { return m_pStorage; }

	struct LuaLoadHelper
	{
	public:
		CLua * pLua;
		const char * pString;
	};

private:
	static int LoadFolderCallback(const char *pName, int IsDir, int DirType, void *pUser);

};

#endif
