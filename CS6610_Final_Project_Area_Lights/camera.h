#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const GLfloat YAW = 90.0f;
const GLfloat PITCH = 30.0f;
const GLfloat RADIUS = 6.0f;

class Camera
{
public:
	// camera attributes
	glm::vec3 position;
	glm::vec3 u;
	glm::vec3 v;
	glm::vec3 w;
	glm::vec3 worldUp;
	// camera transformation
	GLfloat pitch;
	GLfloat yaw;
	GLfloat radius;
	glm::vec3 center;
	// camera options
	const GLfloat MOUSE_SENSITIVITY = 0.05f;

	Camera() : worldUp(glm::vec3(0.0f, 1.0f, 0.0f)), yaw(YAW), pitch(PITCH), radius(RADIUS), center(glm::vec3(0.0f))
	{
		updateCameraVectors();
	}

	Camera(GLfloat yaw, GLfloat pitch, GLfloat radius, glm::vec3 center)
		: worldUp(glm::vec3(0.0f, 1.0f, 0.0f)), yaw(yaw), pitch(pitch), radius(radius), center(center)
	{
		updateCameraVectors();
	}

	Camera& operator=(const Camera& camera)
	{
		position = camera.position;
		u = camera.u;
		v = camera.v;
		w = camera.w;
		worldUp = camera.worldUp;
		pitch = camera.pitch;
		yaw = camera.yaw;
		center = camera.center;

		return *this;
	}

	glm::mat4 getViewMatrix()
	{
		return glm::lookAt(position, position + w, v);
	}

	void processRotation(GLfloat offsetX, GLfloat offsetY)
	{
		offsetX *= MOUSE_SENSITIVITY;
		offsetY *= MOUSE_SENSITIVITY;
		yaw += offsetX;
		pitch -= offsetY;

		if (pitch > 89.9f)
			pitch = 89.9f;
		if (pitch < -89.9f)
			pitch = -89.9f;
		updateCameraVectors();
	}

	void processZooming(GLfloat offsetY)
	{
		radius -= offsetY * MOUSE_SENSITIVITY * 0.1f;
		if (radius < 1.0f)
			radius = 1.0f;
		if (radius > 100.0f)
			radius = 100.0f;
		updateCameraVectors();
	}

	void updateCameraVectors()
	{
		glm::vec3 pos;
		pos.x = radius * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		pos.y = radius * sin(glm::radians(pitch));
		pos.z = radius * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		position = pos + center;
		w = glm::normalize(-position);
		u = glm::normalize(glm::cross(w, worldUp));
		v = glm::normalize(glm::cross(u, w));
	}
};