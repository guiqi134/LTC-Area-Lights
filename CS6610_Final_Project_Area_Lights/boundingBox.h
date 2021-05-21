#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <limits>
#include <vector>

#include "shader.h"

const double infinity = std::numeric_limits<double>::infinity();

class boundingBox
{
public:
	// bounding points
	glm::vec3 minPos;
	glm::vec3 maxPos;

	boundingBox()
	{
		minPos = glm::vec3(-infinity, -infinity, -infinity);
		maxPos = glm::vec3(infinity, infinity, infinity);
		setBoundingBox();
	}

	boundingBox(glm::vec3 minP, glm::vec3 maxP) : minPos(minP), maxPos(maxP) { setBoundingBox(); }

	// set OpenGL bindings
	void setBoundingBox()
	{
		// 8 vetexes in box
		std::vector<glm::vec3> boxVertex = {
			glm::vec3(minPos.x, minPos.y, maxPos.z),
			glm::vec3(maxPos.x, minPos.y, maxPos.z),
			glm::vec3(maxPos.x, maxPos.y, maxPos.z),
			glm::vec3(minPos.x, maxPos.y, maxPos.z),
			glm::vec3(minPos.x, minPos.y, minPos.z),
			glm::vec3(maxPos.x, minPos.y, minPos.z),
			glm::vec3(maxPos.x, maxPos.y, minPos.z),
			glm::vec3(minPos.x, maxPos.y, minPos.z),
		};

		// 12 lines
		GLuint indices[] = {
			0, 1, 1, 2, 2, 3,
			3, 0, 4, 5, 5, 6,
			6, 7, 7, 4, 0, 4,
			1, 5, 2, 6, 3, 7
		};

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(GLfloat), boxVertex.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 24 * sizeof(GLuint), indices, GL_STATIC_DRAW);

		// position attribute
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);

		glBindVertexArray(0);
	}

	// get the center of the box
	glm::vec3 getCenter() const
	{
		return 0.5f * (minPos + maxPos);
	}

	void draw()
	{
		glBindVertexArray(VAO);
		glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

private:
	// render data
	GLuint VAO, VBO, EBO;
};