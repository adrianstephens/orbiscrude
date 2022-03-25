#ifndef SESSION_H
#define SESSION_H

#include "base/defs.h"
#include "events.h"

namespace iso {

enum LANGUAGE {
	ISOLANG_ENGLISH,
	ISOLANG_JAPANESE,
	ISOLANG_GERMAN,
	ISOLANG_FRENCH,
	ISOLANG_SPANISH,
	ISOLANG_ITALIAN,
};

enum MP_STATE {
	MP_IDLE,
	MP_HOSTING,
	MP_JOINING,
	MP_LOBBY,
	MP_STARTING,
	MP_PLAYING,
	MP_SEARCHING,
	MP_DONESEARCH,
	MP_INSHELL,
};

enum MP_CALLBACK {
	MPC_EXITGAME		= -1,
	MPC_INVITE			= 0,
	MPC_KILLSESSION		= 1,
	MPC_LEAVING			= 2,
	MPC_JOINED			= 3,
	MPC_READY			= 4,
	MPC_UNREADY			= 5,
	MPC_STARTGAME		= 6,
	MPC_ENDGAME			= 7,
	MPC_SIGNIN			= 8,
	MPC_SIGNOUT			= 9,
	MPC_INSTALLED		= 10,
	MPC_ETHERNET		= 11,
	MPC_RECEIVEDATA		= 12,
	MPC_MIGRATE			= 13,
	MPC_WRITESTATS		= 14,
	MPC_SEARCHCOMPLETE	= 15,
};

enum SIGNIN_FLAGS {
	SIGNIN_NORMAL		= 0,
	SIGNIN_ONLINE		= 1,
	SIGNIN_GUEST		= 2,
};

enum SESSION_FLAGS {
	SESS_USER0			= 1<<0,
	SESS_USER1			= 1<<1,
	SESS_USER2			= 1<<2,
	SESS_USER3			= 1<<3,
	_SESS_USERS			= 15,
	SESS_RANKED			= 1<<8,
	SESS_STATS			= 1<<9,
	SESS_LAN			= 1<<10,
	SESS_LOCAL			= 1<<11,
};

enum PLAYER_FLAGS {
	_PLAYER_NONE		= 0,

	PLAYER_LOCAL		= 1<<0,
	PLAYER_HOST			= 1<<1,
	PLAYER_PRIVATE		= 1<<2,
	PLAYER_READY		= 1<<3,
	PLAYER_CAMERA		= 1<<4,
	PLAYER_HEADSET		= 1<<5,
	PLAYER_FACEBOOK		= 1<<6,
	PLAYER_FAKE			= 1<<7,
	PLAYER_ONLINE		= 1<<9,
	PLAYER_PLAYONLINE	= 1<<10,
	PLAYER_PLAYSOLO		= 1<<11,

	PLAYER_GUEST		= 0,
};

enum MP_SENDDATA_FLAGS {
	MP_SD_UNRELIABLE	= 0,
	MP_SD_RELIABLE		= 1<<0,
	MP_SD_LOBBY			= 1<<1,
};

enum CHECKPPOINT_MODE {
	CP_INSTANT,
	CP_START,
	CP_STOP,
};

enum SAVEGAME_STATUS {
	SG_ERROR_NOT_SIGNED_IN		= -100,
	SG_ERROR_NO_DEVICE_SET		= -20,
	SG_ERROR_ACCESS_DENIED		= -19,
	SG_ERROR_BAD_DATA			= -18,
	SG_ERROR_OTHER_PROFILE_DATA	= -17,
	SG_ERROR_DEVICE_FULL		= -16,
	SG_ERROR_GENERAL			= -2,
	SG_ERROR_NOT_FOUND			= -1,
	SG_ERROR_NONE				= 0,
	SG_READING					= 1,
	SG_WRITING					= 2
};

enum OSK_STATUS {
	OSK_IDLE			= 0,
	OSK_OPEN			= 1,
	OSK_FINISHED		= 2,
	OSK_CANCELED		= 3,
	OSK_ERROR			= 4
};

struct SaveGameIcon {
	void*		data;
	size_t		size;
	const char*	displayname;
};

template<int ID> struct MPMessage {
	uint8 id;
	MPMessage() : id(ID)	{}
};

typedef iso::callback<int(MP_CALLBACK mpc, void *params)>	MultiplayerCB;

//-----------------------------------------------------------------------------
//	internal structures
//-----------------------------------------------------------------------------

struct _MPPlayer {
	class MPPlayer	*game_player;
	flags<PLAYER_FLAGS>	flags;
	char			name[17];
	int8			local_index;
	uint8			global_index, session_id;

	const char*		Name()					const	{ return name;				}
	int				BaseIndex()				const	{ return local_index;		}
	iso::flags<PLAYER_FLAGS> GetFlags()		const	{ return flags;				}
	bool			test(PLAYER_FLAGS f)	const	{ return flags.test(f);		}
	bool			test_any(uint32 f)		const	{ return flags.test_any(f);	}
	bool			test_all(uint32 f)		const	{ return flags.test_all(f);	}

	bool			GetPicture(void *pixels, int *w, int *h);
	bool			SetReady(bool ready)			{ flags.set(PLAYER_READY, ready); return true; }

	_MPPlayer() : game_player(0), flags(0)	{}
};

typedef uint32		MPPresence;
typedef _MPPlayer	*MPHANDLE;
typedef _MPPlayer	*MPSYSTEM;

//-----------------------------------------------------------------------------
//	MPPlayer
//-----------------------------------------------------------------------------

class MPPlayer {// : public virtfunc<bool(const void*, size_t)> {
	friend class Multiplayer;

	MPHANDLE		handle;
	int				local_index;
	int				team;

public:
	MPHANDLE		Handle()				const	{ return handle; }
	const char*		Name()					const	{ return handle ? handle->name : NULL;		}
	bool			LoggedIn()				const	{ return handle != NULL;					}
	int				GetLocalIndex()			const	{ return local_index;						}
	int				GetGlobalIndex()		const	{ return handle ? handle->global_index : -1;}
	uint8			SessionID()				const	{ return handle->session_id;				}

	const flags<PLAYER_FLAGS> GetFlags()	const	{ return handle ? handle->flags : _PLAYER_NONE;	}
	bool			test(PLAYER_FLAGS f)	const	{ return GetFlags().test(f);		}
	bool			test_any(uint32 f)		const	{ return GetFlags().test_any(f);	}
	bool			test_all(uint32 f)		const	{ return GetFlags().test_all(f);	}
	bool			SetReady(bool ready)	const	{ return handle && handle->SetReady(ready);	}
	float			ReadyFor()				const	{ return 0;	}
	bool			Remove()				const;
	bool			GetPicture(void *pixels, int *w, int *h)	const	{ return false;	}
	void			RenderVideo(int x, int y, int w, int h)		const	{}

	bool			Send(const void *data, size_t size, int flags)				const;
	template<typename T> bool Send(const T &m, int flags = MP_SD_UNRELIABLE)	const	{ return Send(&m, sizeof(m), flags); }

	MPPlayer() : handle(0), local_index(-1), team(0)	{}
};

//-----------------------------------------------------------------------------
//	Multiplayer
//-----------------------------------------------------------------------------
struct ReceiveData {
	MPSYSTEM	from;
	const void*	data;
	size_t		size;
};

class Multiplayer {
	static const int	MP_MAX_PLAYERS	= 8;

	_MPPlayer			players[MP_MAX_PLAYERS];
	MP_STATE			state;
	int					num_players;
	int					num_local;
	MultiplayerCB		cb;

public:
	void		Init(const MultiplayerCB &_cb);
	MultiplayerCB SetHandler(const MultiplayerCB &_cb)			{ MultiplayerCB old = cb; cb = _cb; return old; }
	void		Update()										{}
	MP_STATE	State()											{ return state;	}

	void		Clear()											{}
	void		SetContext(uint32 id, uint32 value)				{}
//	void		SetProperty(uint32 id, const XuserData &value)	{ _XUSER_PROPERTY*p = properties.expand(); p->dwPropertyId = id; p->value = value;	}

	bool		Invite(int user, const char *message)			{ return false; }
	bool		InviteParty(int user)							{ return false; }
	int			PartyMembers()									{ return 0;		}

	bool		ClearSearch()									{ return false; }
	bool		GameSearch(uint32 flags, int host, int query, int num_games);
	int			NumGames()					const				{ return 0;				}

//SESSION
	bool		CreateSession(uint32 flags, int host, int publicslots, int privateslots);
	bool		JoinGame(int i, int local_mask);
	bool		StartGame();
	bool		EndGame();
	bool		KillGame();
	bool		LeaveSession();
	bool		Remove(MPSYSTEM system);
	bool		EnableJoining(bool enable = true)				{ return false;			}
	int			NumPlayers()				const				{ return num_players;	}
	int			NumLocalPlayers()			const				{ return num_local;		}
	bool		AllReady()					const;
	bool		IsHost()					const				{ return true;			}
	bool		SendData(MPSYSTEM system, const void *data, size_t size, int flags)	{ return true; }
	void		CheckPoint(const char *name, CHECKPPOINT_MODE mode = CP_INSTANT) const	{}

//USERS
	int			NumUsers()					const				{ return 1;			}
	int			LocalUserMask()				const				{ return 1;			}
	int			LocalOnlineMask()			const				{ return 0;			}
	_MPPlayer&	GetUser(int i)									{ return players[i];}
	bool		Signin(int user, uint32 flags)					{ return true;		}
	bool		Signout(int user, uint32 flags)					{ return true;		}
	void		SetPresence(int player, MPPresence pres)	{}
	void		Award(int player, int award)				{}
	bool		GetAwardIcon(int player, int award, void *pixels, int *w, int *h)	{ return false; }

	bool		SaveGameEnabled(int player)		const			{ return true; }
	bool		SaveGameDeviceValid(int player)	const			{ return true; }
	void		ResetSaveGame(int player)						{}
	int			ReadSaveGame(int player, void *data, int data_size, void *opt_data, int opt_size, const SaveGameIcon &icon)	{ return 0;	}
	int			WriteSaveGame(int player, void *data, int data_size, void *opt_data, int opt_size, const SaveGameIcon &icon, bool allow_device_selection = false)	{ return 0;	}
	int			GetSaveGameOperationStatus(int player)			{ return 0;	}

//PLAYERS
	void		SetPlayer(MPPlayer &player, MPHANDLE h)			{ player.handle = h; h->game_player = &player; player.local_index = h->local_index; }
	MPPlayer*	GetByHandle(MPHANDLE h)		const				{ return h->game_player;	}
	int			GetUserIndex(MPHANDLE h)	const				{ return h->local_index;	}
	MPHANDLE	GetByUserIndex(uint8 i);
	MPHANDLE	GetBySessionID(uint8 id);
	const flags<PLAYER_FLAGS> GetPlayerFlags(MPHANDLE h)		{ return h->flags;			}
	void		WriteStatistics(int global_player_index, int board, int64 score, int data_size = 0, void *data = NULL)	{}

//KEYBOARD
	int			OSKStatus()								const	{ return 0; }
	int			OSKGetText(char *buffer, size_t maxlen) const	{ return 0; }
	int			OSKOpen(int local_index, int num_chars, const char *message, const char *init_text, const char *description = NULL) { return 0; }

//STORE
	bool		GetLicenseEnabled(int _value)					{ return false; }
	bool		EnumerateContent(int user = -1)					{ return false;	}
	bool		Store(int user = -1)							{ return false; }
	int			StoreItemIndex(const char *item)				{ return 0;	}

//MISC
	void		ShowAd(const char *location = 0, bool on = true)	{}
	LANGUAGE	GetLanguage()							const		{ return ISOLANG_ENGLISH; }
};

//-----------------------------------------------------------------------------
//	Leaderboard
//-----------------------------------------------------------------------------

class Leaderboard {
protected:
	enum FILTER {
		FILTER_FRIENDS,
		FILTER_OVERALL,
		FILTER_MYSCORE,
	} filter;

	int					N;
	int					offset;
	iso::uint32			total;
	bool				ready[2];
public:
	void		SetSpec(int _spec)				{}
	bool		Ready(FILTER _filter)			{ return true;	}
	void		Move(int x, int size);
	void		SetPos(int x, int size)			{ Move(x - offset, size); }
	int			PlayerRank()		const		{ return -1;	}

	int			Index(int i)		const		{ return i + offset;			}
	bool		Exists(int i)		const		{ return i + offset < total;	}
	int			Rating(int i)		const		{ return (i + offset + 1) * 10;	}
	int			Rank(int i)			const		{ return i + offset + 1;		}
	const char*	Gamer(int i)		const		{ return "tag";	}
	iso::uint32	Info(int i, int j)	const		{ return 0;		}
	bool		IsPlayer(int i)		const		{ return false;	}
	int			Total()				const		{ return total;	}

	Leaderboard(int _titleid, int _N = 20) : N(_N) {
		total = 100;
		ready[0] = ready[1] = true;
	}
};

}// namespace iso

extern iso::Multiplayer multiplayer;

#endif	// SESSION_H
