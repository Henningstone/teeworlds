#include <fstream>

#include <base/system.h>
#include <engine/storage.h>
#include <engine/server.h>
#include <engine/console.h>
#include <engine/shared/network.h>
#include <engine/config.h>

#include "lua.h"

IServer * CLua::m_pServer = 0; 
CServer * CLua::m_pCServer = 0;
IGameServer * CLua::m_pGameServer = 0;
CGameServer * CLua::m_pCGameServer = 0;

using namespace luabridge;

CLua::CLua()
{
	m_pStorage = 0;
	m_pConsole = 0;
}

CLua::~CLua()
{
	Shutdown();
}

void CLua::Init(IServer *pServer, IStorage *pStorage, IConsole *pConsole)
{
	m_pServer = pServer;
	m_pCServer = (CServer*)pServer;
	m_pStorage = pStorage;
	m_pConsole = pConsole;
	m_aAutoloadFiles.clear();

	//LoadFolder(); // we can't do it that early
}

void CLua::Shutdown()
{
	SaveAutoloads();

	m_pLuaFiles.delete_all();
	m_pLuaFiles.clear();
}

void CLua::SaveAutoloads()
{
	char aFilePath[768];
	fs_storage_path("Teeworlds", aFilePath, sizeof(aFilePath));
	str_append(aFilePath, "/luafiles.cfg", sizeof(aFilePath));
	std::ofstream f(aFilePath, std::ios::out | std::ios::trunc);
	for(int i = 0; i < m_pLuaFiles.size(); i++)
		if(m_pLuaFiles[i]->GetScriptIsAutoload())
			f << m_pLuaFiles[i]->GetFilename() << std::endl;
	f.close();
}

void CLua::SortLuaFiles()
{
	const int NUM = m_pLuaFiles.size();
	if(NUM < 2)
		return;

	for(int curr = 0; curr < NUM-1; curr++)
	{
		int minIndex = curr;
		for(int i = curr + 1; i < NUM; i++)
		{
			int c = 4;
			for(; str_uppercase(m_pLuaFiles[i]->GetFilename()[c]) == str_uppercase(m_pLuaFiles[minIndex]->GetFilename()[c]); c++);
			if(str_uppercase(m_pLuaFiles[i]->GetFilename()[c]) < str_uppercase(m_pLuaFiles[minIndex]->GetFilename()[c]))
				minIndex = i;
		}

		if(minIndex != curr)
		{
			CLuaFile* temp = m_pLuaFiles[curr];
			m_pLuaFiles[curr] = m_pLuaFiles[minIndex];
			m_pLuaFiles[minIndex] = temp;
		}
	}
}

void CLua::SetGameServer(IGameServer *pGameServer)
{
	CLua::m_pGameServer = pGameServer;
	CLua::m_pCGameServer = (CGameServer*)pGameServer;
}

void CLua::AddUserscript(const char *pFilename)
{
	if(!pFilename || pFilename[0] == '\0' || str_length(pFilename) <= 4 || str_comp_nocase(&pFilename[str_length(pFilename)]-4, ".lua")
																		&& str_comp_nocase(&pFilename[str_length(pFilename)]-4, ".clc")) // "compiled lua chunk"
		return;

	// don't add duplicates
	for(int i = 0; i < m_pLuaFiles.size(); i++)
		if(str_comp(m_pLuaFiles[i]->GetFilename(), pFilename) == 0)
			return;

	bool Compiled = str_comp_nocase(&pFilename[str_length(pFilename)]-4, ".clc") == 0;

	std::string file = pFilename;

	// check for autoload
	bool Autoload = false;
	for(int i = 0; i < m_aAutoloadFiles.size(); i++)
		if(m_aAutoloadFiles[i] == file)
			Autoload = true;

	dbg_msg("Lua", "adding%sscript '%s' to the list", Compiled ? " COMPILED " : " ", file.c_str());

	int index = m_pLuaFiles.add(new CLuaFile(this, file, Autoload));
	if(Autoload)
		m_pLuaFiles[index]->Init();
}

void CLua::LoadFolder()
{
	LoadFolder("lua");
}

void CLua::LoadFolder(const char *pFolder)
{
	// get the files which should be auto-loaded from file
	{
		m_aAutoloadFiles.clear();
		char aFilePath[768];
		fs_storage_path("Teeworlds", aFilePath, sizeof(aFilePath));
		str_append(aFilePath, "/luafiles.cfg", sizeof(aFilePath));
		std::ifstream f(aFilePath);
		if(f.is_open())
		{
			std::string line;
			while(std::getline(f, line))
				m_aAutoloadFiles.add(line);
			f.close();
		}
	}

	//char FullDir[256];
	//str_format(FullDir, sizeof(FullDir), "lua");

	dbg_msg("Lua", "Loading Folder '%s'", pFolder);
	CLua::LuaLoadHelper * pParams = new CLua::LuaLoadHelper;
	pParams->pLua = this;
	pParams->pString = pFolder;

	m_pStorage->ListDirectory(IStorage::TYPE_ALL, pFolder, LoadFolderCallback, pParams);

	delete pParams;

	SortLuaFiles();
}

int CLua::LoadFolderCallback(const char *pName, int IsDir, int DirType, void *pUser)
{
	if(pName[0] == '.')
		return 0;

	LuaLoadHelper *pParams = (LuaLoadHelper *)pUser;

	CLua *pSelf = pParams->pLua;
	const char *pFullDir = pParams->pString;

	char File[64];
	str_format(File, sizeof(File), "%s/%s", pFullDir, pName);
	//dbg_msg("Lua", "-> Found File %s", File);

	if(IsDir)
		pParams->pLua->LoadFolder(File);
	else
		pSelf->AddUserscript(File);
	return 0;
}

int CLua::HandleException(std::exception &e, CLuaFile* culprit)
{
	for(int i = 0; i < m_ErrorCounter.size(); i++)
	{
		if(m_ErrorCounter[i].culprit == culprit)
		{
			char aError[1024];
			str_format(aError, sizeof(aError), "{%i/511} %s", m_ErrorCounter[i].count, e.what());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lua|EXCEPTION", aError);
			if(++m_ErrorCounter[i].count < 512)
				return m_ErrorCounter[i].count;
			m_ErrorCounter[i].count = 0;
			culprit->m_pErrorStr = "Error count limit exceeded (too many exceptions thrown)";
			culprit->Unload(true);
			dbg_msg("lua|ERROR", "<<< unloaded script '%s' (error count exceeded limit)", culprit->GetFilename());
		}
	}

	LuaErrorCounter x;
	x.culprit = culprit;
	x.count = 1;
	m_ErrorCounter.add(x);

	return 1;
}

int CLua::Panic(lua_State *L)
{
	dbg_msg("LUA/FATAL", "panic [%p] %s", L, lua_tostring(L, -1));
	dbg_break();
	return 0;
}

int CLua::ErrorFunc(lua_State *L)
{
	dbg_msg("Lua", "Lua Script Error! :");
	//lua_getglobal(L, "pLUA");
	//CLua *pSelf = (CLua *)lua_touserdata(L, -1);
	//lua_pop(L, 1);

	//int depth = 0;
	//int frameskip = 1;
	//lua_Debug frame;

	if (lua_tostring(L, -1) == 0)
	{
		//dbg_msg("Lua", "PANOS");
		return 0;
	}
	
	//dbg_msg("Lua", pSelf->m_aFilename);
	dbg_msg("Lua", lua_tostring(L, -1));
	/*dbg_msg("Lua", "Backtrace:");

	while(lua_getstack(L, depth, &frame) == 1)
	{
		depth++;
		lua_getinfo(L, "nlSf", &frame);
		// check for functions that just report errors. these frames just confuses more then they help
		if(frameskip && str_comp(frame.short_src, "[C]") == 0 && frame.currentline == -1)
			continue;
		frameskip = 0;
		// print stack frame
		dbg_msg("Lua", "%s(%d): %s %s", frame.short_src, frame.currentline, frame.name, frame.namewhat);
	}*/
	lua_pop(L, 1); // remove error message
	lua_gc(L, LUA_GCCOLLECT, 0);
	return 0;
}
