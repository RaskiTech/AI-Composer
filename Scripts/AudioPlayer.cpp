#include <Eagle.h>
#include <glm/gtx/common.hpp>
#include "AudioPlayer.h"

constexpr double tau = 2.0 * glm::pi<double>();
constexpr double twoOverPI = 2.0 / glm::pi<double>();
constexpr double halfPI = glm::pi<double>() / 2;

NoiseMaker<short>* NotePlayer::emitter;
uint8_t NotePlayer::soundTypeIndex;

struct Note {
	Note(double pitch, double activateTime, uint8_t instrumentIndex) : pitch(pitch), activateTime(activateTime), instrumentIndex(instrumentIndex) {}

	bool pressedDown = true;
	bool finishedPlaying = false;
	double activateTime;
	double deactivateTime;
	float pitch = 0;
	uint8_t instrumentIndex = 0;
};
struct SimpleInstrument {
	SimpleInstrument(double attackTime, double decayTime, double(*MakeSoundWave)(double, double)) 
		: attackTime(attackTime), decayTime(decayTime), MakeSoundWave(MakeSoundWave) {}
	double attackTime;
	double decayTime;
	double(*MakeSoundWave)(double, double);
};

// Here are defined all the sounds. These are functions that make the sounds, and below these functions are used.
// Output a double between [-1, 1]

double MakeSineWave(double hz, double deltaTime) { 
	return glm::sin(hz * tau * deltaTime); 
}
double MakeSquareWave(double hz, double deltaTime) { 
	return glm::sin(hz * tau * deltaTime) > 0.0 ? 0.7 : -0.7;
}
double MakeTriangleWave(double hz, double deltaTime) {
	return glm::asin(glm::sin(hz * tau * deltaTime)) * twoOverPI;
}
double MakeSawWave(double hz, double deltaTime) { 
	return twoOverPI * (hz * glm::pi<double>() * glm::fmod(deltaTime, 1.0 / hz) - halfPI);
}
double MakeCustomWave(double hz, double deltaTime) {
	return 0.5 * MakeSquareWave(hz, deltaTime) + 0.5 * MakeSquareWave(hz * 2, deltaTime);
}
double MakeCustom1Instrument(double hz, double deltaTime) {
	double output = 0;

	output += 1.0 * MakeSineWave(hz, deltaTime);
	output += 0.5 * MakeSineWave(2.0 * hz, deltaTime);
	output += 0.25 * MakeSquareWave(3.0 * hz, deltaTime);

	return output;
}
double MakeCustom2Instrument(double hz, double deltaTime) {
	// Some modulation
	//const double modulationHZ = 2.0; // 0 >=
	//const double modulationAmplitude = 0.3; // [0, 1]
	//hz += modulationAmplitude * (glm::sin(modulationHZ * tau * deltaTime) / 2 + 1.0);

	double output = 0;
	output += 1.0 * MakeSineWave(1.0 * hz, deltaTime);
	output += 0.5 * MakeTriangleWave(1.0 * hz, deltaTime);
	output += 0.25 * MakeSineWave(2.0 * hz, deltaTime);

	return output;
}

// Not all of these are used, check below in GetInstrument(uint8_t index) which ones are used.
SimpleInstrument sineInstrument    (0.05f, 0.05f, MakeSineWave);
SimpleInstrument squareInstrument  (0.05f, 0.05f, MakeSquareWave);
SimpleInstrument triangleInstrument(0.05f, 0.05f, MakeTriangleWave);
SimpleInstrument sawInstrument     (0.05f, 0.05f, MakeSawWave);
SimpleInstrument custom1Instrument (0.01f, 0.05f, MakeCustom1Instrument);
SimpleInstrument custom2Instrument (0.05f, 0.15f, MakeCustom2Instrument);

std::vector<Note> playingNotes;
mutex mutexNotes;
atomic<double> masterVolume = 1.0;

// These are the instrument indices
SimpleInstrument& GetInstrument(uint8_t index) {
	switch (index) {
	case 0:
		return sineInstrument;
	case 1:
		return triangleInstrument;
	case 2:
		return custom1Instrument;
	case 3:
		return custom2Instrument;
	default:
		return sineInstrument;
	}
}

typedef bool(*lambda)(Note const& item);
template<class T>
void safe_remove(T& v, lambda f)
{
	auto n = v.begin();
	while (n != v.end())
		if (!f(*n))
			n = v.erase(n);
		else
			++n;
}

double CalculateNoiseSample(double deltaTime) {
	unique_lock<mutex> lm(mutexNotes);

	double output = 0.0;

	for (auto& note : playingNotes)
	{
		double sound = 0;
		SimpleInstrument& instrument = GetInstrument(note.instrumentIndex);
		sound = instrument.MakeSoundWave(note.pitch, deltaTime);

		double loudnessFactor;
		if (note.pressedDown) {
			double attackedTime = deltaTime - note.activateTime;
			if (attackedTime < instrument.attackTime)
				loudnessFactor = attackedTime / instrument.attackTime;
			else
				loudnessFactor = 1;
		}
		else {
			double decayedTime = deltaTime - note.deactivateTime;
			loudnessFactor = 1 - decayedTime / instrument.decayTime;

			if (loudnessFactor < .001) {
				note.finishedPlaying = true;
				loudnessFactor = 0;
			}
		}

		output += sound * loudnessFactor;
		//LOG("It's {0}", output);
	}

	safe_remove<std::vector<Note>>(playingNotes, [](Note const& item) { return !item.finishedPlaying; });

	double returnVal = output * masterVolume;
	//LOG("{0} {1}", returnVal, masterVolume);
	//if (returnVal > 1)
	//	LOG("Y. Keys: {0}", playingNotes.size());
	//else
	//	LOG("N");
	return returnVal * 0.15; // Constant so the output isn't over 1 when pressing multiple keys
}

void NotePlayer::Init() {
	playingNotes = std::vector<Note>();

	std::vector<std::wstring> devices = NoiseMaker<short>::Enumerate();
	for (auto d : devices) std::wcout << "Found an input device: " << d << std::endl;

	emitter = new NoiseMaker<short>(devices[0], 44100, 1, 8, 512);

	emitter->SetUserFunction(CalculateNoiseSample);
}
void NotePlayer::SetVolume(float val) {
	masterVolume = val;
}

void NotePlayer::KeyDown(double pitch) {
	mutexNotes.lock();
	playingNotes.emplace_back(pitch, emitter->GetTime(), soundTypeIndex);
	mutexNotes.unlock();
}
void NotePlayer::KeyUp(double remove) {
	mutexNotes.lock();
	for (Note& note : playingNotes) {
		if (note.pitch == (float)remove) {
			note.pressedDown = false;
			note.deactivateTime = emitter->GetTime();
		}
	}
	mutexNotes.unlock();
}
void NotePlayer::ClearKeys() {
	mutexNotes.lock();
	playingNotes.clear();
	mutexNotes.unlock();
}

void NotePlayer::SetWaveType(SoundWaveType type) {
	soundTypeIndex = (uint8_t)type;
}

double NotePlayer::IndexToPitch(int keycode) {
	constexpr double lowestNote = 41.20; // E1
	constexpr double twoToPowerOneTwelwth = 1.05946398436;
	return lowestNote * glm::pow(twoToPowerOneTwelwth, keycode);
}

double NotePlayer::GetSoundWaveAt(double deltaTime) {
	//LOG_WARN("New:");
	return CalculateNoiseSample(deltaTime);
}
