#include "controller.h"
#include "base/vector.h"

namespace iso {

Controller::Controller() : type(CTYPE_UNPLUGGED), accum_buttons(0) {
	clear(analog);
}

void Controller::Update(int i) {
	uint32	prev_buttons	= curr_buttons;
	copy_n(analog + 0, analog + CANA_TOTAL, CANA_TOTAL);
	curr_buttons			= _Controller::Update(this, i);
#if !defined PLAT_IOS && !defined PLAT_ANDROID && !defined PLAT_MAC
	if (analog[CANA_LEFT_X]	< -0.9f)
		curr_buttons |= CBUT_ANAL_LEFT;
	else if (analog[CANA_LEFT_X] > 0.9f)
		curr_buttons |= CBUT_ANAL_RIGHT;
	if (analog[CANA_LEFT_Y]	< -0.9f)
		curr_buttons |= CBUT_ANAL_DOWN;
	else if (analog[CANA_LEFT_Y] > 0.9f)
		curr_buttons |= CBUT_ANAL_UP;

	if (analog[CANA_RIGHT_X] < -0.9f)
		curr_buttons |= CBUT_ANAR_LEFT;
	else if (analog[CANA_RIGHT_X] > 0.9f)
		curr_buttons |= CBUT_ANAR_RIGHT;
	if (analog[CANA_RIGHT_Y] < -0.9f)
		curr_buttons |= CBUT_ANAR_UP;
	else if (analog[CANA_RIGHT_Y] > 0.9f)
		curr_buttons |= CBUT_ANAR_DOWN;
#endif
	accum_buttons		|= curr_buttons;
	hit_buttons			= curr_buttons & ~prev_buttons;
	release_buttons		= ~curr_buttons & prev_buttons;
}

void Controller::BeginAccum() {
	copy_n(accum_analog + 0, analog + CANA_TOTAL, CANA_TOTAL);
	hit_buttons		= accum_buttons & ~temp_buttons;
	release_buttons	= ~accum_buttons & temp_buttons;
	temp_buttons	= curr_buttons;
}

void Controller::EndAccum() {
	accum_analog	= analog;
	curr_buttons	= temp_buttons;
	temp_buttons	= accum_buttons;
	accum_buttons	= 0;
}

bool ButtonHistory::CheckCombo(int	*combobtns, int numbtns, float currtime, float timeslack, int mask) const {
	for (int off = HITHISTORYSIZE; numbtns--;) {
		for (int currbtn = combobtns[numbtns]; ; off--) {
			if (off < numbtns)
				return false;

			const HitInfo &hitinfo = history[(index + off) % HITHISTORYSIZE];

			if (currtime - hitinfo.time > timeslack)
				return false;

			if (hitinfo.button & currbtn) {
				currtime = hitinfo.time;
				break;
			}
			if (hitinfo.button & mask)
				return false;
		}
	}
	return true;
}

int	Rumble::GetFreeChannel() const {
	float	oldest = 1e24f;
	int		index = -1;
	for (int i = 0; i < num_elements32(channels); i++) {
		if (channels[i].starttime < oldest) {
			index = i;
			oldest = channels[i].starttime;
		}
	}
	return index;
}

void Rumble::Update(_Controller *cont, float time) {
	float maxvals[2] = {0.0f};
	if (!paused) {
		for (size_t i = 0; i < num_elements(channels); i++) {
			Channel &ch = channels[i];
			if (ch.starttime >= 0) {
				if (ch.endtime >= 0) {
					if (time < ch.endtime) {
						float	t = time - ch.starttime, d = ch.endtime - ch.starttime;
						maxvals[0] = ch.side[0].evaluate(t, d);
						maxvals[1] = ch.side[1].evaluate(t, d);
					} else {
						ch.starttime = -1.0f;  // Free up the channel
					}
				} else {
					maxvals[0] = max(maxvals[0], ch.side[0].init);
					maxvals[1] = max(maxvals[1], ch.side[1].init);
				}
			}
		}
	}
	cont->SetRumble(maxvals[0], maxvals[1]);
}

int Rumble::StartConstant(float amp, float time) {
	int i = GetFreeChannel();
	if (i >= 0) {
		Channel	&ch = channels[i];
		ch.starttime	= time;
		ch.endtime		= -1.0f;
		ch.side[0].set(amp);
		ch.side[1].set(amp);
	}
	return i;
}

int	Rumble::StartImpact(float lifetime, float amp, float decaystart, float time) {
	int i = GetFreeChannel();
	if (i >= 0) {
		Channel	&ch = channels[i];
		ch.starttime		= time;
		ch.endtime			= time + lifetime;
		ch.side[0].set(amp, amp, 0, decaystart);
		ch.side[1].set(amp, amp, 0, decaystart);
	}
	return i;
}

} // namespace iso
