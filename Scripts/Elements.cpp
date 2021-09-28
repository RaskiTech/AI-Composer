#include <Eagle.h>
#include "Elements.h"
using namespace Egl;
#define SMALLER_DISTANCE(posA, posB, distanceX, distanceY) glm::abs(posA.x - posB.x) < distanceX && glm::abs(posA.y - posB.y) < distanceY
#define EAGLE_BIND_EVENT_FUNC(x) std::bind(&Slider::x, this, std::placeholders::_1)

// Value between 0 and 1
void Slider::SetValue(float val, UIAlignComponent& handleTrans) {
	value = val;
	auto sliderUI = GetComponent<UIAlignComponent>();
	float scaleY = sliderUI.GetWorldScale().y;
	float posY = sliderUI.GetWorldPosition().y;
	float upper = posY + scaleY / 2;
	float under = posY - scaleY / 2;
	float worldYPos = under + (upper - under) * val;
	handleTrans.SetYPosValue(handleTrans.GetPrimaryYFromWorldPos(worldYPos));

	callback(val, index);
}

bool Slider::OnEvent(Event& e) {
	if (Input::IsMousePressed(0)) {
		EventDispatcher dispacher(e);
		dispacher.Dispatch<MouseMovedEvent>(std::bind(&Slider::OnMouseMovePressed, this, std::placeholders::_1));
	}
	return false;
}

bool Slider::OnMouseMovePressed(MouseMovedEvent& e) {
	UIAlignComponent& handleTrans = handle.GetComponent<UIAlignComponent>();

	const glm::vec2 mousePos = { e.GetPosX(), e.GetPosY() };
	const glm::vec2& handleWorld = handleTrans.GetWorldPosition();

	const glm::vec2 handlePos = WorldToScreenPos(handleWorld);

	if (dragging) {
		if (SMALLER_DISTANCE(mousePos, handlePos, 40, 50)) {
			MoveToCursor(mousePos, handleTrans);
			dragging = true;
			//return true;
		}
		else dragging = false;
	}
	else {
		if (SMALLER_DISTANCE(mousePos, handlePos, 20, 400)) {
			MoveToCursor(mousePos, handleTrans);
			dragging = true;
			//return true;
		}
	}
	return false;
}

void Slider::MoveToCursor(const glm::vec2& mousePos, UIAlignComponent& handleTrans) {
	auto sliderUI = GetComponent<UIAlignComponent>();
	float scaleY = sliderUI.GetWorldScale().y;
	float posY = sliderUI.GetWorldPosition().y;
	float upper = posY + scaleY / 2;
	float under = posY - scaleY / 2;
	float val = (ScreenToWorldPos(mousePos).y - under) / (upper - under);
	SetValue(glm::clamp<float>(val, 0, 1), handleTrans);
}

bool FillBar::OnEvent(Event& e) {
	if (Input::IsMousePressed(0)) {
		EventDispatcher d(e);
		d.Dispatch<MouseMovedEvent>(std::bind(&FillBar::FillBarValueHere, this, std::placeholders::_1));
	}
	return false;
}

bool FillBar::FillBarValueHere(MouseMovedEvent& e) {
	glm::vec2 mousePos = { e.GetPosX(), e.GetPosY() };
	UIAlignComponent& barTrans = GetComponent<UIAlignComponent>();
	float scaleX = barTrans.GetWorldScale().x;
	float posX = barTrans.GetWorldPosition().x;
	float leftPos = posX - scaleX / 2;
	float rightPos = posX + scaleX / 2;
	SetValue((ScreenToWorldPos(mousePos).x - leftPos) / (rightPos - leftPos));

	return false;
}
FillBar::FillBar(Entity& thisEntity, float value, const glm::vec4& color, std::function<void(float)> callback) : value(value), callback(callback) {
	fillAmount = thisEntity.GetParentScene()->AddUIEntity("Fill", thisEntity);
	fillAmount.AddComponent<SpriteRendererComponent>(color);

	UIAlignComponent& fillAlign = fillAmount.GetComponent<UIAlignComponent>();
	fillAlign.SetUseSidesHorizontal(true);
	fillAlign.SetLeftSideDriver(UIAlignComponent::LeftSideDriver::ConstOffset);
	fillAlign.SetLeftSideValue(0);
	fillAlign.SetRightSideDriver(UIAlignComponent::RightSideDriver::RelativeOffset);
	fillAlign.SetRightSideValue(1-value);
	fillAlign.SetHeightValue(1);
}
void FillBar::SetValue(float val) {
	value = val;
	fillAmount.GetComponent<UIAlignComponent>().SetRightSideValue(1-value);
	callback(val);
}

bool Button::OnEvent(Event& e) {
	if (e.GetEventType() != EventType::MouseButtonPressed)
		return false;

	callback();
	GetComponent<SpriteRendererComponent>().color = fadeFrom;
	fade = 0;

	return false;
}

void Button::OnUpdate() {
	if (fade < 1) {
		fade += Time::GetFrameDelta();
		GetComponent<SpriteRendererComponent>().color = glm::mix(fadeFrom, fadeTo, fade);
	}
}
