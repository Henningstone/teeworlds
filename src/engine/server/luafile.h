#ifndef ENGINE_SERVER_LUAFILE_H
#define ENGINE_SERVER_LUAFILE_H

#include <lua.hpp>
#include <engine/external/luabridge/LuaBridge.h>
#include <engine/external/luabridge/RefCountedPtr.h>

#include "base/system.h"

#define LUA_CALL_FUNC(LUA_STATE, FUNC_NAME, TYPE, RETURN, ...) { try { \
	LuaRef func = getGlobal(LUA_STATE, FUNC_NAME); \
	if(func) \
		RETURN = func(__VA_ARGS__).cast<TYPE>(); }\
	catch (std::exception& e) \
	{ printf("LUA EXCEPTION: %s\n", e.what()); } }

class IServer;
class IStorage;
class CLua;

class CLuaFile
{
	friend class CLuaBinding;

public:
	class CallException : public std::exception
	{
	public:
		CallException(const char *pFilename, const char *what)
		{
			dbg_msg("luafile/CALLEXCEPTION", "%s  ||  what: %s", pFilename, what);
		}
	};

	enum
	{
		LUAFILE_PERMISSION_IO		= 1 << 0,
		LUAFILE_PERMISSION_DEBUG	= 1 << 1,
		LUAFILE_PERMISSION_FFI		= 1 << 2,
		LUAFILE_PERMISSION_OS		= 1 << 3,
		LUAFILE_PERMISSION_PACKAGE	= 1 << 4,
		LUAFILE_NUM_PERMISSIONS
	};

	const char *m_pErrorStr;

	class LuaCallfuncParams
	{
		int m_NumArgs;
		void *m_pArgList;

	public:
		struct Arg_base
		{
			~Arg_base()
			{
				if(next)
					delete next;
			}

			int t;
			Arg_base* Next() const { return next; }

		protected:
			Arg_base* next;
		};

		template<typename T>
		struct Arg : public Arg_base
		{
			void* operator new(size_t size) { return mem_alloc((unsigned int)size, 0); }
			void operator delete(void *block) { mem_free(block); }

			T v;
			Arg(int type, T val, Arg_base *append_to=0) : t(type), v(val) { if(append_to) append_to->next = this; }
		};


		LuaCallfuncParams()
		{
			m_NumArgs = 0;
			m_pArgList = 0;
		}

		~LuaCallfuncParams()
		{
			if(m_pArgList)
				delete m_pArgList;
		}

		template <typename T>
		LuaCallfuncParams& AddArg(int type, T val)
		{
			Arg_base<T>* next = 0;

			if(!m_pArgList)
				m_pArgList = new Arg<T>(type, val);
			else
			{
				next = (Arg<T>*)m_pArgList;
				while(next->Next() != NULL)
					next = next->Next();
				new Arg<T>(type, val, next);
			}

			m_NumArgs++;
			return *this;
		}

		inline int Num() const { return m_NumArgs; }
		void* BasePtr() const { return m_pArgList; }


		enum
		{
			CALLTYPE_INT = 0,
			CALLTYPE_BOOL,

		};

	};

private:
	CLua *m_pLua;
	lua_State *m_pLuaState;
	std::string m_Filename;

	//int m_UID; // the script can use this to identify itself
	int m_PermissionFlags;

	char m_aScriptInfo[128];

public:
	CLuaFile(CLua *pLua, std::string Filename);
	~CLuaFile();
	void Init();
	void LoadPermissionFlags();
	luabridge::LuaRef GetFunc(const char *pFuncName);

	// kinda lowlevel stuff
	/**
	 * Calls a lua function in an somehow epic way. I just wanted to try a concept I worked out for this.
	 * @param pFuncName - Name of the function to call
	 * @param params - The params.
	 * @return The value that the lua function returned
	 * @remark The behavior is undefined if the function is called with an unknown <code>T</code> type
	 */
	template<typename T>
	T CallFunc(const char *pFuncName, const LuaCallfuncParams& params);

	//int GetUID() const { return m_UID; }
	int GetPermissionFlags() const { return m_PermissionFlags; }
	const char* GetFilename() const { return m_Filename.c_str(); }
	const char* GetScriptInfo() const { return m_aScriptInfo; }

	CLua *Lua() const { return m_pLua; }
	
	static void RegisterLuaCallbacks(lua_State * L);

	void LuaPrintOverride(std::string str);

private:
	void OpenLua();
	bool LoadFile(const char *pFilename);
};

#endif
