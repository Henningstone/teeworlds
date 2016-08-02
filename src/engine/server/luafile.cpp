#include <fstream>
#include "lua.h"

#include "luafile.h"

CLuaFile::CLuaFile(CLua *pLua, std::string Filename) : m_pLua(pLua), m_Filename(Filename)
{
	m_pLuaState = 0;
	m_pErrorStr = 0;
	mem_zero(m_aScriptInfo, sizeof(m_aScriptInfo));

	Init();
}

CLuaFile::~CLuaFile()
{

}

void CLuaFile::Init()
{
	LoadPermissionFlags();
	OpenLua();

	dbg_assert(LoadFile("data/luabase/events.lua"), "failed to load events.lua");
	RegisterLuaCallbacks(m_pLuaState);
	dbg_assert(LoadFile(m_Filename.c_str()), (std::string("failed to load script ")+m_Filename).c_str());

	// gather basic global infos from the script
	lua_getglobal(m_pLuaState, "g_ScriptInfo");
	if(lua_isstring(m_pLuaState, -1))
		str_copy(m_aScriptInfo, lua_tostring(m_pLuaState, -1), sizeof(m_aScriptInfo));
	lua_pop(m_pLuaState, -1);

	// pass the uid to the script
	lua_pushlightuserdata(m_pLuaState, this);
	lua_setglobal(m_pLuaState, "g_Self");

	// call the OnScriptInit function if we have one
	bool success = true;
	LUA_CALL_FUNC(m_pLuaState, "OnScriptInit", bool, success);
	dbg_assert(success, (m_Filename+std::string(" rejected being loaded, did 'OnScriptInit()' return true...?")).c_str());
}

void CLuaFile::OpenLua()
{
	dbg_assert(m_pLuaState == NULL, (m_Filename+std::string(" OpenLua() called twice")).c_str());
	dbg_assert((m_pLuaState = luaL_newstate()) != NULL, (std::string("failed to create lua_state for ")+m_Filename).c_str());

	lua_atpanic(m_pLuaState, CLua::Panic);
	lua_register(m_pLuaState, "errorfunc", CLua::ErrorFunc);
	//lua_register(m_pLuaState, "print", CLuaFile::LuaPrintOverride);

	//luaL_openlibs(m_pLuaState);  // we don't need certain libs -> open them all manually

	luaopen_base(m_pLuaState);	// base
	luaopen_math(m_pLuaState);	// math.* functions
	luaopen_string(m_pLuaState);// string.* functions
	luaopen_table(m_pLuaState);	// table operations
	luaopen_bit(m_pLuaState);	// bit operations
	//luaopen_jit(m_pLuaState);	// control the jit-compiler [not needed]

	//if(m_PermissionFlags&LUAFILE_PERMISSION_IO)
		luaopen_io(m_pLuaState);	// input/output of files
	//if(m_PermissionFlags&LUAFILE_PERMISSION_DEBUG) XXX
	luaopen_debug(m_pLuaState);	// debug stuff for whatever... can be removed in further patches
	//if(m_PermissionFlags&LUAFILE_PERMISSION_FFI)
		luaopen_ffi(m_pLuaState);	// register and write own C-Functions and call them in lua (whoever may need that...)
	//if(m_PermissionFlags&LUAFILE_PERMISSION_OS) XXX
	luaopen_os(m_pLuaState);	// evil
	//if(m_PermissionFlags&LUAFILE_PERMISSION_PACKAGE)
		luaopen_package(m_pLuaState); //used for modules etc... not sure whether we should load this
}

void CLuaFile::LoadPermissionFlags()
{
	std::ifstream f(m_Filename.c_str());
	std::string line; bool searching = true;
	while(std::getline(f, line))
	{
		if(line.find("]]") != std::string::npos)
			break;

		if(searching && line != "--[[#!")
			continue;
		if(searching)
		{
			searching = false;
			continue;
		}

		// make sure we only get what we want
		char aBuf[32]; char *p;
		str_copy(aBuf, line.c_str(), sizeof(aBuf));
		str_sanitize_strong(aBuf);
		p = aBuf;
		while(*p == ' ' || *p == '\t')
			p++;

		// some sort of syntax error there? just ignore the line
		if(p++[0] != '#')
			continue;

		if(str_comp_nocase("io", p) == 0)
			m_PermissionFlags |= LUAFILE_PERMISSION_IO;
		if(str_comp_nocase("debug", p) == 0)
			m_PermissionFlags |= LUAFILE_PERMISSION_DEBUG;
		if(str_comp_nocase("ffi", p) == 0)
			m_PermissionFlags |= LUAFILE_PERMISSION_FFI;
		if(str_comp_nocase("os", p) == 0)
			m_PermissionFlags |= LUAFILE_PERMISSION_OS;
		if(str_comp_nocase("package", p) == 0)
			m_PermissionFlags |= LUAFILE_PERMISSION_PACKAGE;
	}

	//m_PermissionFlags |= LUAFILE_PERMISSION_OS;
	//m_PermissionFlags |= LUAFILE_PERMISSION_DEBUG;
}


luabridge::LuaRef CLuaFile::GetFunc(const char *pFuncName)
{
	LuaRef func = getGlobal(m_pLuaState, pFuncName);
	if(func == 0)
		dbg_msg("lua", "error: function '%s' not found in file '%s'", pFuncName, m_Filename.c_str());

	return func;  // return 0 if the function is not found!
}


bool CLuaFile::LoadFile(const char *pFilename)
{
	if(!pFilename || pFilename[0] == '\0' || str_length(pFilename) <= 4 ||
	   (str_comp_nocase(&pFilename[str_length(pFilename)]-4, ".lua") &&
		str_comp_nocase(&pFilename[str_length(pFilename)]-4, ".clc") &&
		str_comp_nocase(&pFilename[str_length(pFilename)]-7, ".config")) || !m_pLuaState)
		return false;

	// make sure that source code scripts are what they're supposed to be
	bool Compiled = str_comp_nocase(&pFilename[str_length(pFilename)]-4, ".clc") == 0;
	IOHANDLE f = io_open(pFilename, IOFLAG_READ);
	if(!f)
	{
		dbg_msg("Failed to open script '%s' for integrity check", pFilename);
		return false;
	}

	char aData[sizeof(LUA_SIGNATURE)] = {0};
	io_read(f, aData, sizeof(aData));
	io_close(f);
	char aHeader[2][7];
	str_format(aHeader[0], sizeof(aHeader[0]), "\\x%02x%s", aData[0], aData+1);
	str_format(aHeader[1], sizeof(aHeader[1]), "\\x%02x%s", LUA_SIGNATURE[0], LUA_SIGNATURE+1);

	if(str_comp(aHeader[0], aHeader[1]) == 0 && !Compiled)
	{
		dbg_msg("lua", "!! WARNING: PREVENTED LOADING A PRECOMPILED SCRIPT PRETENDING TO BE A SOURCE CODE SCRIPT !!");
		dbg_msg("lua", "!! :  %s", pFilename);
		return false;
	}
	else if(str_comp(aHeader[0], aHeader[1]) != 0 && Compiled)
	{
		dbg_msg("lua", "!! WARNING: PREVENTED LOADING AN INVALID PRECOMPILED SCRIPT (%s != %s) !!", aHeader[0], aHeader[1]);
		dbg_msg("lua", "!! :  %s", pFilename);
		return false;
	}


	int Status = luaL_loadfile(m_pLuaState, pFilename);
	if (Status != 0)
	{
		CLua::ErrorFunc(m_pLuaState);
		return false;
	}

	lua_resume(m_pLuaState, 0);

	//Status = lua_pcall(m_pLuaState, 0, LUA_MULTRET, 0);
	//if (Status)
	//{
	//	CLua::ErrorFunc(m_pLuaState);
	//	return false;
	//}

	return true;
}
