#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "base/array.h"
#include "base/vector.h"
#include "_controller.h"

namespace iso {

template<typename T> struct _neg {
	T	t;
	_neg(T &&t) : t(forward<T>(t)) {}
};
template<typename T> struct _dif {
	T	t;
	_dif(T &&t) : t(forward<T>(t)) {}
};

template<class T> const T&		operator-(const _neg<T> &t)	{ return t.t; }
template<class T> _neg<_dif<T>>	operator-(const _dif<T> &t)	{ return t; }
template<class T> _neg<_dif<T>>	operator~(const _neg<T> &t)	{ return -~t.t; }

inline _neg<ControllerAnalog>	operator-(ControllerAnalog e)	{ return e; }
inline _neg<ControllerAnalog2>	operator-(ControllerAnalog2 e)	{ return e; }
inline _neg<ControllerAnalog3>	operator-(ControllerAnalog3 e)	{ return e; }

inline _dif<ControllerAnalog>	operator~(ControllerAnalog e)	{ return e; }
inline _dif<ControllerAnalog2>	operator~(ControllerAnalog2 e)	{ return e; }
inline _dif<ControllerAnalog3>	operator~(ControllerAnalog3 e)	{ return e; }

struct ButtonHistory {
	static const int HITHISTORYSIZE = 16;
	struct HitInfo {
		int		button;
		float	time;
	} history[HITHISTORYSIZE];

	int		index;

	bool	Add(int button, float time) {
		if (button && time != history[index].time) {
			index = (index + 1) % HITHISTORYSIZE;
			history[index].button	= button;
			history[index].time		= time;
			return true;
		}
		return false;
	}
	bool	CheckCombo(int *combobtns, int numbtns, float currtime, float timeslack, int mask) const;
	ButtonHistory() : index(0)	{ history[0].time = 0; }
};

template<typename T> struct Envelope {
	T		init, peak, end;
	float	decay;
	T evaluate(float time, float duration) {
		return time < decay
			? lerp(init, peak, time / decay)
			: lerp(peak, end, (time - decay) / (duration - decay));
	}
	void set(paramT(T) _init, paramT(T) _peak, paramT(T) _end, float _decay) {
		init	= _init;
		peak	= _peak;
		end		= _end;
		decay	= _decay;
	}
	void set(paramT(T) _init) {
		init	= _init;
	}
};

class Rumble {
public:
	struct Channel {
		float			starttime;
		float			endtime;
		Envelope<float>	side[2];
		Channel() : starttime(-1.0f) {}
		void	Stop()	{ starttime = -1.0f; }
	};
	Channel			channels[4];
	float			total;
	bool			paused;

	Rumble() : total(0.0f), paused(false)			{}

	void			Update(class _Controller *cont, float time);
	float			GetCurrentMagnitude() const		{ return total; }
	void			Pause(bool pause = true)		{ paused = pause; }
	void			StopAll()						{ for (int i = 0; i < 4; i++) channels[i].Stop(); }
	int				GetFreeChannel()	const;
	int				StartConstant(float amp, float time);
	int				StartImpact(float lifetime, float amp, float decaystart, float time);
};

class ControllerButtons {
protected:
	uint32		curr_buttons, hit_buttons, release_buttons;
public:
	void		Clear()									{ curr_buttons =  hit_buttons = 0;	}
	void		CombineButtons(ControllerButtons &cb) const {
		cb.curr_buttons		|= curr_buttons;
		cb.hit_buttons		|= hit_buttons;
		cb.release_buttons	|= release_buttons;
	}
public:
	ControllerButtons() : curr_buttons(0), hit_buttons(0), release_buttons(0)	{}
	uint32		Down()						const		{ return curr_buttons;				}
	uint32		Hit()						const		{ return hit_buttons;				}
	bool		AllDown(int i)				const		{ return (curr_buttons & i) == i;	}
	bool		Down(int i)					const		{ return !!(curr_buttons & i);		}
	bool		Hit(int i)					const		{ return !!(hit_buttons & i);		}
	bool		Released(int i)				const		{ return !!(release_buttons & i);	}
	bool		MultiHit(int i)				const		{ return AllDown(i) && Hit(i);		}
	void		EatHit(int i)							{ hit_buttons &= ~i; 				}
	void		EatDown(int i)							{ curr_buttons &= ~i;				}
	void		Release(int i)							{ hit_buttons &= ~i;				}
	void		SetButton(int i)						{ curr_buttons |= i; hit_buttons |= i;	}
};

class Controller : public _Controller, public ControllerButtons {
	friend class _Controller;
private:
	array<float, CANA_TOTAL * 2>	analog;
	array<float, CANA_TOTAL>		accum_analog;
	ControllerType	type;
	uint32			accum_buttons, temp_buttons;
	Rumble			rumble;

public:
	Controller();
	ControllerType Type()							const		{ return type; }
	bool		IsValid()							const		{ return type != CTYPE_UNPLUGGED && type != CTYPE_UNKNOWN; }

	float		Analog(ControllerAnalog a)			const		{ return analog[a]; }
	position2	Analog(ControllerAnalog2 a)			const		{ return position2(load<float2>(&analog[a])); }
	position3	Analog(ControllerAnalog3 a)			const		{ return position3(load<float3>(&analog[a])); }

	template<typename A> auto	Analog(_dif<A> a)	const		{ return Analog(a.t) - Analog(A(a.t + CANA_TOTAL)); }
	template<typename A> auto	Analog(_neg<A> a)	const		{ return -Analog(a.t); }
	template<typename A> auto	Speed(A a)			const		{ return Analog(a.t) - Analog(A(a.t + CANA_TOTAL)); }

	void		Update(int i);
	void		BeginAccum();
	void		EndAccum();

	void		UpdateRumble(float time)				{ rumble.Update(this, time); }
	void		PauseRumble(bool _pause = true)			{ rumble.Pause(_pause); }
	void		StopRumble()							{ rumble.StopAll(); }
	int			StartRumbleConstant(float amp, float time) {
		return rumble.StartConstant(amp, time);
	}
	int			StartRumbleImpact(float lifetime, float amp, float decaystart, float time) {
		return rumble.StartImpact(lifetime, amp, decaystart, time);
	}
};

}//namespace iso

#endif
