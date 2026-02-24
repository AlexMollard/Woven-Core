#pragma once

#include "pch.hpp"

class TaskSchedulingSystem
{
public:
	TaskSchedulingSystem();
	~TaskSchedulingSystem();

	bool Initialize();
	void Shutdown();

	enki::TaskScheduler* GetScheduler()
	{
		return &m_TaskScheduler;
	}

	uint32_t GetWorkerThreadCount() const
	{
		return m_TaskScheduler.GetNumTaskThreads();
	}

	void WaitAll()
	{
		m_TaskScheduler.WaitforAll();
	}

private:
	enki::TaskScheduler m_TaskScheduler;
};
