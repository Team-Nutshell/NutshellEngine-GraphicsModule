#pragma once
#include "common.h"

class AnimationSystem {
public:
	void init(NtshEngn::ECSInterface* ecs);

	void update(float dt, std::unordered_map<NtshEngn::Entity, InternalObject>& objects, std::vector<InternalMesh>& meshes, HostVisibleVulkanBuffer& jointTransformBuffer);

	void playAnimation(const InternalObject& object, NtshEngn::Animation* animation, bool looping);
	void resumeAnimation(const InternalObject& object);
	void pauseAnimation(const InternalObject& object);
	void stopAnimation(const InternalObject& object);
	NtshEngn::Animation* getPlayingAnimation(const InternalObject& object);
	bool isAnimationPlaying(const InternalObject& object, NtshEngn::Animation* animation);
	void setAnimationCurrentTime(const InternalObject& object, float newTime);
	float getAnimationCurrentTime(const InternalObject& object);
	void setAnimationSpeed(const InternalObject& object, float newSpeed);
	float getAnimationSpeed(const InternalObject& object);

private:
	// Find previous animation keyframe
	uint32_t findPreviousAnimationKeyframe(float time, const std::vector<NtshEngn::AnimationChannelKeyframe>& keyframes);

private:
	std::unordered_map<const InternalObject*, PlayingAnimation> m_playingAnimations;

	NtshEngn::ECSInterface* m_ecs;
};