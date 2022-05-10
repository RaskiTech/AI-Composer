#pragma once
#include <thread>
#include <Eagle.h>
#include <EagleApplicationStartup.h>
#include <fdeep/fdeep.hpp>
#include <filesystem>
#include <stdio.h>
#include "Sandbox.h"
#include "Elements.h"
#include "AudioPlayer.h";
using namespace Egl;

// Saving with wav
constexpr int sampleRate = 44100;
constexpr int bitDepth = sizeof(uint16_t) * 8;

// American palette
#define GRAY_DARK    glm::vec4{ 45.0f  / 255, 52.0f  / 255, 54.0f  / 255, 1 }
#define GRAY         glm::vec4{ 178.0f / 255, 190.0f / 255, 195.0f / 255, 1 }
#define GRAY_LIGHT   glm::vec4{ 223.0f / 255, 230.0f / 255, 233.0f / 255, 1 }
#define LIGHT_GREEN  glm::vec4{ 85.0f  / 255, 239.0f / 255, 196.0f / 255, 1 }
#define LIGHT_RED    glm::vec4{ 255.0f / 255, 118.0f / 255, 117.0f / 255, 1 }
#define LIGHT_BLUE   glm::vec4{ 116.0f / 255, 185.0f / 255, 255.0f / 255, 1 }
#define LIGHT_PURPLE glm::vec4{ 162.0f / 255, 155.0f / 255, 254.0f / 255, 1 }
#define LIGHT_YELLOW glm::vec4{ 255.0f / 255, 234.0f / 255, 167.0f / 255, 1 }
#define LIGHT_ORANGE glm::vec4{ 250.0f / 255, 177.0f / 255, 160.0f / 255, 1 }

struct AI {
	AI(fdeep::model model) : model(model) {}
	fdeep::model model;
};

static AI* model = nullptr;


static atomic<float> sensitivity = 0.2f;
static float volume = 0.5f;

static atomic<bool> thread2Working = false;

static atomic<bool> needToPredict = true;
static atomic<bool> predictionDone = false;
static atomic<bool> useData1 = false;
static atomic<uint8_t> minDur = 4;


void AppManager::QueuePredictThread() { needToPredict = true; }

#pragma region SaveToFile

void writeFile(std::ofstream& stream, int value, int size) { stream.write(reinterpret_cast<const char*>(&value), size); }
double clip(double dSample, double dMax) {
	if (dSample >= 0.0)
		return fmin(dSample, dMax);
	else
		return fmax(dSample, -dMax);
}
static void SetStepLinePos(uint32_t step, UIAlignComponent& stepTrans) {
	constexpr float minmaxRelParentVal = 0.57f;
	constexpr float upperRelParentVal = 0.3f;
	constexpr float lowerRelParentVal = -0.14f;
	if (step / (96 * 8) == 0) { // The upper
		stepTrans.SetYPosValue(upperRelParentVal);
		stepTrans.SetXPosValue(((float)step / (96 * 8)) * 2 * minmaxRelParentVal - minmaxRelParentVal);
	}
	else {
		stepTrans.SetYPosValue(lowerRelParentVal);
		stepTrans.SetXPosValue(((float)(step - 96 * 8) / (96 * 8)) * 2 * minmaxRelParentVal - minmaxRelParentVal);
	}
}
void AppManager::SaveAsWave(std::function<void()> doneCallback) {
	bool wasSongPlaying = songPlaying;
	int wasSongStep = songStep;
	{
		notePlayer.SetPause(true);
		songPlaying = false;
		songStep = 0;
		activeNotesThisStep = {};
		notePlayer.ClearKeys();
		std::ofstream file;
		{
			int i = 1;
			stringstream ss;
			do {
				ss.str(std::string()); // Clear it
				ss << "Song" << i << ".wav";
				i++;
			} while (std::filesystem::exists(ss.str()));
			file.open(ss.str(), std::ios::binary);
		}

		// Header chunk
		file << "RIFF";
		file << "----";
		file << "WAVE";

		// Format chunk
		file << "fmt ";
		writeFile(file, 16, 4); // Format chunk size
		writeFile(file, 1, 2); // Compression
		writeFile(file, 1, 2); // Num of channels
		writeFile(file, sampleRate, 4); // Sample rate
		writeFile(file, sampleRate * bitDepth / 8, 4); // Byte rate
		writeFile(file, bitDepth / 8, 2); // Block align
		writeFile(file, bitDepth, 2); // Bit depth

		// Data chunk
		file << "data";
		file << "----";

		double dTime = 0;
		double nextTimeStepTime = 0;
		int prePos = file.tellp();
		int maxAmplitude = glm::pow(2, bitDepth - 1) - 1;
		for (int i = 0; i < (int)(sampleRate * (timeBetweenSteps * 16 * 96)); i++) {
			//if (i % 10 == 0)
			//	LOG("Little delay. Percent done: {0}", (float)i / (sampleRate * (timeBetweenSteps * 16 * 96)));

			dTime += 1.0 / sampleRate;
			NotePlayer::SetKeyTime(dTime); // KeyUp and KeyDown need to know what is the time

			if (dTime > nextTimeStepTime) {
				nextTimeStepTime += timeBetweenSteps;
				AdvanceSongOneStep();
			}

			double sample = clip(NotePlayer::GetSoundWaveAt(dTime), 1.0); // Just multiply it by 0.2 because too large
			//if (i < 3000)
			//	LOG(notePlayer.GetSoundWaveAt(dTime));
			int intSample = sample * maxAmplitude;
			writeFile(file, intSample, 2);
		}
		int postPos = file.tellp();

		file.seekp(prePos - 4);
		writeFile(file, postPos - prePos, 4);
		file.seekp(4, std::ios::beg);
		writeFile(file, postPos - 8, 4);

		file.close();
		activeNotesThisStep = {};
		notePlayer.ClearKeys();
		notePlayer.SetPause(false);
	}
	songStep = wasSongStep;
	songPlaying = wasSongPlaying;
	SetStepLinePos(songStep, songStepLine.GetComponent<UIAlignComponent>());
}
uint32_t SwapBytes32(uint32_t bytes) {
	return (((bytes >> 24) & 0xff) | ((bytes << 8) & 0xff0000) | ((bytes >> 8) & 0xff00) | ((bytes << 24) & 0xff000000));
}
uint16_t SwapBytes16(uint16_t bytes) {
	return ((bytes >> 8) | (bytes << 8));
}

void AppManager::SaveAsMidi(std::function<void()> doneCallback) {
	std::ofstream file;
	{
		int i = 1;
		stringstream ss;
		do {
			ss.str(std::string()); // Clear it
			ss << "Notes" << i << ".mid";
			i++;
		} while (std::filesystem::exists(ss.str()));
		file.open(ss.str(), std::ios::binary);
	}

	// Header chunk
	file << "MThd";
	writeFile(file, SwapBytes32(6), 4);
	writeFile(file, SwapBytes16(0), 2); // Format
	writeFile(file, SwapBytes16(1), 2); // Num of tracks
	writeFile(file, SwapBytes16(24), 2); // Default unit of delta-time
	//writeFile(file, 1, 1); // Default unit of delta-time
	//writeFile(file, 128, 1); // Default unit of delta-time

	// Track chunk
	file << "MTrk";
	file << "----";

	// Only on events are going to happen, since off evens are just on
	// evens with a velocity of 0. This way can use "Running status" for each timeStep
	std::array<bool, 96> noteArr = {};
	auto& data = useData1 ? imageData1 : imageData2;
	int time = 0;
	constexpr int addToIToGetNote = 20;
	bool writingFirstData = true;
	int prePos = file.tellp();

	// Time signature
	writeFile(file, 0  /*00*/, 1);
	writeFile(file, 255/*FF*/, 1);
	writeFile(file, 88 /*58*/, 1);
	writeFile(file, 4  /*04*/, 1);
	writeFile(file, 4  /*04*/, 1); // Time sig, up
	writeFile(file, 2  /*02*/, 1); // Time sig, down, represented as power of 2
	writeFile(file, 24 /*18*/, 1); // MIDI clocks per metronome tick, default
	writeFile(file, 8  /*08*/, 1); // num of 1/32 notes per 24 midi tick, default

	// Instrument change
	writeFile(file, 0  /*00*/, 1);
	writeFile(file, 192/*C0*/, 1);
	writeFile(file, 0  /*00*/, 1); // To instrument 0

	writeFile(file, 0,   1); // Timing byte comes before status, but the status is running. Add this here and then don't later
	writeFile(file, 144, 1); // "Note on" status byte on channel 0
	//*
	for (int step = 0; step < 16 * 96; step++) {
		// Check each note
		for (int i = 0; i < 96; i++) {
			float val = data[(uint64_t)step / 96 * 96 * 96 + (uint64_t)step % 96 * 96 + i];
			if (val > 0.9f) {
				if (!noteArr[i]) {
					noteArr[i] = true;

					// MSB check
					if (time > 127) {
						writeFile(file, time / 128 + 128/*8th bit*/, 1);
						time = time % 128;
					}

					if (!writingFirstData) writeFile(file, time, 1);
					else writingFirstData = false;
					writeFile(file, i+addToIToGetNote, 1); // Key
					writeFile(file, 64, 1); // Vel
					time = 0;
				}
			}
			else {
				if (noteArr[i]) {
					noteArr[i] = false;
					if (time > 127) LOG("Time out of range, implement MSbit check");
					if (!writingFirstData) writeFile(file, time, 1);
					else writingFirstData = false;
					writeFile(file, i+addToIToGetNote, 1); // Key
					writeFile(file, 0, 1); // Vel
					time = 0;
				}
			}
		}

		time += 1;
	}
	// Add all that remained on
	for (int i = 0; i < 96; i++)
		if (noteArr[i]) {
			if (time > 127) LOG("Time out of range, implement MSbit check");
			writeFile(file, time, 1);
			writeFile(file, i + addToIToGetNote, 1); // Key
			writeFile(file, 0, 1); // Vel
			time = 0;
		}

	// End of track
	writeFile(file, 0, 1);   // 00
	writeFile(file, 255, 1); // FF
	writeFile(file, 47, 1);  // 2F
	writeFile(file, 0, 1);   // 00


	int postPos = file.tellp();
	file.seekp(prePos - 4);
	uint32_t diff = postPos - prePos;
	writeFile(file, SwapBytes32(diff), 4);

	file.close();
}

#pragma endregion

AppManager::AppManager(Entity songStepLine) : songStepLine(songStepLine) {
	std::fstream in("Assets/vals.binary", std::ios::in | std::ios::binary);
	in.read((char*)valuesBuffer, valuesBufferSize*4);
	in.close();
}

void AppManager::OnCreate() {
	predictValues = std::vector<float>(120);
	RandomizePredictVals();

	notePlayer.Init();
}
void AppManager::OnUpdate() {
	if (!thread2Working) {
		// Update predict values
		for (auto& pair : setPredictValues)
			predictValues[pair.first] = pair.second * valuesBuffer[(/*evals start*/120 * 120 + 120) + pair.first]; // little bit of PCA
		setPredictValues.clear();

		if (needToPredict) {
			needToPredict = false;
			thread2Working = true;
			std::thread updater(std::bind(&AppManager::UpdateDataFromVector, this));
			updater.detach();
		}
		if (predictionDone) {
			predictionDone = false;
			UpdateTextures();
		}
	}

	static bool spacePressedLastFrame = false;
	bool spacePressed = Input::IsKeyPressed(EGL_KEY_SPACE);
	if (spacePressed && !spacePressedLastFrame) {
		songPlaying = !songPlaying;
		if (!songPlaying) {
			activeNotesThisStep = {};
			notePlayer.ClearKeys();
		}
	}
	spacePressedLastFrame = spacePressed;

	static bool rPressedLastFrame = false;
	bool rPressed = Input::IsKeyPressed(EGL_KEY_R);
	if (rPressed && !rPressedLastFrame) {
		songStep = 0;
		songStepLine.GetComponent<UIAlignComponent>().SetXPosValue(100);
		RandomizePredictVals();
	}
	rPressedLastFrame = rPressed;

	static bool sPressedLastFrame = false;
	bool sPressed = Input::IsKeyPressed(EGL_KEY_S);
	if (sPressed && !sPressedLastFrame)
		SaveAsMidi([]() { LOG("Yeah"); });
	sPressedLastFrame = sPressed;

	if (songPlaying) {
		static float timeToNextStep = 0;
		timeToNextStep -= Time::GetFrameDelta();
		while (timeToNextStep < 0) {
			timeToNextStep += timeBetweenSteps;
			AdvanceSongOneStep();
		}
	}
}

void AppManager::RandomizePredictVals() {
	for (int i = 0; i < 120; i++) {
		float val = Random::Float01Normal();
		if (i < sliderAmount) ((MusicScene*)GetParentScene())->ChangeUISliderValue(i, val);
		ChangePredictValue(i, val);
	}
	needToPredict = true;
}

void AppManager::ChangePredictValue(int index, float value) {
	setPredictValues[index] = value;
}

void AppManager::UpdateDataFromVector() {
	const auto result = model->model.predict({ fdeep::tensor(fdeep::tensor_shape(static_cast<std::size_t>(120)), GetPCAValues()) })[0];

	// Store the values in the vector that isnt active
	if (useData1) {
		imageData2 = result.to_vector();
		PreProcessData(imageData2, sensitivity);
	}
	else {
		imageData1 = result.to_vector();
		PreProcessData(imageData1, sensitivity);
	}

	useData1 = !useData1;
	thread2Working = false;
	predictionDone = true;
}
std::vector<float> AppManager::GetPCAValues()
{
	std::vector<float> final = std::vector<float>(120);

	// Dot
	for (int i = 0; i < 120; i++) {
		float sum = 0;
		for (int j = 0; j < 120; j++)
			sum += valuesBuffer[j * 120 + i] * predictValues[j];
		final[i] = sum;
	}

	for (int i = 0; i < 120; i++)
		final[i] += valuesBuffer[(/*means start*/120 * 120) + i];

	return final;
}
void AppManager::PreProcessData(std::vector<float>& data, float sens) {
	uint8_t minDuration = minDur;
	uint8_t maxGapToFuse = 3;

	for (int y = 0; y < 16 * 96; y++) {
		int stepsFromLastTurnOn = minDuration;
		for (int x = 0; x < 96; x++) {
			int index = (y / 96) * 96 * 96 + x * 96 + (y % 96);
			bool shouldBeNote = data[index] > sens;
			
			// Min duration 
			{
				if (stepsFromLastTurnOn < minDuration) {

					// If there already is a note, revert the last
					if (shouldBeNote && stepsFromLastTurnOn > maxGapToFuse)
						data[index-96] = 0;

					shouldBeNote = true;
					stepsFromLastTurnOn++;
				}
				// Is this a note
				else if (shouldBeNote) {
					// If the last note was extended, revert the last
					if (stepsFromLastTurnOn > maxGapToFuse && x != 0)
						data[index-96] = 0;

					stepsFromLastTurnOn = 1;
					shouldBeNote = true;
				}
			}

			data[index] = shouldBeNote ? 1 : 0;
		}
	}
}

void AppManager::UpdateTextures() {
	if (useData1)
		((MusicScene*)GetParentScene())->UpdateTextures(imageData1, sensitivity);
	else
		((MusicScene*)GetParentScene())->UpdateTextures(imageData2, sensitivity);
}

void AppManager::ValidateSongStatus() {
	auto& data = useData1 ? imageData1 : imageData2;
	for (int i = 0; i < 96; i++) {
		bool shouldBeState = data[songStep / 96 * 96 * 96 + songStep % 96 * 96 + i] > 0.9f;
		if (activeNotesThisStep[i] != shouldBeState) {
			if (shouldBeState)
				notePlayer.KeyDown(notePlayer.IndexToPitch(i));
			else
				notePlayer.KeyUp(notePlayer.IndexToPitch(i));
			activeNotesThisStep[i] = shouldBeState;
		}
	}
}

void AppManager::AdvanceSongOneStep() {
	auto& data = useData1 ? imageData1 : imageData2;
	SetStepLinePos(songStep, songStepLine.GetComponent<UIAlignComponent>());

	for (int i = 0; i < 96; i++) {
		float val = data[(uint64_t)songStep / 96 * 96 * 96 + (uint64_t)songStep % 96 * 96 + i];

		// The value is 1 if it needs to be played, else really low
		if (val > 0.9f) {
			if (!activeNotesThisStep[i]) {
				activeNotesThisStep[i] = true;
				notePlayer.KeyDown(notePlayer.IndexToPitch(i));
			}
		}
		else {
			if (activeNotesThisStep[i]) {
				activeNotesThisStep[i] = false;
				notePlayer.KeyUp(notePlayer.IndexToPitch(i));
			}
		}
	}

	songStep = (songStep + 1) % (96 * 16);
}


void MusicScene::UpdateTextures(const std::vector<float>& vecData, float sensitivity) {
	for (int i = 0; i < 16; i++) {
		uint32_t pixelAmount = 96*96;
		uint32_t dataSize = pixelAmount * 4;
		void* data = malloc(dataSize);
		uint8_t* ptr = (uint8_t*)data;
		for (int j = 0; j < pixelAmount; j++) {
			int x = j % textures[i]->GetWidth();
			int y = 96 - (j / textures[i]->GetHeight());
			float val = vecData[i * 96 * 96 + x * 96 + y];

			if (val > 0.9f) {
				*ptr = 255;
				ptr += 1;
				*ptr = 255;
				ptr += 1;
				*ptr = 255;
				ptr += 1;
			}
			else {
				*ptr = 0;
				ptr += 1;
				*ptr = 0;
				ptr += 1;
				*ptr = 0;
				ptr += 1;
			}
			*ptr = 255; // Alpha
			ptr += 1;
		}

		textures[i]->SetData(data, dataSize);
		free(data);
	}
}


//// Textures ////

Ref<Texture> MusicScene::CreateTexture(Entity& parent, UIEntityParams params, float pos) {
	Entity textureEntity = AddUIEntity(params, parent);
	textureEntity.GetComponent<UIAlignComponent>().SetXPosValue(pos);
	Ref<Texture> texture = Texture::Create(96, 96, false);
	textureEntity.AddComponent<SpriteRendererComponent>(texture);

	return texture;
}

void MusicScene::NoteSensitivityChanged(float val) {
	sensitivity = (1.0f - val) / 2 + 0.1f; // Scale between 0.1 and 0.6
	GetAppManager()->QueuePredictThread();
}
void MusicScene::PlaySpeedChanged(float val) {
	val += 0.001f; // No dividing by zero here
	GetAppManager()->SetPlaySpeed(1 / (val * 30) * 0.5f); // Value between 0.0166 - 0.5
}
void MusicScene::MinDurChanged(float val) {
	uint8_t intVal = (uint8_t)(val * 5) + 1; // 0 and 1 are essentially the same, so add one
	if (minDur != intVal) {
		minDur = intVal;
		GetAppManager()->QueuePredictThread();
	}
}
void MusicScene::VolumeChanged(float val) {
	volume = val;
	GetAppManager()->SetVolume(val);
}
void MusicScene::SetInstrument(int index) {
	GetAppManager()->SetInstrument((NotePlayer::SoundWaveType)index);
}
void MusicScene::SliderChanged(float val, int index) {
	GetAppManager()->ChangePredictValue(index, val);
	GetAppManager()->QueuePredictThread();
}

//// UI ////

Entity MusicScene::CreateSlider(Entity& parent, const glm::vec2& pos, int index) {
	Entity sliderBase = AddUIEntity("Slider", parent);
	sliderBase.AddComponent<SpriteRendererComponent>(GRAY_LIGHT);
	auto& baseTrans = sliderBase.GetComponent<UIAlignComponent>();
	baseTrans.SetXPosValue(pos.x);
	baseTrans.SetYPosValue(pos.y);
	baseTrans.SetWidthDriver(UIAlignComponent::WidthDriver::AspectWidth);
	baseTrans.SetWidthValue(0.15f);
	baseTrans.SetHeightValue(0.2f);

	Entity handle = AddUIEntity("Handle", sliderBase);
	auto& handleTrans = handle.GetComponent<UIAlignComponent>();
	handle.GetComponent<MetadataComponent>().subSorting = 1;
	handle.AddComponent<SpriteRendererComponent>(GRAY);
	handleTrans.SetWidthValue(1.5f);
	handleTrans.SetHeightDriver(UIAlignComponent::HeightDriver::AspectHeight);
	handleTrans.SetHeightValue(0.6f);

	const glm::vec2& posWolrd = handleTrans.GetWorldPosition();

	auto& comp = sliderBase.AddComponent<NativeScriptComponent>();
	comp.Bind<Slider>(handle, index, std::bind(&MusicScene::SliderChanged, this, std::placeholders::_1, std::placeholders::_2));

	sliderEntitites[index] = (Slider*)comp.baseInstance;

	return sliderBase;
}
Entity MusicScene::CreateFillBar(Entity& parent, const glm::vec2& pos, const glm::vec4& color, float startVal, void(MusicScene::*callback)(float)) {
	Entity rect = AddUIEntity("Fill bar", parent);
	Ref<Texture> tex = Texture::Create("Assets/Rect.png", false);
	rect.AddComponent<SpriteRendererComponent>(tex);
	UIAlignComponent& comp = rect.GetComponent<UIAlignComponent>();
	comp.SetWidthDriver(UIAlignComponent::WidthDriver::AspectWidth);
	comp.SetWidthValue(4);
	comp.SetHeightValue(0.1f);
	comp.SetXPosValue(pos.x);
	comp.SetYPosValue(pos.y);

	rect.AddComponent<NativeScriptComponent>().Bind<FillBar>(rect, startVal, color,
		std::bind(callback, this, std::placeholders::_1));

	return rect;
}
Entity MusicScene::CreateToggleGroup(Entity& parent, const glm::vec2& pos,
	const std::string& texturePath, const std::array<glm::vec4, 4>& colors, void(MusicScene::* callback)(int)) 
{
	constexpr size_t optionSize = 4;
	const size_t selected = 3;
	SetInstrument(selected);
	Entity group = AddUIEntity("Toggle group", parent);
	UIAlignComponent& comp = group.GetComponent<UIAlignComponent>();
	comp.SetWidthDriver(UIAlignComponent::WidthDriver::AspectWidth);
	comp.SetWidthValue(3.0f);
	comp.SetHeightValue(0.1f);
	comp.SetXPosValue(pos.x);
	comp.SetYPosValue(pos.y);

	std::array<RadioButton<optionSize>*, optionSize> scripts;
	std::array<Entity, optionSize> entities;

	UIEntityParams params = UIEntityParams("Button",
		(UIAlignComponent::Driver)UIAlignComponent::XDriver::AlignCenter | (UIAlignComponent::Driver)UIAlignComponent::WidthDriver::RelativeWidth,
		(UIAlignComponent::Driver)UIAlignComponent::YDriver::AlignCenter | (UIAlignComponent::Driver)UIAlignComponent::HeightDriver::RelativeHeight,
		0, 0, 0.3f, 0.7f, false, false);
	Ref<Texture> instrumentsTex = Texture::Create(texturePath);
	const glm::vec2 subTextures[optionSize] = {
		{ 0, 0 }, 
		{ 0, 1 },
		{ 1, 0 }, 
		{ 1, 1 }
	};

	for (int i = 0; i < optionSize; i++) {
		params.xPrimaryValue = ((float)i / (optionSize-1) - 0.5f);
		Entity button = AddUIEntity(params, group);
		Ref<SubTexture> instrument = SubTexture::CreateFromIndexes(instrumentsTex, subTextures[i], { 167, 116 });
		const glm::vec4& color = i == selected ? colors[i]-0.1f : colors[i];
		button.AddComponent<SpriteRendererComponent>(instrument, color);
		auto& comp = button.AddComponent<NativeScriptComponent>();
		comp.Bind<RadioButton<optionSize>>(i);
		scripts[i] = (RadioButton<optionSize>*)comp.baseInstance;
		entities[i] = button;
	}

	auto& baseScript = group.AddComponent<NativeScriptComponent>();
	baseScript.Bind<RadioGroup<optionSize>>(entities, selected, std::bind(callback, this, std::placeholders::_1));
	RadioGroup<optionSize>* groupScript = (RadioGroup<optionSize>*)baseScript.baseInstance;
	for (int i = 0; i < optionSize; i++)
		scripts[i]->SetGroup(groupScript);

	return group;
}
Entity MusicScene::CreateButton(Entity& parent, const glm::vec2& pos, const std::string& texturePath, const glm::vec4& color, void(MusicScene::* callback)()) {
	Entity rect = AddUIEntity("Button", parent);
	Ref<Texture> tex = Texture::Create(texturePath, false);
	rect.AddComponent<SpriteRendererComponent>(tex, color);
	UIAlignComponent& comp = rect.GetComponent<UIAlignComponent>();
	comp.SetWidthDriver(UIAlignComponent::WidthDriver::AspectWidth);
	comp.SetWidthValue(1.5f);
	comp.SetHeightValue(0.125f);
	comp.SetXPosValue(pos.x);
	comp.SetYPosValue(pos.y);

	rect.AddComponent<NativeScriptComponent>().Bind<Button>(std::bind(callback, this));

	return rect;
}

void MusicScene::CreateInterface() {

	//// Sliders ////
	Entity canvas = AddCanvas();
	for (int i = 0; i < sliderAmount; i++) {
		float x = ((float)i) * 0.05f - 0.45f;
		CreateSlider(canvas, { x, 0.375f }, i);
	}

	//// Textures ////
	const float size = 0.4f;
	UIEntityParams params;
	params.name = "Texture";
	params.xDrivers = (UIAlignComponent::Driver)UIAlignComponent::XDriver::AlignCenter | (UIAlignComponent::Driver)UIAlignComponent::WidthDriver::AspectWidth;
	params.yDrivers = (UIAlignComponent::Driver)UIAlignComponent::HeightDriver::RelativeHeight | (UIAlignComponent::Driver)UIAlignComponent::YDriver::AlignCenter;
	params.xSecondaryValue = 1;
	params.ySecondaryValue = size; // Texture size
	params.yPrimaryValue = 0.3f; // Texture Y pos;
	UIEntityParams paremtPrams("Textures", (UIAlignComponent::Driver)UIAlignComponent::XDriver::AlignCenter | (UIAlignComponent::Driver)UIAlignComponent::WidthDriver::AspectWidth,
		(UIAlignComponent::Driver)UIAlignComponent::YDriver::AlignCenter | (UIAlignComponent::Driver)UIAlignComponent::HeightDriver::RelativeHeight, 0, 0, size * 7, size, false, false);
	Entity texturesParent = AddUIEntity(paremtPrams, canvas);

	for (int i = 0; i < 8; i++)
		textures[i] = CreateTexture(texturesParent, params, ((float)i/7 - 0.5f));

	params.yPrimaryValue = -0.14f; // Texture Y pos;

	for (int i = 8; i < 16; i++)
		textures[i] = CreateTexture(texturesParent, params, ((float)(i-8) / 7 - 0.5f));

	UIEntityParams textParams = UIEntityParams("Text",
		(UIAlignComponent::Driver)UIAlignComponent::XDriver::AlignCenter | (UIAlignComponent::Driver)UIAlignComponent::WidthDriver::RelativeWidth,
		(UIAlignComponent::Driver)UIAlignComponent::YDriver::AlignCenter | (UIAlignComponent::Driver)UIAlignComponent::HeightDriver::RelativeHeight,
		0, -0.593f, 0.28f, 0.5f, false, false);
	Entity text = AddUIEntity(textParams, texturesParent);
	auto& textComp = text.AddComponent<TextComponent>();
	textComp.SetText("[Space] to Play [R] to Randomize");
	textComp.data.fontSize = 8;
	textComp.data.alignHorizontal = TextAlignHorizontal::Middle;
	textComp.data.alignVertical = TextAlignVertical::Middle;

	Entity line = AddUIEntity("StepLine", texturesParent);
	auto& align = line.GetComponent<UIAlignComponent>();
	line.GetComponent<MetadataComponent>().sortingLayer = 1;
	align.SetWidthDriver(UIAlignComponent::WidthDriver::ConstWidth);
	align.SetWidthValue(5);
	align.SetXPosValue(100); // Don't show until the song starts
	align.SetHeightValue(0.4f);
	line.AddComponent<SpriteRendererComponent>();

	appManagerEntity = AddEntity(EntityParams("AppManager"));
	appManagerEntity.AddComponent<NativeScriptComponent>().Bind<AppManager>(line);

	//// Option Sliders ////
	Entity sensitivitySlider = CreateFillBar(canvas, { -0.325f,-0.35f }, LIGHT_RED, 1.0f-((sensitivity)-0.1f)*2, &MusicScene::NoteSensitivityChanged);
	Entity minDurSlider = CreateFillBar(canvas, { -0.325f,-0.225f }, GRAY_LIGHT, 0.6f, &MusicScene::MinDurChanged);
	Entity speedSlider = CreateFillBar(canvas, { 0,-0.35f }, LIGHT_GREEN, playSpeedAtProgramStart, &MusicScene::PlaySpeedChanged);
	Entity volumeSlider = CreateFillBar(canvas, { 0.325f, -0.35f }, LIGHT_YELLOW, volume, &MusicScene::VolumeChanged);

	Entity instrumentToggle = CreateToggleGroup(canvas, { 0.325f, -0.225f }, "Assets/Instruments.png", 
		std::array<glm::vec4, 4>{ LIGHT_BLUE, LIGHT_BLUE, LIGHT_BLUE, LIGHT_BLUE }, &MusicScene::SetInstrument);

	Entity saveParent = AddUIEntity("Saving", canvas);
	auto& saveParentAlign = saveParent.GetComponent<UIAlignComponent>();
	saveParentAlign.SetXPosValue(0);
	saveParentAlign.SetWidthDriver(UIAlignComponent::WidthDriver::AspectWidth);
	saveParentAlign.SetWidthValue(0.296f);
	saveParentAlign.SetHeightValue(0.5f);
	saveParentAlign.SetYPosValue(-0.45f);
	Entity saveText = AddUIEntity(textParams, saveParent);
	auto& saveTextComp = saveText.AddComponent<TextComponent>();
	auto& textAlign = saveText.GetComponent<UIAlignComponent>();
	textAlign.SetYPosValue(-0.03f);
	textAlign.SetXPosValue(-0.785f);
	textAlign.SetWidthValue(1.162f);
	textAlign.SetHeightValue(0.5f);
	saveTextComp.SetText("Save as");
	saveTextComp.data.fontSize = 9;
	saveTextComp.data.alignHorizontal = TextAlignHorizontal::Middle;
	saveTextComp.data.alignVertical = TextAlignVertical::Middle;
	Entity midButton = CreateButton(saveParent, { 0.97f, 0 }, "Assets/Mid.png", GRAY_LIGHT, &MusicScene::SaveAsMidi);
	Entity wavButton = CreateButton(saveParent, { 0.18f, 0 }, "Assets/Wav.png", GRAY_LIGHT, &MusicScene::SaveAsWave);
}

void MusicScene::SceneBegin() {
	Entity camera = AddEntity(EntityParams("Camera"));
	camera.AddComponent<CameraComponent>().camera.SetSize(8.85f);
	camera.GetComponent<CameraComponent>().backgroundColor = GRAY_DARK;
	SetPrimaryCamera(camera);

	CreateInterface();
}
Ref<Scene> Egl::ApplicationStartup() {
	return CreateRef<MusicScene>();
}
void Egl::EngineInit() {
	std::string path = "Assets/";//"../Sandbox/src/fdeep/"; // 

	std::string shouldBePath = path + "decoder.json";
	if (!std::filesystem::exists(shouldBePath)) {
		std::cout << "Starting for the first time. Please don't close this window..." << std::endl;
		std::fstream part1, part2;
		rename((path + "decoder_part_1.json").c_str(), shouldBePath.c_str());
		part2.open(path + "decoder_part_2.json");
		part1.open(shouldBePath, std::ios::app);
		
		if (!part2)
			printf("\nError Occurred (Assets/decoder.json)!");
		else {
			char ch;
			while (part2 >> std::noskipws >> ch)
				part1 << ch;
			part1 << "\n";
			while (part2 >> noskipws >> ch)
				part1 << ch;
		}
		part1.close();
		part2.close();
		remove((path + "decoder_part_2.json").c_str());
	}
	model = new AI(fdeep::load_model(shouldBePath, true));
}