// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "bg_saga.h"


typedef struct teamgame_s {
	float			last_flag_capture;
	int				last_capture_team;
	flagStatus_t	redStatus;	// CTF
	flagStatus_t	blueStatus;	// CTF
	flagStatus_t	flagStatus;	// One Flag CTF
	int				redTakenTime;
	int				blueTakenTime;
	int				yellowTakenTime;
} teamgame_t;

teamgame_t teamgame;

void Team_SetFlagStatus( int team, flagStatus_t status );

void Team_InitGame( void ) {
	memset(&teamgame, 0, sizeof teamgame);

	switch( g_gametype.integer ) {
	case GT_CTF:
	case GT_CTY:
		teamgame.redStatus = teamgame.blueStatus = -1; // Invalid to force update
		Team_SetFlagStatus( TEAM_RED, FLAG_ATBASE );
		Team_SetFlagStatus( TEAM_BLUE, FLAG_ATBASE );
		if (level.CTF3ModeActive) {
			teamgame.flagStatus = -1;
			Team_SetFlagStatus( TEAM_FREE, FLAG_ATBASE );
		}
		break;
	default:
		break;
	}
}

int OtherTeam(int team) {
	if (team==TEAM_RED)
		return TEAM_BLUE;
	else if (team==TEAM_BLUE)
		return TEAM_RED;
	return team;
}

const char *TeamName(int team)  {
	if (team==TEAM_RED)
		return "RED";
	else if (team==TEAM_BLUE)
		return "BLUE";
	else if (team==TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

const char *OtherTeamName(int team) {
	if (team==TEAM_RED)
		return "BLUE";
	else if (team==TEAM_BLUE)
		return "RED";
	else if (team==TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

const char *TeamColorString(int team) {
	if (team==TEAM_RED)
		return S_COLOR_RED;
	else if (team==TEAM_BLUE)
		return S_COLOR_BLUE;
	else if (team==TEAM_SPECTATOR)
		return S_COLOR_YELLOW;
	return S_COLOR_WHITE;
}

// NULL for everyone
/*
void QDECL PrintMsg( gentity_t *ent, const char *fmt, ... ) {
	char		msg[1024];
	va_list		argptr;
	char		*p;

	va_start (argptr,fmt);
	if (vsprintf (msg, fmt, argptr) > sizeof(msg)) {
		G_Error ( "PrintMsg overrun" );
	}
	va_end (argptr);

	// double quotes are bad
	while ((p = strchr(msg, '"')) != NULL)
		*p = '\'';

	trap_SendServerCommand ( ( (ent == NULL) ? -1 : ent-g_entities ), va("print \"%s\"", msg ));
}
*/
//Printing messages to players via this method is no longer done, StripEd stuff is client only.


//plIndex used to print pl->client->pers.netname
//teamIndex used to print team name
void PrintCTFMessage(int plIndex, int teamIndex, int ctfMessage)
{
	gentity_t *te;

	if (plIndex == -1)
	{
		plIndex = MAX_CLIENTS+1;
	}
	if (teamIndex == -1)
	{
		teamIndex = 50;
	}

	te = G_TempEntity(vec3_origin, EV_CTFMESSAGE);
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = ctfMessage;
	te->s.trickedentindex = plIndex;

	if (ctfMessage == CTFMESSAGE_PLAYER_CAPTURED_FLAG && plIndex < MAX_CLIENTS && plIndex >= 0) {
		//add flag hold time
		const int holdtime = level.time - level.clients[plIndex].pers.teamState.flagsince;

		te->s.genericenemyindex = holdtime;

		// pers_gauntlet_frag_count = total flag hold time
		g_entities[plIndex].client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT] += holdtime;
		g_entities[plIndex].client->pers.teamState.flaghold += holdtime;
		
		level.teamstats[ BinaryTeam(g_entities + plIndex) ].flaghold += holdtime;
	}

	if (ctfMessage == CTFMESSAGE_PLAYER_CAPTURED_FLAG)
	{
		switch (teamIndex) {
			case TEAM_RED:
				te->s.trickedentindex2 = TEAM_BLUE;
				break;
			case TEAM_FREE:
				if (level.CTF3ModeActive) { //modified behavior: teamIndex should be TEAM_FREE when capturing the yellow flag
					te->s.trickedentindex2 = TEAM_FREE;
					break;
				}
			case TEAM_BLUE:
			default:
				te->s.trickedentindex2 = TEAM_RED;
				break;
		}
	}
	else
	{
		te->s.trickedentindex2 = teamIndex;
	}
}

/*
==============
AddTeamScore

 used for gametype > GT_TEAM
 for gametype GT_TEAM the level.teamScores is updated in AddScore in g_combat.c
==============
*/
void AddTeamScore(vec3_t origin, int team, int score) {
	gentity_t	*te;

	te = G_TempEntity(origin, EV_GLOBAL_TEAM_SOUND );
	te->r.svFlags |= SVF_BROADCAST;

	if ( team == TEAM_RED ) {
		if ( level.teamScores[ TEAM_RED ] + score == level.teamScores[ TEAM_BLUE ] ) {
			//teams are tied sound
			te->s.eventParm = GTS_TEAMS_ARE_TIED;
		}
		else if ( level.teamScores[ TEAM_RED ] <= level.teamScores[ TEAM_BLUE ] &&
					level.teamScores[ TEAM_RED ] + score > level.teamScores[ TEAM_BLUE ]) {
			// red took the lead sound
			te->s.eventParm = GTS_REDTEAM_TOOK_LEAD;
		}
		else {
			// red scored sound
			te->s.eventParm = GTS_REDTEAM_SCORED;
		}
	}
	else {
		if ( level.teamScores[ TEAM_BLUE ] + score == level.teamScores[ TEAM_RED ] ) {
			//teams are tied sound
			te->s.eventParm = GTS_TEAMS_ARE_TIED;
		}
		else if ( level.teamScores[ TEAM_BLUE ] <= level.teamScores[ TEAM_RED ] &&
					level.teamScores[ TEAM_BLUE ] + score > level.teamScores[ TEAM_RED ]) {
			// blue took the lead sound
			te->s.eventParm = GTS_BLUETEAM_TOOK_LEAD;
		}
		else {
			// blue scored sound
			te->s.eventParm = GTS_BLUETEAM_SCORED;
		}
	}
	level.teamScores[ team ] += score;
}

/*
==============
OnSameTeam
==============
*/
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 ) {
	if ( !ent1->client || !ent2->client ) {
		return qfalse;
	}

	if ( g_gametype.integer < GT_TEAM ) {
		return qfalse;
	}

	if ( ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam ) {
		return qtrue;
	}

	return qfalse;
}


static char ctfFlagStatusRemap[] = { '0', '1', '*', '*', '2' };

void Team_SetFlagStatus( int team, flagStatus_t status ) {
	qboolean modified = qfalse;

	switch( team ) {
	case TEAM_RED:	// CTF
		if( teamgame.redStatus != status ) {
			teamgame.redStatus = status;
			modified = qtrue;
		}
		break;

	case TEAM_BLUE:	// CTF
		if( teamgame.blueStatus != status ) {
			teamgame.blueStatus = status;
			modified = qtrue;
		}
		break;

	case TEAM_FREE:	// One Flag CTF
		if( teamgame.flagStatus != status ) {
			teamgame.flagStatus = status;
			modified = qtrue;
		}
		break;
	}

	if( modified ) {
		char st[4];

		if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY ) {
			st[0] = ctfFlagStatusRemap[teamgame.redStatus];
			st[1] = ctfFlagStatusRemap[teamgame.blueStatus];
			st[2] = level.CTF3ModeActive ? ctfFlagStatusRemap[teamgame.flagStatus] : 0;
		}

		trap_SetConfigstring( CS_FLAGSTATUS, st );
	}
}

void Team_CheckDroppedItem( gentity_t *dropped ) {
	switch (dropped->item->giTag)
	{
		case PW_REDFLAG:
			Team_SetFlagStatus( TEAM_RED, FLAG_DROPPED );
			break;
		case PW_BLUEFLAG:
			Team_SetFlagStatus( TEAM_BLUE, FLAG_DROPPED );
			break;
		case PW_NEUTRALFLAG:
			Team_SetFlagStatus( TEAM_FREE, FLAG_DROPPED );
			break;
		default:
			break;
	}
}

/*
================
Team_ForceGesture
================
*/
void Team_ForceGesture(int team) {
	int i;
	gentity_t *ent;

	for (i = 0; i < MAX_CLIENTS; i++) {
		ent = &g_entities[i];
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;
		if (ent->client->sess.sessionTeam != team)
			continue;
		//
		ent->flags |= FL_FORCE_GESTURE;
	}
}

/*
================
Team_FragBonuses

Calculate the bonuses for flag defense, flag carrier defense, etc.
Note that bonuses are not cumulative.  You get one, they are in importance
order.
================
*/

qboolean IsCapper(gentity_t *ent) {
	if (ent && ent->client && ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
		(ent->client->ps.powerups[PW_REDFLAG] || ent->client->ps.powerups[PW_BLUEFLAG] || ent->client->ps.powerups[PW_NEUTRALFLAG]))
			return qtrue;

	return qfalse;
}

int BinaryTeam( gentity_t *ent ) { //should probably be renamed now...
	switch (ent->client->sess.sessionTeam) {
		case TEAM_RED:
			return G_REDTEAM;
			break;
		case TEAM_FREE:
			if (level.CTF3ModeActive) {
				return G_YELLOWTEAM;
				break;
			}
		default:
		case TEAM_BLUE:
			return G_BLUETEAM;
			break;
	}
}
void Team_FragBonuses(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker)
{
	int i;
	gentity_t *ent;
	int flag_pw, enemy_flag_pw;
	int otherteam;
	int tokens;
	gentity_t *flag, *carrier = NULL;
	char *c;
	vec3_t v1, v2;
	int team;
	qboolean fraggedCarrier = qfalse;

	// no bonus for fragging yourself or team mates
	if (!targ->client || !attacker->client || targ == attacker || OnSameTeam(targ, attacker))
		return;

	team = targ->client->sess.sessionTeam;
	otherteam = OtherTeam(targ->client->sess.sessionTeam);
	if (otherteam < 0)
		return; // whoever died isn't on a team

	// same team, if the flag at base, check to he has the enemy flag
	switch (team)
	{
		case TEAM_RED:
			flag_pw = PW_REDFLAG;
			if (targ->client->ps.powerups[PW_BLUEFLAG] || targ->client->ps.powerups[PW_NEUTRALFLAG])
				fraggedCarrier = qtrue;
			break;
		case TEAM_BLUE:
			flag_pw = PW_BLUEFLAG;
			if (targ->client->ps.powerups[PW_REDFLAG] || targ->client->ps.powerups[PW_NEUTRALFLAG])
				fraggedCarrier = qtrue;
			break;
		case TEAM_FREE:
			flag_pw = PW_NEUTRALFLAG;
			if (targ->client->ps.powerups[PW_REDFLAG] || targ->client->ps.powerups[PW_BLUEFLAG])
				fraggedCarrier = qtrue;
			break;
		default:
			break;
	}

	// did the attacker frag the flag carrier?
	tokens = 0;
	if (fraggedCarrier) {

		attacker->client->pers.teamState.lastfraggedcarrier = level.time;
		AddScore(attacker, targ->r.currentOrigin, CTF_FRAG_CARRIER_BONUS);

		attacker->client->pers.teamState.fragcarrier++;

		level.teamstats[ BinaryTeam(attacker) ].rets++;

		PrintCTFMessage(attacker->s.number, team, CTFMESSAGE_FRAGGED_FLAG_CARRIER);

		//impressive count is now equal to number of returning frags
		attacker->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
		// add the sprite over the player's head
		attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
		attacker->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		// the target had the flag, clear the hurt carrier
		// field on the other team
		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;
			if (ent->inuse && ent->client->sess.sessionTeam == otherteam)
				ent->client->pers.teamState.lasthurtcarrier = 0;
		}
		return;
	}

	if (targ->client->pers.teamState.lasthurtcarrier &&
		level.time - targ->client->pers.teamState.lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT &&
		!attacker->client->ps.powerups[flag_pw]) {
		// attacker is on the same team as the flag carrier and
		// fragged a guy who hurt our flag carrier
		AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_DANGER_PROTECT_BONUS);

		attacker->client->pers.teamState.carrierdefense++;
		targ->client->pers.teamState.lasthurtcarrier = 0;

		level.teamstats[ BinaryTeam(attacker) ].defense++;

		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;

		team = attacker->client->sess.sessionTeam;
		// add the sprite over the player's head
		attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
		attacker->client->ps.eFlags |= EF_AWARD_DEFEND;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	if (targ->client->pers.teamState.lasthurtcarrier &&
		level.time - targ->client->pers.teamState.lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT) {
		// attacker is on the same team as the skull carrier and
		AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_DANGER_PROTECT_BONUS);

		attacker->client->pers.teamState.carrierdefense++;
		targ->client->pers.teamState.lasthurtcarrier = 0;

		level.teamstats[ BinaryTeam(attacker) ].defense++;


		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;

		team = attacker->client->sess.sessionTeam;
		// add the sprite over the player's head
		attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
		attacker->client->ps.eFlags |= EF_AWARD_DEFEND;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	// flag and flag carrier area defense bonuses

	// we have to find the flag and carrier entities

	// find the flag
	switch (attacker->client->sess.sessionTeam) {
	case TEAM_RED:
		c = "team_CTF_redflag";
		break;
	case TEAM_BLUE:
		c = "team_CTF_blueflag";
		break;
	case TEAM_FREE:
		if (level.CTF3ModeActive) {
			c = "team_CTF_neutralflag";
			break;
		}
	default:
		return;
	}
	// find attacker's team's flag carrier
	for (i = 0; i < g_maxclients.integer; i++) {
		carrier = g_entities + i;
		if (carrier->inuse && carrier->client->ps.powerups[flag_pw])
			break;
		carrier = NULL;
	}
	flag = NULL;
	while ((flag = G_Find (flag, FOFS(classname), c)) != NULL) {
		if (!(flag->flags & FL_DROPPED_ITEM))
			break;
	}

	if (!flag)
		return; // can't find attacker's flag

	// ok we have the attackers flag and a pointer to the carrier

	// check to see if we are defending the base's flag
	VectorSubtract(targ->r.currentOrigin, flag->r.currentOrigin, v1);
	VectorSubtract(attacker->r.currentOrigin, flag->r.currentOrigin, v2);

	if ( ( ( VectorLength(v1) < CTF_TARGET_PROTECT_RADIUS &&
		trap_InPVS(flag->r.currentOrigin, targ->r.currentOrigin ) ) ||
		( VectorLength(v2) < CTF_TARGET_PROTECT_RADIUS &&
		trap_InPVS(flag->r.currentOrigin, attacker->r.currentOrigin ) ) ) &&
		attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam) {

		// we defended the base flag
		AddScore(attacker, targ->r.currentOrigin, CTF_FLAG_DEFENSE_BONUS);
		attacker->client->pers.teamState.basedefense++;

		level.teamstats[ BinaryTeam(attacker) ].defense++;


		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
		// add the sprite over the player's head
		attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
		attacker->client->ps.eFlags |= EF_AWARD_DEFEND;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	if (carrier && carrier != attacker) {
		VectorSubtract(targ->r.currentOrigin, carrier->r.currentOrigin, v1);
		VectorSubtract(attacker->r.currentOrigin, carrier->r.currentOrigin, v1);

		if ( ( ( VectorLength(v1) < CTF_ATTACKER_PROTECT_RADIUS &&
			trap_InPVS(carrier->r.currentOrigin, targ->r.currentOrigin ) ) ||
			( VectorLength(v2) < CTF_ATTACKER_PROTECT_RADIUS &&
				trap_InPVS(carrier->r.currentOrigin, attacker->r.currentOrigin ) ) ) &&
			attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam) {

			AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_PROTECT_BONUS);
			attacker->client->pers.teamState.carrierdefense++;

			level.teamstats[ BinaryTeam(attacker) ].defense++;


			attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
			// add the sprite over the player's head
			attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
			attacker->client->ps.eFlags |= EF_AWARD_DEFEND;
			attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			return;
		}
	}
}

/*
================
Team_CheckHurtCarrier

Check to see if attacker hurt the flag carrier.  Needed when handing out bonuses for assistance to flag
carrier defense.
================
*/
void Team_CheckHurtCarrier(gentity_t *targ, gentity_t *attacker)
{
	int flag_pw;

	if (!targ->client || !attacker->client)
		return;

	switch (targ->client->sess.sessionTeam) {
		case TEAM_RED:
			if (targ->client->ps.powerups[PW_BLUEFLAG])
				flag_pw = PW_BLUEFLAG;
			if (targ->client->ps.powerups[PW_NEUTRALFLAG])
				flag_pw = PW_NEUTRALFLAG;
			break;
		case TEAM_BLUE:
			if (targ->client->ps.powerups[PW_REDFLAG])
				flag_pw = PW_REDFLAG;
			if (targ->client->ps.powerups[PW_NEUTRALFLAG])
				flag_pw = PW_NEUTRALFLAG;
			break;
		case TEAM_FREE:
			if (targ->client->ps.powerups[PW_REDFLAG])
				flag_pw = PW_REDFLAG;
			if (targ->client->ps.powerups[PW_BLUEFLAG])
				flag_pw = PW_BLUEFLAG;
			break;
		default:
			break;
	}

	// flags
	if (targ->client->ps.powerups[flag_pw] &&
		targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
		attacker->client->pers.teamState.lasthurtcarrier = level.time;

	// skulls
	if (targ->client->ps.generic1 &&
		targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
		attacker->client->pers.teamState.lasthurtcarrier = level.time;
}


gentity_t *Team_ResetFlag( int team ) {
	char *c;
	gentity_t *ent, *rent = NULL;

	switch (team) {
	case TEAM_RED:
		c = "team_CTF_redflag";
		break;
	case TEAM_BLUE:
		c = "team_CTF_blueflag";
		break;
	case TEAM_FREE:
		c = "team_CTF_neutralflag";
		break;
	default:
		return NULL;
	}

	ent = NULL;
	while ((ent = G_Find (ent, FOFS(classname), c)) != NULL) {
		if (ent->flags & FL_DROPPED_ITEM)
			G_FreeEntity(ent);
		else {
			rent = ent;
			RespawnItem(ent);
		}
	}

	Team_SetFlagStatus( team, FLAG_ATBASE );

	return rent;
}

void Team_ResetFlags( void ) {
	if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY ) {
		Team_ResetFlag( TEAM_RED );
		Team_ResetFlag( TEAM_BLUE );
		//if (level.CTF3ModeActive)
			Team_ResetFlag( TEAM_FREE );
	}
}

void Team_ReturnFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_ReturnFlagSound\n");
		return;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_RED_RETURN;
	}
	else {
		te->s.eventParm = GTS_BLUE_RETURN;
	}
	te->r.svFlags |= SVF_BROADCAST;
}

void Team_TakeFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_TakeFlagSound\n");
		return;
	}

	// only play sound when the flag was at the base
	// or not picked up the last 10 seconds
	switch(team) {
		case TEAM_RED:
			if( teamgame.blueStatus != FLAG_ATBASE ) {
				if (teamgame.blueTakenTime > level.time - 10000)
					return;
			}
			teamgame.blueTakenTime = level.time;
			break;

		case TEAM_BLUE:	// CTF
			if( teamgame.redStatus != FLAG_ATBASE ) {
				if (teamgame.redTakenTime > level.time - 10000)
					return;
			}
			teamgame.redTakenTime = level.time;
			break;

		case TEAM_FREE:	// CTF - fixme...
			if( teamgame.flagStatus != FLAG_ATBASE ) {
				if (teamgame.yellowTakenTime > level.time - 10000)
					return;
			}
			teamgame.yellowTakenTime = level.time;
			break;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_RED_TAKEN;
	}
	else {
		te->s.eventParm = GTS_BLUE_TAKEN;
	}
	te->r.svFlags |= SVF_BROADCAST;
}

void Team_CaptureFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_CaptureFlagSound\n");
		return;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_BLUE_CAPTURE;
	}
	else {
		te->s.eventParm = GTS_RED_CAPTURE;
	}
	te->r.svFlags |= SVF_BROADCAST;
}

void Team_ReturnFlag( int team ) {
	Team_ReturnFlagSound(Team_ResetFlag(team), team);
	if( team == TEAM_FREE ) {
		//PrintMsg(NULL, "The flag has returned!\n" );
	}
	else { //flag should always have team in normal CTF
		//PrintMsg(NULL, "The %s flag has returned!\n", TeamName(team));
		PrintCTFMessage(-1, team, CTFMESSAGE_FLAG_RETURNED);
	}
}

void Team_FreeEntity( gentity_t *ent ) {
	if( ent->item->giTag == PW_REDFLAG ) {
		Team_ReturnFlag( TEAM_RED );
	}
	else if( ent->item->giTag == PW_BLUEFLAG ) {
		Team_ReturnFlag( TEAM_BLUE );
	}
	else if( ent->item->giTag == PW_NEUTRALFLAG ) {
		Team_ReturnFlag( TEAM_FREE );
	}
}

/*
==============
Team_DroppedFlagThink

Automatically set in Launch_Item if the item is one of the flags

Flags are unique in that if they are dropped, the base flag must be respawned when they time out
==============
*/
void Team_DroppedFlagThink(gentity_t *ent) {
	int		team = TEAM_FREE;

	if ( ent->item->giTag == PW_REDFLAG ) {
		team = TEAM_RED;
	}
	else if ( ent->item->giTag == PW_BLUEFLAG ) {
		team = TEAM_BLUE;
	}
	else if ( ent->item->giTag == PW_NEUTRALFLAG ) {
		team = TEAM_FREE;
	}

	Team_ReturnFlagSound( Team_ResetFlag( team ), team );
	// Reset Flag will delete this entity
}


/*
==============
Team_DroppedFlagThink
==============
*/

//OPENJK:
// This is to account for situations when there are more players standing
// on flag stand and then flag gets returned. This leaded to bit random flag
// grabs/captures, improved version takes distance to the center of flag stand
// into consideration (closer player will get/capture the flag).
static vec3_t	minFlagRange = { 50, 36, 36 };
static vec3_t	maxFlagRange = { 44, 36, 36 };
int Team_TouchEnemyFlag( gentity_t *ent, gentity_t *other, int team );

int Team_TouchOurFlag( gentity_t *ent, gentity_t *other, int team ) {
	int			i;
	gentity_t	*player;
	gclient_t	*cl = other->client;
	int			enemy_flag;
	int			teamIndex = team;
	qboolean	hasEnemyFlag = qfalse;

	switch (cl->sess.sessionTeam)
	{
		case TEAM_RED:
			if (cl->ps.powerups[PW_BLUEFLAG])
			{
				enemy_flag = PW_BLUEFLAG;
				hasEnemyFlag = qtrue;
			}
			else if (cl->ps.powerups[PW_NEUTRALFLAG]) {
				enemy_flag = PW_NEUTRALFLAG;
				hasEnemyFlag = qtrue;
			}
			break;
		case TEAM_BLUE:
			if (cl->ps.powerups[PW_REDFLAG]) {
				enemy_flag = PW_REDFLAG;
				hasEnemyFlag = qtrue;
			}
			else if (cl->ps.powerups[PW_NEUTRALFLAG]) {
				enemy_flag = PW_NEUTRALFLAG;
				hasEnemyFlag = qtrue;
			}
			break;
		case TEAM_FREE:
			if (cl->ps.powerups[PW_REDFLAG]) {
				enemy_flag = PW_REDFLAG;
				hasEnemyFlag = qtrue;
			}
			else if (cl->ps.powerups[PW_BLUEFLAG]) {
				enemy_flag = PW_BLUEFLAG;
				hasEnemyFlag = qtrue;
			}
			break;
		default:
			break;
	}

	if ( ent->flags & FL_DROPPED_ITEM ) {
		// hey, its not home.  return it by teleporting it back
		PrintCTFMessage(other->s.number, team, CTFMESSAGE_PLAYER_RETURNED_FLAG);

		AddScore(other, ent->r.currentOrigin, CTF_RECOVERY_BONUS);
		other->client->pers.teamState.flagrecovery++;

		other->client->pers.teamState.lastreturnedflag = level.time;
		//ResetFlag will remove this entity!  We must return zero
		Team_ReturnFlagSound(Team_ResetFlag(team), team);
		return 0;
	}

	// the flag is at home base.  if the player has the enemy
	// flag, he's just won!
	if (!hasEnemyFlag)
		return 0; // We don't have the flag

	// fix from openJK: captures after timelimit hit could
	// cause game ending with tied score
	if (level.intermissionQueued)
		return 0;

	if (g_fairflag.integer) {
		//Openjk
		int			num, j, enemyTeam;
		vec3_t		mins, maxs;
		int			touch[MAX_GENTITIES];
		gentity_t*	enemy;
		float		enemyDist, dist;

		//OPENJK
		// check for enemy closer to grab the flag
		VectorSubtract( ent->s.pos.trBase, minFlagRange, mins );
		VectorAdd( ent->s.pos.trBase, maxFlagRange, maxs );

		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

		dist = Distance( ent->s.pos.trBase, other->client->ps.origin );

		if (other->client->sess.sessionTeam == TEAM_RED)
			enemyTeam = TEAM_BLUE;
		else
			enemyTeam = TEAM_RED;

		for (j = 0; j < num; j++) {
			enemy = (g_entities + touch[j]);

			if (!enemy || !enemy->inuse || !enemy->client)
				continue;

			if (enemy->client->pers.connected != CON_CONNECTED)
				continue;

			//ignore specs
			if (enemy->client->sess.sessionTeam == TEAM_SPECTATOR)
				continue;

			//check if its alive
			if (enemy->health < 1)
				continue;		// dead people can't pickup

			//check if this is enemy
			if (enemy->client->sess.sessionTeam == other->client->sess.sessionTeam)
				continue;

			//check if enemy is closer to our flag than us
			enemyDist = Distance(ent->s.pos.trBase, enemy->client->ps.origin);
			if (enemyDist < dist) {
				// possible recursion is hidden in this, but
				// infinite recursion wont happen, because we cant
				// have a < b and b < a at the same time
				return Team_TouchEnemyFlag( ent, enemy, team );
			}
		}
	}

	if (!level.CTF3ModeActive) {
		PrintCTFMessage(other->s.number, team, CTFMESSAGE_PLAYER_CAPTURED_FLAG);
	}
	else { //hack team index...
		if (enemy_flag == PW_NEUTRALFLAG) {
			teamIndex = TEAM_FREE;
		}
		else if (teamIndex == TEAM_FREE) {
			teamIndex = cl->ps.powerups[PW_REDFLAG] ? TEAM_BLUE : TEAM_RED;
		}

		PrintCTFMessage(other->s.number, teamIndex, CTFMESSAGE_PLAYER_CAPTURED_FLAG);
	}

	cl->ps.powerups[enemy_flag] = 0;

	teamgame.last_flag_capture = level.time;
	teamgame.last_capture_team = team;

	// Increase the team's score
	AddTeamScore(ent->s.pos.trBase, other->client->sess.sessionTeam, 1);

	if (level.CTF3ModeActive) { //send message with score to clients - should update and only do this for clients w/o plugin
		char s[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(s, sizeof(s), "%sRed: %s%i%s Blue: %s%i%s Yellow: %s%i%s",
			S_COLOR_WHITE, S_COLOR_RED, level.teamScores[TEAM_RED], S_COLOR_WHITE, S_COLOR_BLUE, level.teamScores[TEAM_BLUE], S_COLOR_WHITE, S_COLOR_YELLOW, level.teamScores[TEAM_FREE], S_COLOR_WHITE);
		trap_SendServerCommand(-1, va("chat \"Scores: %s\"", s));
		trap_SendServerCommand(-1, va("cp \"%s\"", s));
	}

	other->client->pers.teamState.captures++;
	// add the sprite over the player's head
	other->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
	other->client->ps.eFlags |= EF_AWARD_CAP;
	other->client->rewardTime = level.time + REWARD_SPRITE_TIME;
	other->client->ps.persistant[PERS_CAPTURES]++;


	// other gets another 10 frag bonus
	AddScore(other, ent->r.currentOrigin, CTF_CAPTURE_BONUS);

	Team_CaptureFlagSound( ent, team );

	// Ok, let's do the player loop, hand out the bonuses
	for (i = 0; i < g_maxclients.integer; i++) {
		player = &g_entities[i];
		if (!player->inuse)
			continue;

		if (player->client->sess.sessionTeam !=
			cl->sess.sessionTeam) {
			player->client->pers.teamState.lasthurtcarrier = -5;
		} else if (player->client->sess.sessionTeam == cl->sess.sessionTeam) {
			if (player != other)
				AddScore(player, ent->r.currentOrigin, CTF_TEAM_BONUS);
			// award extra points for capture assists
			if (player->client->pers.teamState.lastreturnedflag +
				CTF_RETURN_FLAG_ASSIST_TIMEOUT > level.time) {
				AddScore (player, ent->r.currentOrigin, CTF_RETURN_FLAG_ASSIST_BONUS);
				player->client->pers.teamState.assists++;

				level.teamstats[ BinaryTeam(player) ].assist++;

				player->client->ps.persistant[PERS_ASSIST_COUNT]++;

				// add the sprite over the player's head
				player->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
				player->client->ps.eFlags |= EF_AWARD_ASSIST;
				player->client->rewardTime = level.time + REWARD_SPRITE_TIME;

			} else if (player->client->pers.teamState.lastfraggedcarrier +
				CTF_FRAG_CARRIER_ASSIST_TIMEOUT > level.time) {
				AddScore(player, ent->r.currentOrigin, CTF_FRAG_CARRIER_ASSIST_BONUS);
				player->client->pers.teamState.assists++;
				player->client->ps.persistant[PERS_ASSIST_COUNT]++;

				level.teamstats[ BinaryTeam(player) ].assist++;

				// add the sprite over the player's head
				player->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
				player->client->ps.eFlags |= EF_AWARD_ASSIST;
				player->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}
		}
	}

	if (level.CTF3ModeActive) { //if we have 3 teams, only reset the flag that was just captured - why did the original code reset all flags anyway?
		switch (enemy_flag)
		{
			case PW_REDFLAG:
				Team_ResetFlag(TEAM_RED);
				break;
			case PW_BLUEFLAG:
				Team_ResetFlag(TEAM_BLUE);
				break;
			case PW_NEUTRALFLAG:
				Team_ResetFlag(TEAM_FREE);
				break;
			default:
				break;
		}
	}
	else {
		Team_ResetFlags();
	}

	CalculateRanks();

	return 0; // Do not respawn this automatically
}

int Team_TouchEnemyFlag( gentity_t *ent, gentity_t *other, int team ) {
	gclient_t *cl = other->client;

	if (g_fairflag.integer) {
		vec3_t		mins, maxs;
		int			num, j, ourFlag;
		int			touch[MAX_GENTITIES];
		gentity_t*	enemy;
		float		enemyDist, dist;

		VectorSubtract( ent->s.pos.trBase, minFlagRange, mins );
		VectorAdd( ent->s.pos.trBase, maxFlagRange, maxs );

		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

		dist = Distance(ent->s.pos.trBase, other->client->ps.origin);

		switch (other->client->sess.sessionTeam)
		{
			case TEAM_RED:
				ourFlag = PW_REDFLAG;
				break;
			case TEAM_FREE:
				if (level.CTF3ModeActive) {
					ourFlag = PW_NEUTRALFLAG;
					break;
				}
			default:
			case TEAM_BLUE:
				ourFlag = PW_BLUEFLAG;
				break;
		}

		for (j = 0; j < num; ++j) {
			enemy = (g_entities + touch[j]);

			if (!enemy || !enemy->inuse || !enemy->client)
				continue;

			//ignore specs
			if (enemy->client->sess.sessionTeam == TEAM_SPECTATOR)
				continue;

			//check if its alive
			if (enemy->health < 1)
				continue;		// dead people can't pick up items

			//lets check if he has our flag (he must have our flag)
			if (!enemy->client->ps.powerups[ourFlag])
				continue;

			//check if enemy is closer to our flag than us
			enemyDist = Distance(ent->s.pos.trBase,enemy->client->ps.origin);
			if (enemyDist < dist) {
				// possible recursion is hidden in this, but
				// infinite recursion wont happen, because we cant
				// have a < b and b < a at the same time
				return Team_TouchOurFlag( ent, enemy, team );
			}
		}
	}

	PrintCTFMessage(other->s.number, team, CTFMESSAGE_PLAYER_GOT_FLAG);

	level.teamstats[ BinaryTeam(other) ].flaggrabs++;

	// excellent count = flag steal count
	if (!level.intermissionQueued) {
		other->client->ps.persistant[PERS_EXCELLENT_COUNT]++;
		other->client->pers.teamState.flagsteal++;
	}

	switch (team) {
		case TEAM_RED:
			cl->ps.powerups[PW_REDFLAG] = INT_MAX; // flags never expire
			break;
		case TEAM_BLUE:
			cl->ps.powerups[PW_BLUEFLAG] = INT_MAX; // flags never expire
			break;
		case TEAM_FREE:
			cl->ps.powerups[PW_NEUTRALFLAG] = INT_MAX;
			break;
		default:
			break;
	}

	Team_SetFlagStatus( team, FLAG_TAKEN );

	AddScore(other, ent->r.currentOrigin, CTF_FLAG_BONUS);
	cl->pers.teamState.flagsince = level.time;
	Team_TakeFlagSound( ent, team );

	return -1; // Do not respawn this automatically, but do delete it if it was FL_DROPPED
}

int Pickup_Team( gentity_t *ent, gentity_t *other ) {
	int team;
	gclient_t *cl = other->client;

	// figure out what team this flag is
	if( strcmp(ent->classname, "team_CTF_redflag") == 0 ) {
		team = TEAM_RED;
	}
	else if( strcmp(ent->classname, "team_CTF_blueflag") == 0 ) {
		team = TEAM_BLUE;
	}
	else if( strcmp(ent->classname, "team_CTF_neutralflag") == 0  ) {
		team = TEAM_FREE;
	}
	else {
//		PrintMsg ( other, "Don't know what team the flag is on.\n");
		return 0;
	}
	// GT_CTF
	if (team == cl->sess.sessionTeam) {
		return Team_TouchOurFlag( ent, other, team );
	}
	return Team_TouchEnemyFlag( ent, other, team );
}

/*
===========
Team_GetLocation

Report a location for the player. Uses placed nearby target_location entities
============
*/
gentity_t *Team_GetLocation(gentity_t *ent)
{
	gentity_t		*eloc, *best;
	float			bestlen, len;
	vec3_t			origin;

	best = NULL;
	bestlen = 3*8192.0*8192.0;

	VectorCopy( ent->r.currentOrigin, origin );

	for (eloc = level.locationHead; eloc; eloc = eloc->nextTrain) {
		len = ( origin[0] - eloc->r.currentOrigin[0] ) * ( origin[0] - eloc->r.currentOrigin[0] )
			+ ( origin[1] - eloc->r.currentOrigin[1] ) * ( origin[1] - eloc->r.currentOrigin[1] )
			+ ( origin[2] - eloc->r.currentOrigin[2] ) * ( origin[2] - eloc->r.currentOrigin[2] );

		if ( len > bestlen ) {
			continue;
		}

		if ( !trap_InPVS( origin, eloc->r.currentOrigin ) ) {
			continue;
		}

		bestlen = len;
		best = eloc;
	}

	return best;
}


/*
===========
Team_GetLocation

Report a location for the player. Uses placed nearby target_location entities
============
*/
qboolean Team_GetLocationMsg(gentity_t *ent, char *loc, int loclen)
{
	gentity_t *best;

	best = Team_GetLocation( ent );

	if (!best)
		return qfalse;

	if (best->count) {
		if (best->count < 0)
			best->count = 0;
		if (best->count > 7)
			best->count = 7;
		Com_sprintf(loc, loclen, "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, best->count + '0', best->message );
	} else
		Com_sprintf(loc, loclen, "%s", best->message);

	return qtrue;
}


/*---------------------------------------------------------------------------*/

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define	MAX_TEAM_SPAWN_POINTS	32
gentity_t *SelectRandomTeamSpawnPoint( int teamstate, team_t team ) {
	gentity_t	*spot;
	int			count;
	int			selection;
	gentity_t	*spots[MAX_TEAM_SPAWN_POINTS];
	char		*classname;

	if (g_gametype.integer == GT_SAGA)
	{
		if (team == SAGATEAM_IMPERIAL)
		{
			classname = "info_player_imperial";
		}
		else
		{
			classname = "info_player_rebel";
		}
	}
	else
	{
		switch (team)
		{
			case TEAM_RED:
				classname = teamstate == TEAM_BEGIN ? "team_CTF_redplayer" : "team_CTF_redspawn";
				break;
			case TEAM_BLUE:
				classname = teamstate == TEAM_BEGIN ? "team_CTF_blueplayer" : "team_CTF_bluespawn";
				break;
			case TEAM_FREE:
				if (level.CTF3ModeActive) {
					classname = teamstate == TEAM_BEGIN ? "info_player_start" : "info_player_deathmatch"; //info_player_start won't be renamed to info_player_deathmatch now
					break;
				}
			default:
				return NULL;
				break;
		}
	}
	count = 0;

	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), classname)) != NULL) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}
		spots[ count ] = spot;
		if (++count == MAX_TEAM_SPAWN_POINTS)
			break;
	}

	if ( !count ) {	// no spots that won't telefrag
		return G_Find( NULL, FOFS(classname), classname);
	}

	selection = rand() % count;
	return spots[ selection ];
}


/*
===========
SelectCTFSpawnPoint

============
*/
gentity_t *SelectCTFSpawnPoint ( team_t team, int teamstate, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;

	spot = SelectRandomTeamSpawnPoint ( teamstate, team );

	if (!spot) {
		return SelectSpawnPoint( vec3_origin, origin, angles );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*
===========
SelectSagaSpawnPoint

============
*/
gentity_t *SelectSagaSpawnPoint ( team_t team, int teamstate, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;

	spot = SelectRandomTeamSpawnPoint ( teamstate, team );

	if (!spot) {
		return SelectSpawnPoint( vec3_origin, origin, angles );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*---------------------------------------------------------------------------*/

static int QDECL SortClients( const void *a, const void *b ) {
	return *(int *)a - *(int *)b;
}


/*
==================
TeamplayLocationsMessage

Format:
	clientNum location health armor weapon powerups

==================
*/

void TeamplayInfoMessage( gentity_t *ent, const int team ) {
	char		entry[1024];
	char		string[8192];
	int			stringlength;
	int			i, j;
	gentity_t	*player;
	int			cnt;
	int			h, a;
	int			clients[TEAM_MAXOVERLAY];

	// figure out what client should be on the display
	// we are limited to 8, but we want to use the top eight players
	// but in client order (so they don't keep changing position on the overlay)
	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++) {
		player = g_entities + level.sortedClients[i];
		if (player->inuse && player->client->sess.sessionTeam ==
			// ent->client->sess.sessionTeam ) {
			team ) {
			clients[cnt++] = level.sortedClients[i];
		}
	}

	// We have the top eight players, sort them by clientNum
	qsort( clients, cnt, sizeof( clients[0] ), SortClients );

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;

	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++) {
		player = g_entities + i;
		if (player->inuse && player->client->sess.sessionTeam == team ) {

			h = player->client->ps.stats[STAT_HEALTH];
			a = player->client->ps.stats[STAT_ARMOR];
			if (h < 0) h = 0;
			if (a < 0) a = 0;

			Com_sprintf (entry, sizeof(entry),
				" %i %i %i %i %i %i",
				i, player->client->pers.teamState.location, h, a,
				player->client->ps.weapon, player->s.powerups);
			j = strlen(entry);
			if (stringlength + j > sizeof(string))
				break;
			strcpy (string + stringlength, entry);
			stringlength += j;
			cnt++;
		}
	}

	trap_SendServerCommand( ent-g_entities, va("tinfo %i %s", cnt, string) );
}

//nasty
team_t G_SpeccingClientTeam (gentity_t *ent) {

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.spectatorState == SPECTATOR_FOLLOW
		&& ent->client->ps.clientNum >= 0 && ent->client->ps.clientNum < MAX_CLIENTS) {

		return level.clients[ ent->client->ps.clientNum ].sess.sessionTeam;
	}

	return -1;
}

void CheckTeamStatus(void) {
	int i;
	gentity_t *loc, *ent;

	if (level.time - level.lastTeamLocationTime > TEAM_LOCATION_UPDATE_TIME) {

		level.lastTeamLocationTime = level.time;

		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;

			if ( ent->client->pers.connected != CON_CONNECTED ) {
				continue;
			}

			if (ent->inuse && (ent->client->sess.sessionTeam == TEAM_RED ||	ent->client->sess.sessionTeam == TEAM_BLUE)) {
				loc = Team_GetLocation( ent );
				if (loc)
					ent->client->pers.teamState.location = loc->health;
				else
					ent->client->pers.teamState.location = 0;
			}
		}

		for (i = 0; i < g_maxclients.integer; i++) {
			int refteam;
			ent = g_entities + i;

			if ( !ent->client || !ent->inuse || ent->client->pers.connected != CON_CONNECTED ) {
				continue;
			}

			if ( !ent->client->pers.teamInfo )
				continue;

			refteam = ent->client->sess.sessionTeam;
			if (refteam == TEAM_SPECTATOR)
				refteam = G_SpeccingClientTeam(ent);

			if ( refteam == TEAM_RED || refteam == TEAM_BLUE || refteam == TEAM_FREE ) {
				TeamplayInfoMessage( ent, refteam );
			}
		}
	}
}

/*-----------------------------------------------------------------*/

/*QUAKED team_CTF_redplayer (1 0 0) (-16 -16 -16) (16 16 32)
Only in CTF games.  Red players spawn here at game start.
*/
void SP_team_CTF_redplayer( gentity_t *ent ) {
}


/*QUAKED team_CTF_blueplayer (0 0 1) (-16 -16 -16) (16 16 32)
Only in CTF games.  Blue players spawn here at game start.
*/
void SP_team_CTF_blueplayer( gentity_t *ent ) {
}


/*QUAKED team_CTF_redspawn (1 0 0) (-16 -16 -24) (16 16 32)
potential spawning position for red team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_redspawn(gentity_t *ent) {
}

/*QUAKED team_CTF_bluespawn (0 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for blue team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_bluespawn(gentity_t *ent) {
}

