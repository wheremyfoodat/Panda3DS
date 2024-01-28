#include <stdio.h>
#include <imgui.h>
#include <deque>

#include "lua.hpp"

#define ENABLE_IM_LUA_END_STACK

#ifdef ENABLE_IM_LUA_END_STACK
// Stack for imgui begin and end
std::deque<int> endStack;
static void AddToStack(int type) {
	endStack.push_back(type);
}

static void PopEndStack(int type) {
	if (!endStack.empty()) {
		endStack.pop_back(); // hopefully the type matches
	}
}

static void ImEndStack(int type);

#endif

#define IMGUI_FUNCTION_DRAW_LIST(name) \
static int impl_draw_list_##name(lua_State *L) { \
  int max_args = lua_gettop(L); \
  int arg = 1; \
  int stackval = 0;

#define IMGUI_FUNCTION(name) \
static int impl_##name(lua_State *L) { \
  int max_args = lua_gettop(L); \
  int arg = 1; \
  int stackval = 0;

// I use OpenGL so this is a GLuint
// Using unsigned int cause im lazy don't copy me
#define IM_TEXTURE_ID_ARG(name) \
  const ImTextureID name = (ImTextureID)luaL_checkinteger(L, arg++);

#define OPTIONAL_LABEL_ARG(name, otherwise) \
  const char* name; \
  if (arg <= max_args) { \
    name = lua_tostring(L, arg++); \
  } else { \
    name = otherwise; \
  }

#define LABEL_ARG(name) \
  size_t i_##name##_size; \
  const char * name = luaL_checklstring(L, arg++, &(i_##name##_size));

#define IM_VEC_2_ARG(name)\
  const lua_Number i_##name##_x = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_y = luaL_checknumber(L, arg++); \
  const ImVec2 name((double)i_##name##_x, (double)i_##name##_y);

#define OPTIONAL_IM_VEC_2_ARG(name, x, y) \
  lua_Number i_##name##_x = x; \
  lua_Number i_##name##_y = y; \
  if (arg <= max_args - 1) { \
    i_##name##_x = luaL_checknumber(L, arg++); \
    i_##name##_y = luaL_checknumber(L, arg++); \
  } \
  const ImVec2 name((double)i_##name##_x, (double)i_##name##_y);

#define IM_VEC_4_ARG(name) \
  const lua_Number i_##name##_x = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_y = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_z = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_w = luaL_checknumber(L, arg++); \
  const ImVec4 name((double)i_##name##_x, (double)i_##name##_y, (double)i_##name##_z, (double)i_##name##_w);

#define OPTIONAL_IM_VEC_4_ARG(name, x, y, z, w) \
  lua_Number i_##name##_x = x; \
  lua_Number i_##name##_y = y; \
  lua_Number i_##name##_z = z; \
  lua_Number i_##name##_w = w; \
  if (arg <= max_args - 1) { \
    i_##name##_x = luaL_checknumber(L, arg++); \
    i_##name##_y = luaL_checknumber(L, arg++); \
    i_##name##_z = luaL_checknumber(L, arg++); \
    i_##name##_w = luaL_checknumber(L, arg++); \
  } \
  const ImVec4 name((double)i_##name##_x, (double)i_##name##_y, (double)i_##name##_z, (double)i_##name##_w);

#define NUMBER_ARG(name)\
  lua_Number name = luaL_checknumber(L, arg++);

#define OPTIONAL_NUMBER_ARG(name, otherwise)\
  lua_Number name = otherwise; \
  if (arg <= max_args) { \
    name = lua_tonumber(L, arg++); \
  }

#define FLOAT_POINTER_ARG(name) \
  float i_##name##_value = luaL_checknumber(L, arg++); \
  float* name = &(i_##name##_value);

#define END_FLOAT_POINTER(name) \
  if (name != NULL) { \
    lua_pushnumber(L, i_##name##_value); \
    stackval++; \
  }

#define FLOAT_ARRAY_ARG(name) \
  if (!lua_istable(L, arg)) { \
    return luaL_error(L, "Expected table for argument %d", arg); \
  } \
  int name##_count = lua_objlen(L, arg); \
  float* name = new float[name##_count]; \
  for (int i = 0; i < name##_count; i++) { \
    lua_rawgeti(L, arg, i + 1); \
    name[i] = luaL_checknumber(L, -1); \
    lua_pop(L, 1); \
  } \
  arg++;

#define END_FLOAT_ARRAY(name) \
  delete[] name;

#define OPTIONAL_INT_ARG(name, otherwise)\
  int name = otherwise; \
  if (arg <= max_args) { \
    name = (int)lua_tonumber(L, arg++); \
  }

#define INT_ARG(name) \
  const int name = (int)luaL_checknumber(L, arg++);

#define OPTIONAL_UINT_ARG(name, otherwise)\
  unsigned int name = otherwise; \
  if (arg <= max_args) { \
    name = (unsigned int)lua_tonumber(L, arg++); \
  }

#define UINT_ARG(name) \
  const unsigned int name = (unsigned int)luaL_checkinteger(L, arg++);

#define INT_POINTER_ARG(name) \
  int i_##name##_value = (int)luaL_checkinteger(L, arg++); \
  int* name = &(i_##name##_value);

#define END_INT_POINTER(name) \
  if (name != NULL) { \
    lua_pushnumber(L, i_##name##_value); \
    stackval++; \
  }

#define UINT_POINTER_ARG(name) \
  unsigned int i_##name##_value = (unsigned int)luaL_checkinteger(L, arg++); \
  unsigned int* name = &(i_##name##_value);

#define END_UINT_POINTER(name) \
  if (name != NULL) { \
    lua_pushnumber(L, i_##name##_value); \
    stackval++; \
  }

#define BOOL_POINTER_ARG(name) \
  bool i_##name##_value = lua_toboolean(L, arg++); \
  bool* name = &(i_##name##_value);

#define OPTIONAL_BOOL_POINTER_ARG(name) \
  bool i_##name##_value; \
  bool* name = NULL; \
  if (arg <= max_args) { \
    i_##name##_value = lua_toboolean(L, arg++); \
    name = &(i_##name##_value); \
  }

#define OPTIONAL_BOOL_ARG(name, otherwise) \
  bool name = otherwise; \
  if (arg <= max_args) { \
    name = lua_toboolean(L, arg++); \
  }

#define BOOL_ARG(name) \
  bool name = lua_toboolean(L, arg++);

#define CALL_FUNCTION(name, retType,...) \
  retType ret = ImGui::name(__VA_ARGS__);

#define DRAW_LIST_CALL_FUNCTION(name, retType,...) \
  retType ret = ImGui::GetWindowDrawList()->name(__VA_ARGS__);

#define CALL_FUNCTION_NO_RET(name, ...) \
  ImGui::name(__VA_ARGS__);

#define DRAW_LIST_CALL_FUNCTION_NO_RET(name, ...) \
  ImGui::GetWindowDrawList()->name(__VA_ARGS__);

#define PUSH_STRING(name) \
  lua_pushstring(L, name); \
  stackval++;

#define PUSH_NUMBER(name) \
  lua_pushnumber(L, name); \
  stackval++;

#define PUSH_BOOL(name) \
  lua_pushboolean(L, (int) name); \
  stackval++;

#define END_BOOL_POINTER(name) \
  if (name != NULL) { \
    lua_pushboolean(L, (int)i_##name##_value); \
    stackval++; \
  }

#define END_IMGUI_FUNC \
  return stackval; \
}

#ifdef ENABLE_IM_LUA_END_STACK
#define IF_RET_ADD_END_STACK(type) \
  if (ret) { \
    AddToStack(type); \
  }

#define ADD_END_STACK(type) \
  AddToStack(type);

#define POP_END_STACK(type) \
  PopEndStack(type);

#define END_STACK_START \
static void ImEndStack(int type) { \
  switch(type) {

#define END_STACK_OPTION(type, function) \
    case type: \
      ImGui::function(); \
      break;

#define END_STACK_END \
  } \
}
#else
#define END_STACK_START
#define END_STACK_OPTION(type, function)
#define END_STACK_END
#define IF_RET_ADD_END_STACK(type)
#define ADD_END_STACK(type)
#define POP_END_STACK(type)
#endif

#define START_ENUM(name)
#define MAKE_ENUM(c_name,lua_name)
#define END_ENUM(name)

#include "imgui_iterator.inl"


static const struct luaL_Reg imguilib[] = {
#undef IMGUI_FUNCTION
#define IMGUI_FUNCTION(name) {#name, impl_##name},
#undef IMGUI_FUNCTION_DRAW_LIST
#define IMGUI_FUNCTION_DRAW_LIST(name) {"DrawList_" #name, impl_draw_list_##name},
	// These defines are just redefining everything to nothing so
	// we can get the function names
	#undef IM_TEXTURE_ID_ARG
	#define IM_TEXTURE_ID_ARG(name)
	#undef OPTIONAL_LABEL_ARG
	#define OPTIONAL_LABEL_ARG(name, otherwise)
	#undef LABEL_ARG
	#define LABEL_ARG(name)
	#undef IM_VEC_2_ARG
	#define IM_VEC_2_ARG(name)
	#undef OPTIONAL_IM_VEC_2_ARG
	#define OPTIONAL_IM_VEC_2_ARG(name, x, y)
	#undef IM_VEC_4_ARG
	#define IM_VEC_4_ARG(name)
	#undef OPTIONAL_IM_VEC_4_ARG
	#define OPTIONAL_IM_VEC_4_ARG(name, x, y, z, w)
	#undef NUMBER_ARG
	#define NUMBER_ARG(name)
	#undef OPTIONAL_NUMBER_ARG
	#define OPTIONAL_NUMBER_ARG(name, otherwise)
	#undef FLOAT_POINTER_ARG
	#define FLOAT_POINTER_ARG(name)
	#undef END_FLOAT_POINTER
	#define END_FLOAT_POINTER(name)
	#undef FLOAT_ARRAY_ARG
	#define FLOAT_ARRAY_ARG(name)
	#undef END_FLOAT_ARRAY
	#define END_FLOAT_ARRAY(name)
	#undef OPTIONAL_INT_ARG
	#define OPTIONAL_INT_ARG(name, otherwise)
	#undef INT_ARG
	#define INT_ARG(name)
	#undef OPTIONAL_UINT_ARG
	#define OPTIONAL_UINT_ARG(name, otherwise)
	#undef UINT_ARG
	#define UINT_ARG(name)
	#undef INT_POINTER_ARG
	#define INT_POINTER_ARG(name)
	#undef END_INT_POINTER
	#define END_INT_POINTER(name)
	#undef UINT_POINTER_ARG
	#define UINT_POINTER_ARG(name)
	#undef END_UINT_POINTER
	#define END_UINT_POINTER(name)
	#undef BOOL_POINTER_ARG
	#define BOOL_POINTER_ARG(name)
	#undef OPTIONAL_BOOL_POINTER_ARG
	#define OPTIONAL_BOOL_POINTER_ARG(name)
	#undef OPTIONAL_BOOL_ARG
	#define OPTIONAL_BOOL_ARG(name, otherwise)
	#undef BOOL_ARG
	#define BOOL_ARG(name)
	#undef CALL_FUNCTION
	#define CALL_FUNCTION(name, retType, ...)
	#undef DRAW_LIST_CALL_FUNCTION
	#define DRAW_LIST_CALL_FUNCTION(name, retType, ...)
	#undef CALL_FUNCTION_NO_RET
	#define CALL_FUNCTION_NO_RET(name, ...)
	#undef DRAW_LIST_CALL_FUNCTION_NO_RET
	#define DRAW_LIST_CALL_FUNCTION_NO_RET(name, ...)
	#undef PUSH_STRING
	#define PUSH_STRING(name)
	#undef PUSH_NUMBER
	#define PUSH_NUMBER(name)
	#undef PUSH_BOOL
	#define PUSH_BOOL(name)
	#undef END_BOOL_POINTER
	#define END_BOOL_POINTER(name)
	#undef END_IMGUI_FUNC
	#define END_IMGUI_FUNC
	#undef END_STACK_START
	#define END_STACK_START
	#undef END_STACK_OPTION
	#define END_STACK_OPTION(type, function)
	#undef END_STACK_END
	#define END_STACK_END
	#undef IF_RET_ADD_END_STACK
	#define IF_RET_ADD_END_STACK(type)
	#undef ADD_END_STACK
	#define ADD_END_STACK(type)
	#undef POP_END_STACK
	#define POP_END_STACK(type)
	#undef START_ENUM
	#define START_ENUM(name)
	#undef MAKE_ENUM
	#define MAKE_ENUM(c_name,lua_name)
	#undef END_ENUM
	#define END_ENUM(name)

	#include "imgui_iterator.inl"
//	  {"Button", impl_Button},
	  {NULL, NULL}
};

static void PushImguiEnums(lua_State* lState, const char* tableName) {
	lua_pushstring(lState, tableName);
	lua_newtable(lState);

#undef START_ENUM
#undef MAKE_ENUM
#undef END_ENUM
#define START_ENUM(name) \
  lua_pushstring(lState, #name); \
  lua_newtable(lState); \
  { \
    int i = 1;
#define MAKE_ENUM(c_name,lua_name) \
  lua_pushstring(lState, #lua_name); \
  lua_pushnumber(lState, c_name); \
  lua_rawset(lState, -3);
#define END_ENUM(name) \
  } \
  lua_rawset(lState, -3);
	// These defines are just redefining everything to nothing so
	// we get only the enums.
#undef IMGUI_FUNCTION
#define IMGUI_FUNCTION(name)
#undef IMGUI_FUNCTION_DRAW_LIST
#define IMGUI_FUNCTION_DRAW_LIST(name)
#undef IM_TEXTURE_ID_ARG
#define IM_TEXTURE_ID_ARG(name)
#undef OPTIONAL_LABEL_ARG
#define OPTIONAL_LABEL_ARG(name, otherwise)
#undef LABEL_ARG
#define LABEL_ARG(name)
#undef IM_VEC_2_ARG
#define IM_VEC_2_ARG(name)
#undef OPTIONAL_IM_VEC_2_ARG
#define OPTIONAL_IM_VEC_2_ARG(name, x, y)
#undef IM_VEC_4_ARG
#define IM_VEC_4_ARG(name)
#undef OPTIONAL_IM_VEC_4_ARG
#define OPTIONAL_IM_VEC_4_ARG(name, x, y, z, w)
#undef NUMBER_ARG
#define NUMBER_ARG(name)
#undef OPTIONAL_NUMBER_ARG
#define OPTIONAL_NUMBER_ARG(name, otherwise)
#undef FLOAT_POINTER_ARG
#define FLOAT_POINTER_ARG(name)
#undef END_FLOAT_POINTER
#define END_FLOAT_POINTER(name)
#undef FLOAT_ARRAY_ARG
#define FLOAT_ARRAY_ARG(name)
#undef END_FLOAT_ARRAY
#define END_FLOAT_ARRAY(name)
#undef OPTIONAL_INT_ARG
#define OPTIONAL_INT_ARG(name, otherwise)
#undef INT_ARG
#define INT_ARG(name)
#undef OPTIONAL_UINT_ARG
#define OPTIONAL_UINT_ARG(name, otherwise)
#undef UINT_ARG
#define UINT_ARG(name)
#undef INT_POINTER_ARG
#define INT_POINTER_ARG(name)
#undef END_INT_POINTER
#define END_INT_POINTER(name)
#undef UINT_POINTER_ARG
#define UINT_POINTER_ARG(name)
#undef END_UINT_POINTER
#define END_UINT_POINTER(name)
#undef BOOL_POINTER_ARG
#define BOOL_POINTER_ARG(name)
#undef OPTIONAL_BOOL_POINTER_ARG
#define OPTIONAL_BOOL_POINTER_ARG(name)
#undef OPTIONAL_BOOL_ARG
#define OPTIONAL_BOOL_ARG(name, otherwise)
#undef BOOL_ARG
#define BOOL_ARG(name)
#undef CALL_FUNCTION
#define CALL_FUNCTION(name, retType, ...)
#undef DRAW_LIST_CALL_FUNCTION
#define DRAW_LIST_CALL_FUNCTION(name, retType, ...)
#undef CALL_FUNCTION_NO_RET
#define CALL_FUNCTION_NO_RET(name, ...)
#undef DRAW_LIST_CALL_FUNCTION_NO_RET
#define DRAW_LIST_CALL_FUNCTION_NO_RET(name, ...)
#undef PUSH_STRING
#define PUSH_STRING(name)
#undef PUSH_NUMBER
#define PUSH_NUMBER(name)
#undef PUSH_BOOL
#define PUSH_BOOL(name)
#undef END_BOOL_POINTER
#define END_BOOL_POINTER(name)
#undef END_IMGUI_FUNC
#define END_IMGUI_FUNC
#undef END_STACK_START
#define END_STACK_START
#undef END_STACK_OPTION
#define END_STACK_OPTION(type, function)
#undef END_STACK_END
#define END_STACK_END
#undef IF_RET_ADD_END_STACK
#define IF_RET_ADD_END_STACK(type)
#undef ADD_END_STACK
#define ADD_END_STACK(type)
#undef POP_END_STACK
#define POP_END_STACK(type)

#include "imgui_iterator.inl"

	lua_rawset(lState, -3);
};

void LoadImguiBindings(lua_State* lState) {
	lua_newtable(lState);
	luaL_setfuncs(lState, imguilib, 0);
	PushImguiEnums(lState, "constant");
	lua_setglobal(lState, "imgui");
}
