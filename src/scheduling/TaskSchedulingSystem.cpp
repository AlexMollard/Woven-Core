#include "pch.hpp"

#include "core/Logger.hpp"
#include "TaskSchedulingSystem.hpp"

TaskSchedulingSystem::TaskSchedulingSystem()
{
}

TaskSchedulingSystem::~TaskSchedulingSystem()
{
}

bool TaskSchedulingSystem::Initialize()
{
	ZoneScopedN("TaskSchedulingSystem::Initialize");

	enki::TaskSchedulerConfig config;
	m_TaskScheduler.Initialize(config);

	uint32_t numThreads = m_TaskScheduler.GetNumTaskThreads();
	Logger::Info("Task Scheduler initialized with %u worker threads", numThreads);
	return true;
}

void TaskSchedulingSystem::Shutdown()
{
	ZoneScopedN("TaskSchedulingSystem::Shutdown");
	// TaskScheduler cleanup is handled in destructor
}
