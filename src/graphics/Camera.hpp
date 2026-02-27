#pragma once

#include "pch.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
	Camera()
	{
		UpdateViewMatrix();
		UpdateProjectionMatrix();
	}

	void SetPosition(const glm::vec3& position)
	{
		m_Position = position;
		UpdateViewMatrix();
	}

	void SetTarget(const glm::vec3& target)
	{
		m_Target = target;
		UpdateViewMatrix();
	}

	void SetUp(const glm::vec3& up)
	{
		m_Up = up;
		UpdateViewMatrix();
	}

	void SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane)
	{
		m_FovDegrees = fovDegrees;
		m_AspectRatio = aspectRatio;
		m_NearPlane = nearPlane;
		m_FarPlane = farPlane;
		UpdateProjectionMatrix();
	}

	void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
	{
		m_Projection = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
	}

	const glm::mat4& GetViewMatrix() const
	{
		return m_View;
	}

	const glm::mat4& GetProjectionMatrix() const
	{
		return m_Projection;
	}

	glm::mat4 GetViewProjectionMatrix() const
	{
		return m_Projection * m_View;
	}

	const glm::vec3& GetPosition() const
	{
		return m_Position;
	}

	const glm::vec3& GetTarget() const
	{
		return m_Target;
	}

	const glm::vec3& GetUp() const
	{
		return m_Up;
	}

	float GetFov() const
	{
		return m_FovDegrees;
	}

	float GetAspectRatio() const
	{
		return m_AspectRatio;
	}

	float GetNearPlane() const
	{
		return m_NearPlane;
	}

	float GetFarPlane() const
	{
		return m_FarPlane;
	}

private:
	void UpdateViewMatrix()
	{
		m_View = glm::lookAt(m_Position, m_Target, m_Up);
	}

	void UpdateProjectionMatrix()
	{
		// Vulkan NDC: x: [-1, 1], y: [-1, 1] (top-to-bottom), z: [0, 1]
		// GLM_FORCE_DEPTH_ZERO_TO_ONE handles z
		// GLM_FORCE_LEFT_HANDED uses left-handed coordinate system
		m_Projection = glm::perspective(glm::radians(m_FovDegrees), m_AspectRatio, m_NearPlane, m_FarPlane);
		// Vulkan's Y is inverted compared to OpenGL, flip it
		m_Projection[1][1] *= -1.0f;
	}

	glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 m_Target = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);

	float m_FovDegrees = 60.0f;
	float m_AspectRatio = 16.0f / 9.0f;
	float m_NearPlane = 0.1f;
	float m_FarPlane = 1000.0f;

	glm::mat4 m_View = glm::mat4(1.0f);
	glm::mat4 m_Projection = glm::mat4(1.0f);
};
