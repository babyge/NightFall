
#define INCL_DOSMODULEMGR
#include "gamex86.h"
#include "misc.h"
#include "slre.h"
#include "ihuddraw.h"
#include "detours.h"
#include "ClassDef.h"
#include "Player.h"
#include "ScriptThread.h"
#include "Director.h"
#include "ScriptedEvent.h"

//#include "hooks/script.h"
#define BEGININTERMISSION_ADDR 0x3111E910
#define CMDFUNCTIONS_ADDR 0x5E1C38

#ifdef _WIN32
	HMODULE hmod; //Windows
	HMODULE systemHMOD;
#else
	#define _GNU_SOURCE
	void *hmod = 0; //*Nix
#endif


/*
//------------------	Global Variables	------------------\\
*/

typedef gameExport_t *(*pGetGameAPI_spec)( gameImport_t *import );
pGetGameAPI_spec pGetGameAPI;


typedef void (*pG_BeginIntermission_spec)(const char *map_name, INTTYPE_e transtype, bool shouldFade);
pG_BeginIntermission_spec G_BeginIntermission_original;

typedef void *(*pMemoryMalloc_spec)(int size);
pMemoryMalloc_spec pMemoryMalloc;

typedef void (*pMemoryFree_spec)(void *);
pMemoryFree_spec pMemoryFree;

xcommand_t SV_MapRestart_original;
xcommand_t SV_Map_original;
xcommand_t SV_GameMap_original;

gameImport_t	gi;
gameExport_t	*globals;
gameExport_t	globals_backup;

clientdata_t clients[32];
extern iplist_t iplist;
// GLOBALS NEEDED FOR NO-RECOIL DETECTION
int frames = 0;
int lastAnim = VMA_NUMANIMATIONS; //initializing
int start, middle, end;
//

/*
//-----------------------	    End	   -----------------------\\
*/

bool Cmd_HookCommand(const char* cmd_name, xcommand_t *oldFunc, xcommand_t newFunc)
{
	static cmd_function_t **cmd_functions = reinterpret_cast<cmd_function_t **>(CMDFUNCTIONS_ADDR);
	cmd_function_t	*cmd;
	for (cmd = *cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp(cmd_name, cmd->name)) 
		{
			*oldFunc = cmd->function;
			cmd->function = newFunc;
			return true;
		}
	}
	return false;
}

bool Cmd_UnHookCommand(xcommand_t oldFunc, xcommand_t newFunc)
{
	static cmd_function_t **cmd_functions = reinterpret_cast<cmd_function_t **>(CMDFUNCTIONS_ADDR);
	cmd_function_t	*cmd;
	for (cmd = *cmd_functions; cmd; cmd = cmd->next)
	{
		if (cmd->function == newFunc)
		{
			cmd->function = oldFunc;
			return true;
		}
	}
	return false;
}

void SV_MapRestart()
{
	ScriptedEvent sev(SEV_INTERMISSION);
	if (sev.isRegistered())
	{
		sev.Trigger({ INTERM_RESTART });
	}
	SV_MapRestart_original();
}

void SV_Map()
{
	ScriptedEvent sev(SEV_INTERMISSION);

	if (sev.isRegistered())
	{
		sev.Trigger({ INTERM_MAP });
	}
	SV_Map_original();
}

void SV_GameMap()
{
	ScriptedEvent sev(SEV_INTERMISSION);

	if (sev.isRegistered())
	{
		sev.Trigger({ INTERM_MAP });
	}
	SV_GameMap_original();
}

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
void G_ClientBegin( gentity_t *ent, userCmd_t *cmd )
{
	//sizeof(playerState_t);//676
	//sizeof(entityState_t);
	//sizeof(gentity_t);
	//sizeof(int);		  //4
						  //1350
	//sizeof(client_persistant_t);//1588
	//gi.centerprintf(ent, "Welcome %s, have fun!", ent->client->pers.netname); //not working ?
	gi.centerprintf(ent, "Welcome %s, have fun!", ent->client->pers.netname);
	//gi.Printf("Player Name: %s : %d\n", ent->client->pers.netname, ent->client->ps.clientNum);

	globals_backup.ClientBegin( ent, cmd );

	ScriptedEvent sev(SEV_CONNECTED);

	if (sev.isRegistered())
	{
		sev.Trigger({ (Entity*)ent->entity });
	}
}

/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/
char *G_ClientConnect( int clientNum, qboolean firstTime )
{
	char * name ;
	char buff[MAX_INFOSTRING];
	gi.GetUserinfo(clientNum , buff , MAX_INFOSTRING);
	name = Info_ValueForKey(buff,"name");
	if(strHasIP(name))
	{
		gi.Printf("G_ClientConnect Name Detected !\n");
		//Info_SetValueForKey(buff,"name","*** Blank Name ***");
		//gi.setUserinfo(clientNum , buff);
		return "Spamming other server ip|\nDetection By RyBack";
	}
	return globals_backup.ClientConnect( clientNum, firstTime );
}

void G_ClientDisconnect(gentity_t *ent)
{

	ScriptedEvent sev(SEV_DISCONNECTED);

	if (sev.isRegistered())
	{
		sev.Trigger({ (Entity*)ent->entity });
	}
	globals_backup.ClientDisconnect(ent);
}
/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
//*/
//void G_ClientThink( gentity_t *ent, userCmd_t *ucmd, userEyes_t *eyeInfo )
//{
///*
////------------------	Global Variables	------------------\\
//*/
//	
//	float n[3] = { eyeInfo->ofs[0] + ent->client->ps.origin[0], eyeInfo->ofs[1] + ent->client->ps.origin[1], eyeInfo->ofs[2] + ent->client->ps.origin[2] };
//	gentity_t *gEntTo = globals->gentities;		//no need for this right now, but may be useful later - RazoRapid
//	float anglesDifference[3];	//delta angles for computing viewkick
//	//char msg2[256];
//	//char            userinfo[ MAX_INFOSTRING ];
//	//char            stuff[256], cvar[7], value[5];
//	int k;
//	Weapon_t *weapon;
//	int bazooka_or_nade = 0;
///*
////-----------------------	    End	   -----------------------\\
//*/
//	/* ======================================== Start of STWH Detection =================================================*/
//
//	//Simple, but effective anti STWH
//	//Check if we are in the game & currently active players & NOT spectators
//	if(ent->client->pers.team != TEAM_SPECTATOR && ent->entity->health > 0) 
//	{
//		
//		
//			//We check to see if their heade is inside a wall, this ONLY happens with STWH
//			if( gi.pointcontents(n,0) == CONTENTS_SOLID )
//			{
//				//Check if they have a weapon or an item. Otherwise STWH is pointless
//				//Changed from if(ent->client->ps.activeItems), -1: no items in hand  - Razorapid, prev version didn't work well because activeItems is a pointer to an array which is never <= 0
//				if(ent->client->ps.activeItems[0] != -1)
//				{
//					//Check to see if they are trying to shoot when their head is in the wall
//					if(ucmd->buttons & BUTTON_GROUP_FIRE_PRIMARY || ucmd->buttons & BUTTON_GROUP_FIRE_SECONDARY)
//					{
//						//We detected the cheater, now lets kick him
//						SendGameMessage(ent->client->ps.clientNum, HUD_MESSAGE_WHITE, "%s is kicked for using STWH!\n", ent->client->pers.netname);
//						
//						//gi.DropClient( ent->client->ps.clientNum, "Kicked for using STWH!" );
//					}
//				}
//			}
//	}
//
//	/* ======================================== Done with STWH Detection =================================================*/
//
//	/*=========================================== NO-RECOIL DETECTION ===========================================================*/
//	
//	//Check if they have a weapon or not. -1: stands for no weapons/holstered 6: stands for granade 8: stand for bazookas
//	// Bazookas seems to have no-recoil as standard setting
//	
//	// Thirst of all, we go throught every entity in game
//	for(k = 0; k < globals->numEntities; gEntTo++, k++)
//	{
//		// We are checking if this Entity is ET_ITEM - this stands for both items and weapons
//		if(gEntTo->s.eType == ET_ITEM)
//		{
//			// If it's an item or a weapon, we assign it's Entity_s pointer to a Weapon_t struct
//			// since when entity is a weapon, this holds a pointer to weapon struct instead of Entity_s struct
//			
//			weapon = (Weapon_t *)gEntTo->entity;
//			
//			
//			//gi.Printf("Weapon Ptr: %i\n", (DWORD)weapon);
//			//gi.Printf("Weapon String: %i\n", weapon->m_csWeaponGroup);
//			
//			// I did a lil bit calculation here because I guess that some structs aren't 100% right, so
//			// basicly if we hold an item or weapon then 'weapon->owner' value is a pointer to it's owner entity
//			// if we dont hold anything then this value is 0.
//			// We compare it to ent->entity which is a pointer to our player Entity_s struct but since it's
//			// somehow incomplete, we have to add 0x3DC to it manually
//
//			// If this is positive, it means that the owner of the weapon is our player
//			if( weapon->owner == ( (DWORD)(ent->entity) + 0x3DC) )
//			{
//				
//				// so we check if the weapon class is 16: stands for grenade or 32: stands for bazooka
//				// classes are always the same for allies and axis, because weapon class never changes if this is grenade or something else
//				if(weapon->weapon_class == 16 || weapon->weapon_class == 32)
//				{
//					bazooka_or_nade = 1;
//				}
//				else
//				{
//					bazooka_or_nade = 0;
//				}
//				
//			}
//		}
//	}
//				
//				
//	// Here we do the viewkick deltas computation for the fire animation BEGGINING, MIDDLE, END
//	
//	// We check if we hold grenade or bazooka because they don't have a recoil added originally 
//	if(!bazooka_or_nade)
//	{
//		
//		//if previous anim was different than FIRE and actual ANIM is FIRE
//		if(ent->client->ps.viewModelAnim == VMA_FIRE && lastAnim != VMA_FIRE)	//we got the first shoot frame
//		{
//			lastAnim = VMA_FIRE;
//			anglesDifference[0] = eyeInfo->angles[0] - ent->client->ps.viewAngles[0];
//			anglesDifference[1] = eyeInfo->angles[1] - ent->client->ps.viewAngles[1];
//
//			if(anglesDifference[0] == 0 && anglesDifference[1] == 0)
//			{
//				start = 0;
//			}
//			else
//			{
//				start = 1;
//			}
//
//		}
//		else if(ent->client->ps.viewModelAnim == VMA_FIRE && lastAnim == VMA_FIRE)	// this is the next frame from the 2nd frame to the end of the fire anim
//		{
//			frames++;
//		
//			anglesDifference[0] = eyeInfo->angles[0] - ent->client->ps.viewAngles[0];
//			anglesDifference[1] = eyeInfo->angles[1] - ent->client->ps.viewAngles[1];
//			
//			if(frames == 6) // I did it after debug printing frame counts for every weapon when shooting. it's almost middle, every weapon here have already viewkick added
//			{
//				
//				if(anglesDifference[0] == 0 && anglesDifference[1] == 0)
//				{
//					middle = 0;
//				
//				}
//				else
//				{
//					middle = 1;
//				
//				}
//			}
//			// We have to do this again, because we don't know which frame is the last one 
//			
//			if(anglesDifference[0] == 0 && anglesDifference[1] == 0)
//			{
//				end = 0;
//				
//			}
//			else
//			{
//				end = 1;
//			
//			}
//		}
//		else if(ent->client->ps.viewModelAnim != VMA_FIRE && lastAnim == VMA_FIRE)	// lastAnim was the last FIRE ANIM so shooting is done
//		{
//			frames = 0;		// reseting the frames counter
//			lastAnim = ent->client->ps.viewModelAnim;	//setting the actual anim state as the last anim
//			
//			//Here we can check if player is using no recoil
//			//Because the fire animation is done and we have got the info of viewkick deltas
//			if(start != 0 || middle != 0 || end != 0)
//			{
//				gi.Printf("Recoil On\n");
//				//SendGameMessage(ent->client->ps.clientNum, HUD_MESSAGE_WHITE, "%s - NORECOIL is OFF!\n", ent->client->pers.netname);
//			}
//			else if(start == 0 && middle == 0 && end == 0)
//			{
//				gi.Printf("Recoil Off\n");
//				//SendGameMessage(ent->client->ps.clientNum, HUD_MESSAGE_WHITE, "%s - NORECOIL is ON!\n", ent->client->pers.netname);
//			}
//		}
//	}
//
//	
//	//	This was used to check if playerstate viewangles stays the same when shooting while eyeInfo angles change
//	//	Came out to be positive so we now can compute the viewkick deltas ^^
//	//	No need to comment this out, but if it will be needed, you need to declare the char msg2[256] array
//	/*
//		sprintf(msg2,"eyeInfo Viewangles: %f \t, %f \t\n ", eyeInfo->angles[0], eyeInfo->angles[1]);
//		gi.Printf(msg2);
//		sprintf(msg2,"PlayerS Viewangles: %f \t, %f \t\n ", ent->client->ps.viewAngles[0] , ent->client->ps.viewAngles[1]);
//		gi.Printf(msg2);
//	
//	*/
//
//	/*========================================End of NO-RECOIL DETECTION ========================================================*/
//	
//	/*=========================================== TEST AREA ===========================================================*/
//	
//
//	/*	CreateRandomText(7, cvar);
//		CreateRandomText(5, value);
//
//		gi.SendServerCommand( ent->client->ps.clientNum, "stufftext \"setu %s %s\"", cvar, value );
//		gi.getUserinfo(ent->client->ps.clientNum, userinfo, sizeof(userinfo) );
//  
//		strncpy( stuff, Info_ValueForKey(userinfo, cvar),256 );
//		if( !stricmp(stuff, "") )
//		{
//        
//			gi.Printf("Player kicked for tampered files\n");
//        
//		}
//	*/	
//	/*=========================================== End of TEST AREA ===========================================================*/
//
//	globals_backup.ClientThink( ent, ucmd, eyeInfo);
//	
//}

void  G_ClientCommand ( gentity_t *ent ){
	//gi.Argv(0) dmmessage Gi.Argv(1) 0 gi,Argv(2) blah gi.Args() 0 blah test
	//gi.Printf("\ngi.Argv(0) %s Gi.Argv(1) %s gi,Argv(2) %s gi.Args() %s\n", gi.Argv(0) ,gi.Argv(1) ,gi.Argv(2) ,gi.Args());
	char *cmd = gi.Argv(0);
	if(!strcmp(cmd,"dmmessage"))
	{
		if(strHasIP(gi.Args()))
		{
			gi.Printf("IP Typed Here !\n");
			return;
		}
	}
	else if (!strcmp(cmd, "keyp"))
	{
		char *idStr = gi.Argv(1);
		if (idStr)
		{
			long int id = strtol(idStr, NULL, 0);
			if (errno == ERANGE)
			{
				id = 0;
			}

			ScriptedEvent sev(SEV_KEYPRESS);

			if (sev.isRegistered())
			{
				sev.Trigger({ (Entity*)ent->entity, id});
			}
		}
	}
	else if (!strcmp(cmd, "scmd"))
	{
		int numArgs = gi.Argc();
		str cmdStr = gi.Argv(1);
		if (cmdStr)
		{
			str argsStr = "";
			for (size_t i = 2; i < numArgs; i++)
			{
				argsStr += gi.Argv(i);
				argsStr += i==numArgs-1 ? "" : " ";
			}

			ScriptedEvent sev(SEV_KEYPRESS);

			if (sev.isRegistered())
			{
				sev.Trigger({ (Entity*)ent->entity, cmdStr, argsStr });
			}
		}
	}
	globals_backup.ClientCommand(ent);
}
void G_ClientUserinfoChanged(gentity_t *ent, char *userInfo)
{
	//gi.Printf("\nname: %s\n" , Info_ValueForKey(userInfo,"name"));
	char * name;
	//char * usrInfo;
	name = Info_ValueForKey(userInfo,"name");
	if(strHasIP(name))
	{
		gi.Printf("G_ClientUserinfoChanged: Name Detected !\n");
		sizeof(Entity_t);
		//usrInfo = userInfo;
		//Info_SetValueForKey(usrInfo,"name","*** Blank Name ***");
		//globals_backup.ClientUserinfoChanged(ent,usrInfo);
		return;
	}
	globals_backup.ClientUserinfoChanged(ent,userInfo);
}
void __fastcall G_BeginIntermission(const char *map_name, INTTYPE_e transtype, bool shouldFade)
{

	ScriptedEvent sev(SEV_INTERMISSION);

	if (sev.isRegistered())
	{
		sev.Trigger({ INTERM_SCREEN });
	}
	G_BeginIntermission_original(map_name, transtype, shouldFade);
}

void initScriptHooks()
{
	G_BeginIntermission_original = reinterpret_cast<pG_BeginIntermission_spec>(BEGININTERMISSION_ADDR);
	bool success = Cmd_HookCommand("restart", &SV_MapRestart_original, &SV_MapRestart);
	if (!success)
	{
		gi.Printf("newpatchname INIT ERROR: failed to hook SV_MapRestart !\n");
	}
	success = Cmd_HookCommand("map", &SV_Map_original, &SV_Map);
	if (!success)
	{
		gi.Printf("newpatchname INIT ERROR: failed to hook SV_Map !\n");
	}
	success = Cmd_HookCommand("gamemap", &SV_GameMap_original, &SV_GameMap);
	if (!success)
	{
		gi.Printf("newpatchname INIT ERROR: failed to hook SV_GameMap !\n");
	}
	Event::Init();
	ClassDef::Init();
	Listener::Init();
	DirectorClass::Init();
	ScriptVariable::Init();

	Entity::Init();
	Player::Init();
	ScriptThread::Init();

	LONG ret = DetourTransactionBegin();
	ret = DetourUpdateThread(GetCurrentThread());
	ret = DetourAttach(&(PVOID&)(ClassDef::BuildResponseList_Orignal), (PVOID)(&BuildResponseListHook));
	ret = DetourTransactionCommit();

	ret = DetourTransactionBegin();
	ret = DetourUpdateThread(GetCurrentThread());
	ret = DetourAttach(&(PVOID&)(G_BeginIntermission_original), (PVOID)(&G_BeginIntermission));
	ret = DetourTransactionCommit();


}

void shutdownScriptHooks()
{
	Cmd_UnHookCommand(SV_GameMap_original, &SV_GameMap);
	Cmd_UnHookCommand(SV_Map_original, &SV_Map);
	Cmd_UnHookCommand(SV_MapRestart_original, &SV_MapRestart);

	Entity::Shutdown();
	Player::Shutdown();
	ScriptThread::Shutdown();

	LONG ret = DetourTransactionBegin();
	ret = DetourUpdateThread(GetCurrentThread());
	ret = DetourDetach(&(PVOID&)(ClassDef::BuildResponseList_Orignal), (PVOID)(&BuildResponseListHook));
	ret = DetourTransactionCommit();

	ret = DetourTransactionBegin();
	ret = DetourUpdateThread(GetCurrentThread());
	ret = DetourDetach(&(PVOID&)(G_BeginIntermission_original), (PVOID)(&G_BeginIntermission));
	ret = DetourTransactionCommit();
}
extern int classcount;
/*
============
G_InitGame

This will be called when the dll is first loaded, which
only happens when a new game is begun
============
*/
void G_InitGame( int startTime, int randomSeed )
{
	gi.Printf("==== Wrapper InitGame ==== \n");

	#ifndef _WIN32
	initsighandlers();  // init our custom signal handlers  (linux only)
	#endif

	initScriptHooks();

	globals_backup.Init( startTime, randomSeed );
	gi.Printf("Class count: %d", classcount);
	initIPBlocker();
	gi.Printf( "RyBack Wrapper inited \n");
	
}

/*
SHUTDOWN game
Needed to unload fgamededmohaa.so on mapchange!
*/
void	G_Shutdown (void)
{

	gi.Printf("==== Wrapper Shutdown ==== \n");
	shutDownIPBlocker();
	shutdownScriptHooks();

	globals_backup.Shutdown();

	#ifndef _WIN32
		// linux
		dlerror();
		if ( dlclose(hmod) )
			add_log(LOG_ERROR,"dlclose failed. reason: \"%s\"", dlerror() );
			
		resetsighandlers();  // reset to the original signal handlers
			
	#endif	
}


gameExport_t* __cdecl GetGameAPI( gameImport_t *import )
{
	/****** Linux: Load fgamededmohaa.so ******/
	#ifndef _WIN32

		dlerror();

		hmod = dlopen( "./fgamededmohaa.so", RTLD_LAZY|RTLD_LOCAL );
		if( !hmod ) 
		{
			printf( "dlopen failed. Reason: \"%s\"\n", dlerror() );
		}

		pGetGameAPI = ( pGetGameAPI_spec ) dlsym( hmod, "GetGameAPI" );

		if( !pGetGameAPI )	
			printf( "dlsym failed. reason: \"%s\"\n", dlerror() );

	#endif
	/* ************************************** */

	gi = *import;
//	import->Argv					= argv;
	globals							= pGetGameAPI( import );
	globals_backup					= *globals;

	if (!systemHMOD)
	{
		gi.Error(ERR_DROP, "newpatchaname: Failed to load memory management routines!");
		return NULL;
	}
	/*Game Exports*/
/*
	globals->AllowPaused			= G_AllowPaused;	
	globals->apiVersion				= G_apiVersion;	
	globals->ArchiveFloat			= G_ArchiveFloat;
	globals->ArchiveInteger			= G_ArchiveInteger;
	globals->ArchivePersistant		= G_ArchivePersistant;
	globals->ArchiveString			= G_ArchiveString;
	globals->ArchiveSvsTime			= G_ArchiveSvsTime;
	globals->BotBegin				= G_BotBegin;
	globals->BotThink				= G_BotThink;
	globals->Cleanup				= G_Cleanup;
*/
	globals->ClientBegin			= G_ClientBegin;

	
	globals->ClientCommand			= G_ClientCommand;
	globals->ClientConnect			= G_ClientConnect;

	globals->ClientDisconnect		= G_ClientDisconnect;
//	globals->ClientThink			= G_ClientThink;

	globals->ClientUserinfoChanged	= G_ClientUserinfoChanged;
/*	globals->ConsoleCommand			= G_ConsoleCommand;
	globals->DebugCircle			= G_DebugCircle;
	globals->errorMessage			= G_errorMessage;
	globals->gentities				= G_gentities;
	globals->gentitySize			= G_gentitySize;
*/
	globals->Init					= G_InitGame;
/*
	globals->LevelArchiveValid		= G_LevelArchiveValid;
	globals->maxEntities			= G_maxEntities;
	globals->numEntities			= G_numEntities;
	globals->Precache				= G_Precache;
	globals->PrepFrame				= G_PrepFrame;
	globals->profStruct				= G_profStruct;
	globals->ReadLevel				= G_ReadLevel;
	globals->RegisterSounds			= G_RegisterSounds;
	globals->Restart				= G_Restart;
	globals->RunFrame				= G_RunFrame;
	globals->ServerSpawned			= G_ServerSpawned;
	globals->SetFrameNumber			= G_SetFrameNumber;
	globals->SetMap					= G_SetMap;
	globals->SetTime				= G_SetTime;
*/
	globals->Shutdown				= G_Shutdown;
/*
	globals->SoundCallback			= G_SoundCallback;
	globals->SpawnEntities			= G_SpawnEntities;
	globals->TIKI_Orientation		= G_TIKI_Orientation;
	globals->WriteLevel				= G_WriteLevel;
*/
	
	return globals;
}

#ifdef _WIN32

BOOL WINAPI DllMain( HINSTANCE hModule, DWORD dwReason, PVOID lpReserved ) 
{
	char path[MAX_PATH];
	int i;
	DWORD dwSize;
//	DWORD dwAddr;
		
	if( dwReason == DLL_PROCESS_ATTACH ) 
    { 
//		DisableThreadLibraryCalls( hModule );
		
		dwSize = GetModuleFileName( hModule, path, 2048 );

		if(!dwSize || GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
		{
			path[0] = 0;
		}

		else
		{
			char* pszDir = &path[dwSize];

			while( *pszDir != '\\' ) 
			{
				pszDir--;

				if(!*pszDir) 
				{
					break;
				}
			}

			if( *pszDir == '\\' ) 
			{
				*(pszDir+1) = '\0';
			}

			else 
			{
				path[0] = 0;
			}
		}
			
		for( i=strlen(path); i>0; i-- ) 
		{
			if (path[i] == '\\') 
			{
				path[i+1] = 0;
				break;
			}
		}
		
		hmod = LoadLibrary("maintt\\dgamex86mohbt.dll");

		if(!hmod)
		{
			return FALSE;
		}
		pGetGameAPI = (pGetGameAPI_spec)GetProcAddress( hmod,"GetGameAPI" );

		systemHMOD = LoadLibrary("system86tt.dll");
		pMemoryMalloc = (pMemoryMalloc_spec)GetProcAddress(systemHMOD, "MemoryMalloc");
		pMemoryFree = (pMemoryFree_spec)GetProcAddress(systemHMOD, "MemoryFree");

		//initscriptfuncs();

		return TRUE; 
	}

	else if( dwReason == DLL_THREAD_DETACH ) 
    {
		return 0;
    }

	else if( dwReason == DLL_PROCESS_DETACH ) 
    {
		if (hmod)
		{
			FreeLibrary(hmod);
		}
		if (systemHMOD)
		{
			FreeLibrary(systemHMOD);
		}
		return 0;
    }



	return FALSE;
}

#endif

void *MemoryMalloc(int size)
{
	return pMemoryMalloc(size);
}

void MemoryFree(void *ptr)
{
	pMemoryFree(ptr);
}