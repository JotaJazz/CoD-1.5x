#include "shared.h"

Cvar_Get_t Cvar_Get = (Cvar_Get_t)0x43B880;
Cvar_Set_t Cvar_Set = (Cvar_Set_t)0x43BBB0;
Cvar_FindVar_t Cvar_FindVar = (Cvar_FindVar_t)0x43B790;

int Cvar_VariableIntegerValue(const char *var_name) {
	cvar_t  *var;

	var = Cvar_FindVar(var_name);
	if (!var) {
		return 0;
	}
	return var->integer;
}
