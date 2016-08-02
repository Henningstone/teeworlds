#include <type_traits>

#include "luafile.h"

template<typename T>
T CLuaFile::CallFunc(const char *pFuncName, const CLuaFile::LuaCallfuncParams& params)
{
	// push the function onto the stack
	lua_getglobal(m_pLuaState, pFuncName);
	if(!lua_isfunction(m_pLuaState, lua_gettop(m_pLuaState)))
		throw CLuaFile::CallException(m_Filename.c_str(), (std::string("Couldn't get function ") + std::string(pFuncName)).c_str());

	// push the arguments onto the stack (in direct order)
	if(!lua_checkstack(params.Num()+2))
		throw CLuaFile::CallException(m_Filename.c_str(), (std::string("Could not allocate enough stackspace to call ") + std::string(pFuncName)).c_str());
		
	CLuaFile::LuaCallfuncParams::Arg_base* conductor = (CLuaFile::LuaCallfuncParams::Arg_base*)params.BasePtr();
	for(int i = 0; i < params.Num(); i++)
	{
		switch(conductor->t)
		{
			case LuaCallfuncParams::CALLTYPE_INT:
				lua_pushinteger(m_pLuaState, ((LuaCallfuncParams::Arg<int>*)conductor)->v);
				//conductor += sizeof(LuaCallfuncParams::Arg<int>);
				break;

			case LuaCallfuncParams::CALLTYPE_BOOL:
				lua_pushboolean(m_pLuaState, (int)((LuaCallfuncParams::Arg<bool>*)conductor)->v);
				//conductor += sizeof(LuaCallfuncParams::Arg<bool>);
				break;
		}
		conductor = conductor->Next();
	}

	if(lua_pcall(m_pLuaState, params.Num(), 1, 0) != 0)
	{
		std::string what(lua_tostring(m_pLuaState, -1));
		lua_pop(m_pLuaState, 1);
		throw CLuaFile::CallException(m_Filename.c_str(), (std::string("Lua Runtime Error: ")+what).c_str());
	}

	// handle returns
	T ret;
	if(std::is_same<T, int>::value)
		ret = (T)lua_tointeger(m_pLuaState, lua_gettop(m_pLuaState));
	else if(std::is_same<T, bool>::value)
		ret = (T)lua_tointeger(m_pLuaState, lua_gettop(m_pLuaState));

	lua_pop(m_pLuaState, 1);
	return ret;
}
