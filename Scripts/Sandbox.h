#pragma once
#include "AudioPlayer.h"
#include "Elements.h"
#include <thread>
#include <Eagle.h>
using namespace Egl;

const float playSpeedAtProgramStart = 0.5f;

constexpr size_t valuesBufferSize = 120 * 120 + 2 * 120;
constexpr size_t sliderAmount = 19;

class AppManager : public Script {
public:
	AppManager(Entity songStepLine);
	void OnCreate();
	void OnUpdate();

	void RandomizePredictVals();

	// API
	void PlaySong(bool play, uint32_t startTimeStep) { songPlaying = play; songStep = startTimeStep; }
	void SetPlaySpeed(float timeBetweenSteps) { this->timeBetweenSteps = timeBetweenSteps; }
	void SetInstrument(NotePlayer::SoundWaveType type) { notePlayer.SetWaveType(type); }
	void SetVolume(float volume) { notePlayer.SetVolume(volume); }
	void ChangePredictValue(int index, float value);
	void QueuePredictThread();

	void SaveAsWave(std::function<void()> doneCallback);
	void SaveAsMidi(std::function<void()> doneCallback);

private:
	// Thread 2 functions
	void UpdateDataFromVector(); // Use predictValues to predict
	std::vector<float> GetPCAValues();
	void PreProcessData(std::vector<float>& data, float sens);

	void UpdateTextures();
	void ValidateSongStatus(); // Make sure that the corect notes are playing
	void AdvanceSongOneStep();

	// Song members

	bool songPlaying = false;
	float timeBetweenSteps = 1 / (playSpeedAtProgramStart * 30) * 0.5f;
	uint32_t songStep = 0;
	std::array<bool, 96> activeNotesThisStep;
	NotePlayer notePlayer;
	Entity songStepLine;

	// Data members

	std::unordered_map<int, float> setPredictValues;
	std::vector<float> predictValues;

	std::vector<float> imageData1;
	std::vector<float> imageData2;
	//vecs = means + np.dot(rand_vecs * evals, evecs)
	float valuesBuffer[valuesBufferSize]; // 120*120 evecs, 120 means, 120 evals
};

class MusicScene : public Scene {
public:
	Ref<Texture> CreateTexture(Entity& parent, UIEntityParams params, float pos);

	void NoteSensitivityChanged(float val);
	void PlaySpeedChanged(float val);
	void SliderChanged(float val, int index);
	void MinDurChanged(float val);
	void VolumeChanged(float val);
	void SetInstrument(int index);
	void ChangeUISliderValue(int index, float value) { sliderEntitites[index]->SetValue(value); }
	void SaveAsMidi() { GetAppManager()->SaveAsMidi([]() {}); }
	void SaveAsWave() { GetAppManager()->SaveAsWave([]() {}); }

	Entity CreateSlider(Entity& parent, const glm::vec2& pos, int index);
	Entity CreateFillBar(Entity& parent, const glm::vec2& pos, const glm::vec4& color, float startVal, void(MusicScene::*callback)(float));
	Entity CreateToggleGroup(Entity& parent, const glm::vec2& pos, 
		const std::string& texturePath, const std::array<glm::vec4, 4>& colors, void(MusicScene::* callback)(int));
	Entity CreateButton(Entity& parent, const glm::vec2& pos, const std::string& texturePath,
		const glm::vec4& color, void(MusicScene::* callback)());

	void CreateInterface();
	void SceneBegin() override;
	void SceneEnd() override {};

	AppManager* GetAppManager() { return (AppManager*)appManagerEntity.GetComponent<NativeScriptComponent>().baseInstance; }
	
	// Thread 2 functions
	void UpdateTextures(const std::vector<float>& data, float sensitivity);   // Updates Textures from data
	//
private:
	std::array<Slider*, sliderAmount> sliderEntitites;
	Entity appManagerEntity;
	Ref<Texture> textures[16];
};