#pragma once
#include <Eagle.h>
using namespace Egl;


class Slider : public Script {
public:
	Slider(Entity handle, float index, std::function<void(float, int)> callback) : handle(handle), callback(callback), index(index) {}

	void SetValue(float val, UIAlignComponent& handleTrans);
	void SetValue(float val) { SetValue(val, handle.GetComponent<UIAlignComponent>()); }
	float GetValue() const { return value; }

	bool OnEvent(Event& e);
private:
	bool OnMouseMovePressed(MouseMovedEvent& e);
	void MoveToCursor(const glm::vec2& mousePos, UIAlignComponent& handleTrans);

	Entity handle;
	bool dragging = false;
	float value = 0;
	float index = -1;
	std::function<void(float, int)> callback;
};
class FillBar : public Script {
public:
	FillBar(Entity& thisEntity, float value, const glm::vec4& color, std::function<void(float)> callback);

	void SetValue(float val);
	bool OnEvent(Event& e);
private:
	bool FillBarValueHere(MouseMovedEvent& e);

	Entity fillAmount;
	float value = 0.5f;
	std::function<void(float)> callback;
};

class Button : public Script {
public:
	Button(std::function<void()> callback) : callback(callback) {}
	bool OnEvent(Event& e);
	void OnUpdate();
private:
	std::function<void()> callback;
	float fade = 1;
	glm::vec4 fadeTo = glm::vec4{ 223.0f / 255, 230.0f / 255, 233.0f / 255, 1 };
	glm::vec4 fadeFrom = glm::vec4{ 0 / 255, 184.0f / 255, 148.0f / 255, 1 };
};

template<size_t buttonsInGroup> class RadioButton;

template<size_t buttonsInGroup>
class RadioGroup : public Script {
public:
	RadioGroup(std::array<Entity, buttonsInGroup>& buttons, int selected, std::function<void(int)> callback)
		: buttons(buttons), activeIndex(selected), callback(callback) {};
	void ActivateButton(int index);
private:
	int activeIndex = 0;
	std::function<void(int)> callback;
	std::array<Entity, buttonsInGroup> buttons;
};
template<size_t buttonsInGroup>
class RadioButton : public Script {
public:
	RadioButton(int index) : thisIndex(index) {}
	bool OnEvent(Event& e);
	void SetGroup(RadioGroup<buttonsInGroup>* group) { this->group = group; }
private:
	int thisIndex = 0;
	RadioGroup<buttonsInGroup>* group;
};

template<size_t buttonsInGroup>
inline void RadioGroup<buttonsInGroup>::ActivateButton(int index) {
	buttons[activeIndex].GetComponent<SpriteRendererComponent>().color += 0.1f;
	activeIndex = index;
	buttons[activeIndex].GetComponent<SpriteRendererComponent>().color -= 0.1f;
	callback(index);
}

template<size_t buttonsInGroup>
inline bool RadioButton<buttonsInGroup>::OnEvent(Event& e) {
	if (e.GetEventType() == EventType::MouseButtonReleased)
		group->ActivateButton(thisIndex);
	return false;
}
