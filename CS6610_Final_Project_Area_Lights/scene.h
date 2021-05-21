#pragma once

#include <glad/glad.h>
#include <vector>

#include "shader.h"
#include "model.h"
#include "polyLight.h"

using namespace std;

class Scene
{
public:
	vector<Shader> shaderList;
	vector<Model> modelList;
	vector<shared_ptr<AreaLight>> lightList;

	Scene(vector<Shader> shaders, vector<Model> models, vector<shared_ptr<AreaLight>> areaLights)
		: shaderList(shaders), modelList(models), lightList(areaLights)
	{ }
};
