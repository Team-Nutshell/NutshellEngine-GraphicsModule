#pragma once
#include "common.h"

class AnimationSystem {
public:
	void init(NtshEngn::ECSInterface* ecs);

	void update(float dt, std::unordered_map<NtshEngn::Entity, InternalObject>& objects, std::vector<InternalMesh>& meshes, HostVisibleVulkanBuffer& jointTransformBuffer);

	void playAnimation(InternalObject* object, uint32_t animationIndex);
	void pauseAnimation(InternalObject* object);
	void stopAnimation(InternalObject* object);
	void setAnimationCurrentTime(InternalObject* object, float time);
	bool isAnimationPlaying(InternalObject* object, uint32_t animationIndex);

private:
	// Find previous animation keyframe
	uint32_t findPreviousAnimationKeyframe(float time, const std::vector<NtshEngn::AnimationChannelKeyframe>& keyframes);

private:
	std::unordered_map<InternalObject*, PlayingAnimation> m_playingAnimations;

	NtshEngn::ECSInterface* m_ecs;
};