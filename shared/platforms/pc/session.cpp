#include "session.h"
#include "base/interval.h"

using namespace iso;

iso::Multiplayer multiplayer;

void Multiplayer::Init(const MultiplayerCB &_cb)
{
	cb = _cb;
	num_players = num_local = 0;

}

bool Multiplayer::CreateSession(uint32 flags, int host, int publicslots, int privateslots)
{
	flags	|= 1 << host;
	state	= MP_HOSTING;

	int		n = 0;
	for (int i = 0, m = flags & _SESS_USERS; m; i++, m>>=1) {
		if (m & 1) {
			_MPPlayer	&p	= players[n];
			p.game_player	= NULL;
			p.flags			= PLAYER_LOCAL;
			if (i == host)
				p.flags.set(PLAYER_HOST);
			p.local_index	= i;
			p.global_index	= n;
			p.session_id	= n;
//			strcpy(p.name, GetLocalUserName(i));
			cb(MPC_JOINED, &p);
			n++;
		}
	}
	num_local	= n;
	num_players = n;
	return true;
}

bool Multiplayer::GameSearch(uint32 flags, int host, int query, int num_games)
{
	return false;
}

bool Multiplayer::JoinGame(int i, int local_mask)
{
	return true;
}

bool Multiplayer::StartGame()
{
	state = MP_PLAYING;
	return !!cb(MPC_STARTGAME, NULL);
}

bool Multiplayer::EndGame()
{
	state = MP_LOBBY;
	cb(MPC_ENDGAME, NULL);
	return true;
}

bool Multiplayer::KillGame()
{
	return true;
}

bool Multiplayer::LeaveSession()
{
	for (int i = 0; i < MP_MAX_PLAYERS; i++) {
		_MPPlayer	*p = &players[i];
		if (MPPlayer *gp = p->game_player) {
			cb(MPC_LEAVING, gp);
			p->flags		= 0;
			p->game_player	= NULL;
			gp->handle		= 0;
			gp->local_index	= -1;
		}
	}
	num_players	= 0;
	num_local	= 0;
	state		= MP_IDLE;
	return true;
}

MPHANDLE Multiplayer::GetBySessionID(uint8 id) {
	for (int i = 0; i < MP_MAX_PLAYERS; i++) {
		if (players[i].game_player && players[i].session_id == id)
			return &players[i];
	}
	return 0;
}

MPHANDLE Multiplayer::GetByUserIndex(uint8 id) {
	for (int i = 0; i < MP_MAX_PLAYERS; i++) {
		if (players[i].game_player && players[i].local_index == id)
			return &players[i];
	}
	return 0;
}

bool Multiplayer::AllReady() const {
	for (int i = 0; i < MP_MAX_PLAYERS; i++) {
		if (players[i].game_player && !(players[i].flags & PLAYER_READY))
			return false;
	}
	return true;
}

bool MPPlayer::Send(const void *data, size_t size, int flags) const {
	return multiplayer.SendData(handle, data, size, flags);
}

//-----------------------------------------------------------------------------
//	Leaderboard
//-----------------------------------------------------------------------------

void Leaderboard::Move(int x, int size)
{
	int	bn0 = (offset - (N / 4)) / N;
	offset = clamp(offset + x, 0, max(int(total - size), 0));
	int	bn1 = (offset - (N / 4)) / N;
	if (bn0 != bn1) {
		if (bn1 == bn0 + 1)
			ready[bn0 & 1] = false;
		else if (bn1 == bn0 - 1)
			ready[bn1 & 1] = false;
		else
			ready[0] = ready[1] = false;
	}
}