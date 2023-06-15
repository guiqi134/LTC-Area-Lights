#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>

#include "shader.h"
#include "camera.h"
#include "model.h"
#include "LTC.h" // LTC1 and LTC2 
#include "polyLight.h"
#include "GUI.h"

const GLuint SCR_WIDTH = 1600;
const GLuint SCR_HEIGHT = 900;
const GLuint TEXTURE_WIDTH = 1185;
const GLuint TEXTURE_HEIGHT = 860;
const char* GLSL_VERSION = "#version 460";
const GLfloat PLANE_SCALER = 30.0f;

// camera object
Camera camera;
GLfloat lastX = TEXTURE_WIDTH / 2.0f, lastY = TEXTURE_HEIGHT / 2.0f;
GLboolean firstMouse = true;

// light object
glm::vec3 lightCenter(0.0f, 1.5f, 0.0f);
glm::vec3 lightColor(1.0f);
glm::vec3 lightDirX(1.0f, 0.0f, 0.0f);
glm::vec3 lightDirY(0.0f, 1.0f, 0.0f);
glm::vec3 lightDirZ(0.0f, 0.0f, 1.0f);
// initalize different area lights
shared_ptr<RectDiskLight> rectLight = make_shared<RectDiskLight>(
	LightType::Rectangle, lightColor, lightCenter, 4.0f, lightDirX, lightDirY, 0.5f, 0.5f
);
shared_ptr<CylinderLight> cylinderLight = make_shared<CylinderLight>(
	lightColor, glm::vec3(0.0f, 0.3f, 0.0f), 10.0f, glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 0.02f
);
shared_ptr<RectDiskLight> diskLight = make_shared<RectDiskLight>(
	LightType::Disk, lightColor, lightCenter, 4.0f, lightDirX, lightDirY, 0.5f, 0.5f
);
shared_ptr<SphereLight> sphereLight = make_shared<SphereLight>(
	lightColor, lightCenter, 10.0f, lightDirX, lightDirY, lightDirZ, 0.4f, 0.4f, 0.4f
);
// initalize scene1 and scene2 light list
vector<shared_ptr<AreaLight>> areaLights1 = { rectLight, cylinderLight, diskLight, sphereLight };
vector<shared_ptr<AreaLight>> areaLights2 = { sphereLight, rectLight, diskLight, cylinderLight };

// moving sphere light object
struct MovingSphereLight
{
	SphereLight sphereLight;
	glm::vec3 initCenter;
	glm::vec3 movingDir;
	GLfloat movingSpeed;
	GLfloat bounceHeight;
	GLint countBounceHit;
	GLfloat t;
	GLfloat s;
};

// material object
glm::vec3 diffuse(1.0f);
glm::vec3 specular(0.23f);
GLfloat roughness(0.25f);
struct Material
{
	glm::vec3 diffuse;
	glm::vec3 specular;
	GLfloat roughness;
} GGXMaterial{ diffuse, specular, roughness };

// texture object
struct TextureMap
{
	string name;
	GLuint id;
};

// scene constrol
GLint scene = 0; 

GLfloat deltaTime = 0.0f, lastTime = 0.0f;
GLboolean shouldReloadShader = false;
GLfloat rotY = 0.0f, rotZ = 0.0f;

using namespace std;

inline GLfloat random(GLfloat min, GLfloat max)
{
	return min + (max - min) * (rand() / (RAND_MAX + 1.0f));
}

GLuint setLTCTexture(const float* LTC)
{
	GLuint LTCTexMap;
	glGenTextures(1, &LTCTexMap);
	glBindTexture(GL_TEXTURE_2D, LTCTexMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_FLOAT, LTC);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return LTCTexMap;
}

TextureMap loadTexture(const char* path, string typeName)
{
	GLuint textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format{};
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return TextureMap{ typeName, textureID };
}

void useDefault()
{
	// reset lights
	rectLight = make_shared<RectDiskLight>(
		LightType::Rectangle, glm::vec3(1.0f), lightCenter, 4.0f, lightDirX, lightDirY, 0.5f, 0.5f
	);
	cylinderLight = make_shared<CylinderLight>(
		glm::vec3(1.0f), glm::vec3(0.0f, 0.3f, 0.0f), 10.0f, glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 0.02f
	);
	diskLight = make_shared<RectDiskLight>(
		LightType::Disk, glm::vec3(1.0f), lightCenter, 4.0f, lightDirX, lightDirY, 0.5f, 0.5f
	);
	sphereLight = make_shared<SphereLight>(
		glm::vec3(1.0f), lightCenter, 10.0f, lightDirX, lightDirY, lightDirZ, 0.4f, 0.4f, 0.4f
	);

	areaLights1 = { rectLight, cylinderLight, diskLight, sphereLight };
	areaLights2 = { sphereLight, rectLight, diskLight, cylinderLight };

	// reset material
	GGXMaterial = { diffuse, specular, roughness };

	// reset rotation
	rotY = 0.0f;
	rotZ = 0.0f;
}

void createFBO(GLuint& framebuffer, GLuint& renderedTex)
{
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glGenTextures(1, &renderedTex);
	glBindTexture(GL_TEXTURE_2D, renderedTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTex, 0);

	GLuint renderbuffer;
	glGenRenderbuffers(1, &renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, TEXTURE_WIDTH, TEXTURE_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		cout << "ERROR::FRAMEBUFFER: Framebuffer is not complete!" << endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow* window, int key, int scancodes, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (key == GLFW_KEY_F5 && action == GLFW_PRESS)
		shouldReloadShader = true;
}

void cursor_pos_callback(GLFWwindow* window, double xPos, double yPos)
{
	// avoid sudden change
	if (firstMouse)
	{
		lastX = xPos;
		lastY = yPos;
		firstMouse = false;
	}
	GLfloat offsetX = xPos - lastX;
	GLfloat offsetY = lastY - yPos;
	lastX = xPos;
	lastY = yPos;

	// adjust camera
	if (xPos <= 1200.0f && yPos <= 900.0f)
	{
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
		{
			camera.processRotation(offsetX, offsetY);
		}

		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
		{
			camera.processZooming(offsetY);
		}
	}
}

int main(int argc, char* args[])
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Area Light Implementation", NULL, NULL);
	if (window == NULL)
	{
		cout << "Fail to create glfw window\n";
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(0); // v-sync off
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		cout << "Fail to load GLAD\n";
		return -1;
	}

	// openGL configuration
	// -----------------------------------------------------
	glEnable(GL_DEPTH_TEST);
	glPatchParameteri(GL_PATCH_VERTICES, 4); // 4 control points

	// setup Dear ImGui 
	// -----------------------------------------------------
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = "editorConfig.ini";

	// setup Dear ImGui style
	ImGui::StyleColorsDark();

	// load custom font
	io.Fonts->AddFontFromFileTTF("resources/fonts/trebucbd.ttf", 16.0f);

	// setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(GLSL_VERSION);

	// setup window flags
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoResize;


	// load shaders
	// -----------------------------------------------------
	Shader shader;
	Shader rectShader("ltc.vert", "ltcRect.frag");
	Shader cylinderShader("ltc.vert", "ltcCylinder.frag");
	Shader diskShader("ltc.vert", "ltcDisk.frag");
	vector<Shader> areaLightShaders = { rectShader, cylinderShader, diskShader, diskShader };
	Shader polyLightShader("polyLight.vert", "polyLight.frag");

	Shader ltcAllShader("ltcAll.vert", "ltcAll.frag", nullptr, "ltcAll.tesc", "ltcAll.tese"); // scene2

	// load models
	// -----------------------------------------------------
	Model quadModel = Model("resources/models/quad.obj");
	Model tessQuadModel = Model("resources/models/quad.obj"); // use for tessellation plane
	Model cylinderModel = Model("resources/models/cylinder.obj");
	Model diskModel = Model("resources/models/disk.obj");
	Model sphereModel = Model("resources/models/sphere.obj");
	vector<Model> areaLightModels1 = { quadModel, cylinderModel, diskModel, sphereModel };
	vector<Model> areaLightModels2 = { sphereModel, quadModel, diskModel, cylinderModel };

	// load plane textures
	// TODO: using tessellation to improve mapping quality
	TextureMap stoneDiffuseMap = loadTexture("resources/textures/tex1/PavingStones_Color.jpg", "diffuse");
	TextureMap stoneNormalMap = loadTexture("resources/textures/tex1/PavingStones_Normal.jpg", "normal");
	TextureMap stoneRoughnessMap = loadTexture("resources/textures/tex1/PavingStones_Roughness.jpg", "roughness");
	TextureMap stoneAOMap = loadTexture("resources/textures/tex1/PavingStones_AmbientOcclusion.jpg", "AO");
	TextureMap stoneDispMap = loadTexture("resources/textures/tex1/PavingStones_Displacement.jpg", "displacement");

	TextureMap marbleDiffuseMap = loadTexture("resources/textures/tex2/Marble_Color.jpg", "diffuse");
	TextureMap marbleNormalMap = loadTexture("resources/textures/tex2/Marble_Normal.jpg", "normal");
	TextureMap marbleRoughnessMap = loadTexture("resources/textures/tex2/Marble_Roughness.jpg", "roughness");
	TextureMap marbleDispMap = loadTexture("resources/textures/tex2/Marble_Disp.jpg", "displacement");

	TextureMap woodFloorDiffuseMap = loadTexture("resources/textures/tex3/WoodFloor_Color.jpg", "diffuse");
	TextureMap woodFloorNormalMap = loadTexture("resources/textures/tex3/WoodFloor_Normal.jpg", "normal");
	TextureMap woodFloorRoughnessMap = loadTexture("resources/textures/tex3/WoodFloor_Roughness.jpg", "roughness");
	TextureMap woodFloorAOMap = loadTexture("resources/textures/tex3/WoodFloor_AmbientOcclusion.jpg", "AO");
	TextureMap woodFloorDispMap = loadTexture("resources/textures/tex3/WoodFloor_Displacement.jpg", "displacement");

	TextureMap diamondDiffuseMap = loadTexture("resources/textures/tex4/DiamondPlate_Color.jpg", "diffuse");
	TextureMap diamondNormalMap = loadTexture("resources/textures/tex4/DiamondPlate_Normal.jpg", "normal");
	TextureMap diamondRoughnessMap = loadTexture("resources/textures/tex4/DiamondPlate_Roughness.jpg", "roughness");
	TextureMap diamondMetalMap = loadTexture("resources/textures/tex4/DiamondPlate_Metalness.jpg", "metallic");
	TextureMap diamondDispMap = loadTexture("resources/textures/tex4/DiamondPlate_Displacement.jpg", "displacement");

	vector<vector<TextureMap>> texMapList = {
		{ }, 
		{ stoneDiffuseMap, stoneNormalMap, stoneRoughnessMap, stoneAOMap, stoneDispMap },
		{ marbleDiffuseMap, marbleNormalMap, marbleRoughnessMap, marbleDispMap },
		{ woodFloorDiffuseMap, woodFloorNormalMap, woodFloorRoughnessMap, woodFloorAOMap, woodFloorDispMap},
		{ diamondDiffuseMap, diamondNormalMap, diamondRoughnessMap, diamondMetalMap, diamondDispMap }
	};

	// create ltc1 and ltc2 texture 
	GLuint LTC1TexMap = setLTCTexture(LTC1);
	GLuint LTC2TexMap = setLTCTexture(LTC2);

	// set FBO
	GLuint framebuffer, renderedTex;
	createFBO(framebuffer, renderedTex);

	// set tessellation plane
	GLuint tessQuadVAO = tessQuadModel.meshes[0].VAO;
	GLuint tessQuadVBO = tessQuadModel.meshes[0].VBO;
	auto tessQuadVertices = tessQuadModel.meshes[0].vertices;
	vector<Vertex> newVertices = {
		tessQuadVertices[2], tessQuadVertices[0], tessQuadVertices[3], tessQuadVertices[1]
	};
	glBindVertexArray(tessQuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, tessQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)* newVertices.size(), newVertices.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);

	// ImGui demo setting
	bool show_demo_window = false;

	// scene1 per-frame variables
	auto areaLightModels = areaLightModels1;
	auto areaLights = areaLights1;
	auto areaLight = areaLights[0];
	auto lightIndex = static_cast<int>(areaLight->type);
	shader = areaLightShaders[lightIndex];
	auto modelScaler = glm::vec3(1.0f);

	// scene2 variables
	time_t randomSeed = time(0);
	GLint planeType = 0;
	auto textureMaps = texMapList[planeType];
	auto numSmallSphereLight = 0;
	auto cameraRotation = 90.0f;
	//if (scene == 1)
	//{
	//	shader = ltcAllShader;
	//	areaLightModels = areaLightModels2;
	//	areaLights = areaLights2;
	//}

	vector<MovingSphereLight> movingSphereLights;
	for (int i = 0; i < 100; i++)
	{
		auto r = random(0.3f, 0.4f);
		auto initCenter = glm::vec3(random(-25.0f, 25.0f), r + 0.1f, random(-25.0f, 25.0f));
		auto movingDir = glm::normalize(glm::vec3(random(-1.0f, 1.0f), 0.0f, random(-1.0f, 1.0f)));
		auto movingSpeed = random(1.0f, 2.0f);
		auto bounceHeight = random(1.0f, 2.0f);
		auto color = glm::vec3(random(0.3f, 1.0f), random(0.3f, 1.0f), random(0.3f, 1.0f));
		auto intensity = random(1.0f, 5.0f);

		movingSphereLights.push_back({
			SphereLight(color, initCenter, intensity, lightDirX, lightDirY, lightDirZ, r, r, r),
			initCenter, movingDir, movingSpeed, bounceHeight, 0, 0.0f, 0.0f
		});
	}


	// FPS 
	GLfloat accuTime = 0.0f;
	GLint numFrames = 0;
	GLint FPS = 0;

	// shader pre-configuration
	// -----------------------------------------------------
	shader.use();
	shader.setInt("LTC1", 0);
	shader.setInt("LTC2", 1);

	while (!glfwWindowShouldClose(window))
	{
		// TODO: F5 reload shader
		// -----------------------------------------------------

		// per-frame animation
		GLfloat currentTime = glfwGetTime();
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;
		accuTime += deltaTime;
		numFrames++;

		if (accuTime >= 1.0f)
		{
			FPS = numFrames;
			accuTime -= 1.0f;
			numFrames = 0;
		}

		// poll and handle events
		glfwPollEvents();

		// start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();


		// Window1: configuration
		// -----------------------------------------------------
		{

			ImGui::Begin("Configuration", NULL, window_flags);

			ImGui::Text("FPS: %u (%.2f ms/frame)", FPS, (GLfloat)deltaTime * 1000.0f);
			ImGui::Text("");

			ImGui::Text("Scenes");
			{
				auto prevScene = scene;
 				ImGui::RadioButton("Scene1", &scene, 0); 
				ImGui::SameLine();
				ImGui::RadioButton("Scene2", &scene, 1);

				if (prevScene != scene)
				{
					useDefault();
					if (scene == 0)
					{
						shader = areaLightShaders[lightIndex];
						areaLightModels = areaLightModels1;
						areaLights = areaLights1;
						areaLight = areaLights[lightIndex];
						
						camera.yaw = 90.0f;
						camera.pitch = 30.0f;
						camera.radius = 6.0f;
						camera.center = glm::vec3(0.0f);
						camera.updateCameraVectors();
					}
					else
					{
						shader = ltcAllShader;
						areaLightModels = areaLightModels2;
						areaLights = areaLights2;

						camera.yaw = 90.0f;
						camera.pitch = 30.0f;
						camera.radius = 35.0f;
						camera.center = glm::vec3(0.0f, 0.0f, 0.0f);
						camera.updateCameraVectors();
					}
				}
			}
			ImGui::Text("");
			
			if (scene == 0) // scene1 GUI
			{
				ImGui::Text("Area light type");
				{
					auto prevIndex = lightIndex;
					const char* types[] = { "Rectangle", "Cylinder", "Disk", "Sphere" };
					ImGui::Combo("Light Types", &lightIndex, types, IM_ARRAYSIZE(types));
				
					// update per-frame variables based on current light type 
					if (prevIndex != lightIndex)
					{
						cout << "Call in Area light type" << endl;
						useDefault();
						shader = areaLightShaders[lightIndex];
						areaLights = areaLights1;
						areaLight = areaLights[lightIndex];
					}
				}
				ImGui::Text("");

				ImGui::Text("GGX BRDF material properties");
				{
					ImGui::SliderFloat("Roughness", &GGXMaterial.roughness, 0.08f, 1.0f, "%.3f");
					ImGui::ColorEdit3("Diffuse Color", glm::value_ptr(GGXMaterial.diffuse));
					ImGui::ColorEdit3("Specular Color", glm::value_ptr(GGXMaterial.specular));
				}
				ImGui::Text("");

				ImGui::Text("Light properties");
				{
					ImGui::ColorEdit3("Light Color", glm::value_ptr(areaLight->color));
					ImGui::SliderFloat("Intensity", &(areaLight->intensity), 0.0f, 100.0f, "%.1f");
					ImGui::SliderFloat("Rotation Y", &rotY, 0.0f, 360.0f, "%.0f");
					ImGui::SliderFloat("Rotation Z", &rotZ, 0.0f, 360.0f, "%.0f");

					auto rotYZMatrix = glm::mat4(1.0f);
					rotYZMatrix = glm::rotate(rotYZMatrix, glm::radians(rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
					rotYZMatrix = glm::rotate(rotYZMatrix, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));

					// set some stuffs based on current light type
					if (areaLight->type == LightType::Rectangle || areaLight->type == LightType::Disk)
					{
						static bool twoSided = false;
						auto currentLight = dynamic_pointer_cast<RectDiskLight>(areaLight);
						ImGui::SliderFloat("Width", &currentLight->halfX, 0.01f, 40.0f, "%.2f");
						ImGui::SliderFloat("Height", &currentLight->halfY, 0.01f, 40.0f, "%.2f");
						ImGui::Checkbox("TwoSided", &twoSided);

						// update dirX and dirY in light
						currentLight->dirX = glm::vec3(rotYZMatrix * glm::vec4(lightDirX, 1.0f));
						currentLight->dirY = glm::vec3(rotYZMatrix * glm::vec4(lightDirY, 1.0f));

						// shader configuration
						shader.use();
						shader.setBool("twoSided", twoSided);

						// set scale factors for drawing light object
						modelScaler = glm::vec3(currentLight->halfX, currentLight->halfY, 1.0f);
					}
					else if (areaLight->type == LightType::Cylinder)
					{
						static bool analytic = true;
						static bool endCaps = false;
						auto currentLight = dynamic_pointer_cast<CylinderLight>(areaLight);
						ImGui::SliderFloat("Length", &currentLight->length, 0.01f, 1.0f, "%.2f");
						ImGui::SliderFloat("Radius", &currentLight->radius, 0.01f, 1.0f, "%.2f");
						ImGui::Checkbox("Analytic", &analytic);
						ImGui::Checkbox("EndCaps", &endCaps);

						// update tangent
						currentLight->tangent = glm::vec3(rotYZMatrix * glm::vec4(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f));
						//currentLight->updatePoints();

						// shader configuration
						shader.use();
						shader.setFloat("light.radius", currentLight->radius);
						shader.setBool("analytic", analytic);
						shader.setBool("endCaps", endCaps);

						// set scale factors for drawing light object
						modelScaler = glm::vec3(currentLight->length / 2.0f, currentLight->radius, currentLight->radius);
					}
					else if (areaLight->type == LightType::Sphere)
					{
						static bool twoSided = false;
						auto currentLight = dynamic_pointer_cast<SphereLight>(areaLight);
						ImGui::SliderFloat("Length X", &currentLight->lengthX, 0.01f, 1.0f, "%.2f");
						ImGui::SliderFloat("Length Y", &currentLight->lengthY, 0.01f, 1.0f, "%.2f");
						ImGui::SliderFloat("Length Z", &currentLight->lengthZ, 0.01f, 1.0f, "%.2f");

						// update dirX and dirY in light
						currentLight->dirX = glm::vec3(rotYZMatrix * glm::vec4(lightDirX, 1.0f));
						currentLight->dirY = glm::vec3(rotYZMatrix * glm::vec4(lightDirY, 1.0f));
						currentLight->dirZ = glm::vec3(rotYZMatrix * glm::vec4(lightDirZ, 1.0f));
						//currentLight->updatePoints();

						// shader configuration
						shader.use();
						shader.setBool("twoSided", true);

						// set scale factors for drawing light object
						modelScaler = glm::vec3(currentLight->lengthX, currentLight->lengthY, currentLight->lengthZ);
					}
					// call update function in light. TODO: only call it when changed
					areaLight->updatePoints();
				}
			}
			else // scene2 GUI
			{
				{
					const char* types[] = { "Default", "Stone", "Marble", "Wood", "Diamond Plate" };
					ImGui::Combo("Plane textures", &planeType, types, IM_ARRAYSIZE(types));

					ImGui::SliderInt("Sphere Lights", &numSmallSphereLight, 0, 100);

					static bool dithering = false;
					ImGui::Checkbox("Dithering", &dithering);
					shader.use();
					shader.setBool("dithering", dithering);

					static bool autoRotation = false;
					ImGui::Checkbox("Auto-Rotate", &autoRotation);
					if (autoRotation)
					{
						cameraRotation -= 0.1f;
						camera.yaw = cameraRotation;
						camera.updateCameraVectors();
					}

					static bool ripple = false;
					ImGui::Checkbox("Ripple", &ripple);
					shader.use();
					shader.setBool("ripple", ripple);
				}
				// update plane texture maps
				textureMaps = texMapList[planeType];
			}

			ImGui::End();
		}

		// 1. render the scene into texture
		// -----------------------------------------------------
		glViewport(0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (scene == 0)
		{
			// rendering scene 1
			// -----------------------------------------------------

			// configure transforms
			auto model = glm::mat4(1.0f);
			model = glm::scale(model, glm::vec3(PLANE_SCALER));
			auto view = camera.getViewMatrix();
			auto projection = glm::perspective(glm::radians(45.0f), (float)TEXTURE_WIDTH / (float)TEXTURE_HEIGHT, 0.1f, 100.0f);

			// draw plane
			shader.use();
			shader.setMat4("model", model);
			shader.setMat4("view", view);
			shader.setMat4("projection", projection);
			shader.setVec3("cameraPos", camera.position);
			shader.setVec3("light.lightColor", areaLight->color);
			shader.setFloat("light.intensity", areaLight->intensity);
			for (int i = 0; i < areaLight->points.size(); i++)
				shader.setVec3("light.points[" + to_string(i) + "]", areaLight->points[i]);
			shader.setVec3("material.diffuse", GGXMaterial.diffuse);
			shader.setVec3("material.specular", GGXMaterial.specular);
			shader.setFloat("material.roughness", GGXMaterial.roughness);


			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, LTC1TexMap);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, LTC2TexMap);
			quadModel.draw(shader);

			// draw light model
			model = glm::mat4(1.0f);
			model = glm::translate(model, areaLight->center);
			model = glm::rotate(model, glm::radians(rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
			model = glm::rotate(model, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
			model = glm::scale(model, modelScaler);
			model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

			polyLightShader.use();
			polyLightShader.setMat4("model", model);
			polyLightShader.setMat4("view", view);
			polyLightShader.setMat4("projection", projection);
			polyLightShader.setVec3("lightColor", areaLight->color);

			areaLightModels[lightIndex].draw(polyLightShader);
		}
		else
		{
			// rendering scene 2
			// -----------------------------------------------------

			// random small moving sphere lights
			for (int i = 0; i < numSmallSphereLight; i++)
			{
				auto& movingSphereLight = movingSphereLights[i];
				movingSphereLight.t += deltaTime;
				movingSphereLight.s += movingSphereLight.countBounceHit % 2 == 0 ? deltaTime : -deltaTime;

				// changes over time
				auto posXZ = movingSphereLight.movingSpeed * movingSphereLight.movingDir * movingSphereLight.t;
				auto posY = movingSphereLight.bounceHeight * fabs(sin(movingSphereLight.s));
				auto center = movingSphereLight.initCenter + glm::vec3(posXZ.x, 0.0f, posXZ.z);
				center.y = movingSphereLight.sphereLight.lengthY + posY + 0.1f;

				// bounce back if hit the plane boundary
				if (fabs(center.x) >= 29.5f || fabs(center.z >= 29.5f))
				{
					movingSphereLight.initCenter = movingSphereLight.sphereLight.center;

					movingSphereLight.t = deltaTime;
					movingSphereLight.movingDir = -movingSphereLight.movingDir;
					posXZ = movingSphereLight.movingSpeed * movingSphereLight.movingDir * movingSphereLight.t;

					movingSphereLight.countBounceHit++;
					movingSphereLight.s += movingSphereLight.countBounceHit % 2 == 0 ? deltaTime : -deltaTime;
					posY = movingSphereLight.bounceHeight * fabs(sin(movingSphereLight.s));

					center = movingSphereLight.initCenter + glm::vec3(posXZ.x, 0.0f, posXZ.z);
					center.y = movingSphereLight.sphereLight.lengthY + posY + 0.1f;
				}
				movingSphereLight.sphereLight.center = center;

				// update points
				movingSphereLight.sphereLight.updatePoints();
			}


			// random displacement and color for lights
			vector<glm::mat4> translateMatrice;
			vector<glm::mat4> rotationMatrice;
			vector<glm::mat4> modelMatrice;
			srand(randomSeed);
			GLfloat radius = 0.0f;
			GLfloat orbitSpeed = 0.5f;
			GLfloat selfRotSpeed = 30.0f;
			GLint numLight = 4;
			auto obitCenter = glm::vec3(0.0f, 10.0f, 0.0f);
			auto origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			auto model = mat4(1.0f);

			for (int i = 0; i < numLight; i++, radius += 5.0f)
			{
				GLfloat angle = currentTime * orbitSpeed * random(0.5f, 1.0f);
				GLfloat x = sin(angle) * radius;
				GLfloat y = 0.0f;
				GLfloat z = cos(angle) * radius;
				auto translate = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
				translate = glm::translate(translate, obitCenter);
				translateMatrice.push_back(translate);

				GLfloat selfRotAngle = currentTime * selfRotSpeed * random(0.6f, 1.0f);
				auto rotAxis = glm::vec3(random(0.1f, 1.0f), random(0.1f, 1.0f), random(0.1f, 1.0f));
				auto rotate = glm::rotate(glm::mat4(1.0f), glm::radians(selfRotAngle), rotAxis);
				rotationMatrice.push_back(rotate);

				model = glm::translate(model, glm::vec3(x, y, z));
				model = glm::translate(model, obitCenter);
				model = glm::rotate(model, glm::radians(selfRotAngle), rotAxis);
				model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

				modelMatrice.push_back(model);
				model = mat4(1.0f);
			}

			sphereLight->color = glm::vec3(
				fabs(sin(0.3f * currentTime) / 2.0f + 0.5f), 
				fabs(sin(0.7f * currentTime) / 2.0f + 0.5f),
				fabs(sin(0.5f * currentTime) / 2.0f + 0.5f)
			);
			sphereLight->intensity = 10.0f;
			sphereLight->lengthX = 2.0f;
			sphereLight->lengthY = 2.0f;
			sphereLight->lengthZ = 2.0f;
			sphereLight->center = glm::vec3(translateMatrice[0] * origin);
			modelMatrice[0] = glm::scale(modelMatrice[0], glm::vec3(sphereLight->lengthX, sphereLight->lengthY, sphereLight->lengthZ));
			sphereLight->updatePoints();

			rectLight->color = glm::vec3(1.0f, 0.0f, 0.0f);
			rectLight->intensity = 8.0f;
			rectLight->halfX = 1.0f;
			rectLight->halfY = 1.0f;
			rectLight->center = glm::vec3(translateMatrice[1] * origin);
			rectLight->dirX = glm::vec3(rotationMatrice[1] * glm::vec4(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f));
			rectLight->dirY = glm::vec3(rotationMatrice[1] * glm::vec4(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f));
			modelMatrice[1] = glm::scale(modelMatrice[1], glm::vec3(rectLight->halfX, 1.0f, rectLight->halfY));
			rectLight->updatePoints();

			diskLight->color = glm::vec3(0.0f, 1.0f, 0.0f);
			diskLight->intensity = 8.0f;
			diskLight->halfX = 1.0f;
			diskLight->halfY = 1.0f;
			diskLight->center = glm::vec3(translateMatrice[2] * origin);
			diskLight->dirX = glm::vec3(rotationMatrice[2] * glm::vec4(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f));
			diskLight->dirY = glm::vec3(rotationMatrice[2] * glm::vec4(glm::vec3(0.0f, 1.0f, 0.0f), 1.0f));
			modelMatrice[2] = glm::scale(modelMatrice[2], glm::vec3(diskLight->halfX, 1.0f, diskLight->halfY));
			diskLight->updatePoints();

			cylinderLight->color = glm::vec3(0.0f, 0.0f, 1.0f);
			cylinderLight->intensity = 20.0f;
			cylinderLight->length = 2.0f;
			cylinderLight->radius = 0.05f;
			cylinderLight->center = glm::vec3(translateMatrice[3] * origin);
			cylinderLight->tangent = glm::vec3(rotationMatrice[3] * glm::vec4(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f));
			modelMatrice[3] = glm::scale(modelMatrice[3], glm::vec3(cylinderLight->length / 2.0f, cylinderLight->radius, cylinderLight->radius));
			cylinderLight->updatePoints();


			// configure transforms
			model = glm::mat4(1.0f);
			model = glm::scale(model, glm::vec3(PLANE_SCALER));
			auto view = camera.getViewMatrix();
			auto projection = glm::perspective(glm::radians(45.0f), (float)TEXTURE_WIDTH / (float)TEXTURE_HEIGHT, 0.1f, 100.0f);
			auto normalMapRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

			// set shader uniforms
			shader.use();
			shader.setMat4("model", model);
			shader.setMat4("view", view);
			shader.setMat4("projection", projection);
			shader.setMat4("normalMapRot", normalMapRot);
			shader.setVec3("cameraPos", camera.position);
			for (int i = 0; i < numLight; i++)
			{
				shader.setInt("lights[" + to_string(i) + "].type", i);
				shader.setFloat("lights[" + to_string(i) + "].intensity", areaLights[i]->intensity);
				shader.setVec3("lights[" + to_string(i) + "].lightColor", areaLights[i]->color);
				for (int j = 0; j < areaLights[i]->points.size(); j++)
					shader.setVec3("lights[" + to_string(i) + "].points[" + to_string(j) + "]", areaLights[i]->points[j]); // lights[i].points[j]
			}
			shader.setFloat("lights[3].radius", cylinderLight->radius);
			for (int i = 0; i < numSmallSphereLight; i++)
			{
				shader.setFloat("sphereLights[" + to_string(i) + "].intensity", movingSphereLights[i].sphereLight.intensity);
				shader.setVec3("sphereLights[" + to_string(i) + "].lightColor", movingSphereLights[i].sphereLight.color);
				for (int j = 0; j < movingSphereLights[i].sphereLight.points.size(); j++)
					shader.setVec3("sphereLights[" + to_string(i) + "].points[" + to_string(j) + "]", movingSphereLights[i].sphereLight.points[j]); // lights[i].points[j]
			}
			shader.setVec3("material.diffuse", GGXMaterial.diffuse);
			shader.setVec3("material.specular", GGXMaterial.specular);
			shader.setFloat("material.roughness", GGXMaterial.roughness);
			shader.setInt("planeType", planeType);
			shader.setInt("numSphereLights", numSmallSphereLight);
			shader.setFloat("time", currentTime);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, LTC1TexMap);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, LTC2TexMap);
			for (int i = 0; i < textureMaps.size(); i++)
			{
				glActiveTexture(GL_TEXTURE2 + i);
				string name = textureMaps[i].name;
				name = name == "displacement" ? "dispMap" : "material.texture_" + name;
				shader.setInt(name, 2 + i);
				glBindTexture(GL_TEXTURE_2D, textureMaps[i].id);
			}
			glBindVertexArray(tessQuadModel.meshes[0].VAO);
			glDrawArrays(GL_PATCHES, 0, 4);
			glBindVertexArray(0);

			// draw light model
			polyLightShader.use();
			polyLightShader.setMat4("view", view);
			polyLightShader.setMat4("projection", projection);

			for (int i = 0; i < numLight; i++)
			{
				model = modelMatrice[i] ;
				polyLightShader.setMat4("model", model);
				polyLightShader.setVec3("lightColor", areaLights[i]->color);

				areaLightModels[i].draw(polyLightShader);
			}
			for (int i = 0; i < numSmallSphereLight; i++)
			{
				model = mat4(1.0f);
				model = glm::translate(model, movingSphereLights[i].sphereLight.center);
				model = glm::scale(model, glm::vec3(movingSphereLights[i].sphereLight.lengthX));
				polyLightShader.setMat4("model", model);
				polyLightShader.setVec3("lightColor", movingSphereLights[i].sphereLight.color);

				sphereModel.draw(polyLightShader);
			}
		}


		// 2. output rendering result
		// ----------------------------------------------------
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDisable(GL_DEPTH_TEST);

		{
			ImGui::Begin("Scene Window", NULL, window_flags);
			ImVec2 screenPos = ImGui::GetCursorScreenPos();
			ImGui::Image((void*)renderedTex, ImVec2(TEXTURE_WIDTH, TEXTURE_HEIGHT), ImVec2(0, 1), ImVec2(1, 0));
			ImGui::End();
		}

		//ImGui::ShowDemoWindow(&show_demo_window);

		// render Dear ImGui into screen
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		

		// swap buffer
		// -----------------------------------------------------
		glfwSwapBuffers(window);
	}

	// cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}