#pragma once
#include "common.h"

class AnimationSystem {
public:
	void init(NtshEngn::ECSInterface* ecs);

	void update(float dt, std::unordered_map<NtshEngn::Entity, InternalObject>& objects, std::vector<InternalMesh>& meshes, HostVisibleVulkanBuffer& jointTransformBuffer);

	void playAnimation(InternalObject* object, uint32_t animationIndex, bool looping);
	void resumeAnimation(InternalObject* object);
	void pauseAnimation(InternalObject* object);
	void stopAnimation(InternalObject* object);
	uint32_t getPlayingAnimation(InternalObject* object);
	bool isAnimationPlaying(InternalObject* object, uint32_t animationIndex);
	void setAnimationCurrentTime(InternalObject* object, NtshEngn::Mesh* mesh, float newTime);
	float getAnimationCurrentTime(InternalObject* object);
	void setAnimationSpeed(InternalObject* object, float newSpeed);
	float getAnimationSpeed(InternalObject* object);

private:
	// Find previous animation keyframe
	uint32_t findPreviousAnimationKeyframe(float time, const std::vector<NtshEngn::AnimationChannelKeyframe>& keyframes);

private:
	std::unordered_map<InternalObject*, PlayingAnimation> m_playingAnimations;

	NtshEngn::ECSInterface* m_ecs;
};