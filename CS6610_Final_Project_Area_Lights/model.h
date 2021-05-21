#pragma once
#include <glad/glad.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vector>
#include <string>
#include <cmath>

#include "mesh.h"
#include "boundingBox.h"

using namespace std;

class Model
{
public:
	vector<Mesh> meshes;

	Model(const char* path)
	{
		loadModel(path);
	}
	void draw(Shader& shader)
	{
		for (int i = 0; i < meshes.size(); i++)
			meshes[i].draw(shader);
	}

	void drawBox()
	{
		modelBox.draw();
	}

	// loop over all bounding box to get the model center
	// for our teapot, we only have one bounding box
	glm::vec3 getCenter()
	{
		// get the model's bounding box
		glm::vec3 minPos = boxes[0].minPos;
		glm::vec3 maxPos = boxes[0].maxPos;
		for (int i = 1; i < boxes.size(); i++)
		{
			minPos.x = std::min(minPos.x, boxes[i].minPos.x);
			minPos.y = std::min(minPos.y, boxes[i].minPos.y);
			minPos.z = std::min(minPos.z, boxes[i].minPos.z);
			maxPos.x = std::max(maxPos.x, boxes[i].maxPos.x);
			maxPos.y = std::max(maxPos.y, boxes[i].maxPos.y);
			maxPos.z = std::max(maxPos.z, boxes[i].maxPos.z);
		}
		modelBox = boundingBox(minPos, maxPos);
		return modelBox.getCenter();
	}

private:
	// model data
	string directory; // model data directory
	vector<Texture> textures_loaded; // avoid loading same texture in different meshes many times 
	vector<boundingBox> boxes;
	boundingBox modelBox;

	void loadModel(string path);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName);
};

void Model::loadModel(string path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_GenBoundingBoxes |
		aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		cout << "Error assimp: " << importer.GetErrorString() << endl;
		return;
	}
	directory = path.substr(0, path.find_last_of('/'));

	processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
	// process all the meshes in node
	for (int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

		// get bounding box points
		glm::vec3 minPos = glm::vec3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z);
		glm::vec3 maxPos = glm::vec3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);

		boxes.push_back(boundingBox(minPos, maxPos));
		meshes.push_back(processMesh(mesh, scene));
	}
	// process node's children
	for (int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	// mesh data
	vector<Vertex> vertices;
	vector<GLuint> indices;
	vector<Texture> textures;
	vector<glm::vec3> materialComponent;
	GLfloat shininess;

	// process Vertex
	for (int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		// process position
		glm::vec3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.Position = vector;
		// process normal
		if (mesh->mNormals)
		{
			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.Normal = vector;
		}
		else
			vertex.Normal = glm::vec3(0.0f, 0.0f, 0.0f);
		// process texCoords
		// because assimp allows the model have up to 8 texCoords,
		// so we only care about the first set
		if (mesh->mTextureCoords[0])
		{
			glm::vec2 texCoords;
			texCoords.x = mesh->mTextureCoords[0][i].x;
			texCoords.y = mesh->mTextureCoords[0][i].y;
			vertex.TexCoords = texCoords;
		}
		else
			vertex.TexCoords = glm::vec2(0.0f, 0.0f);

		assert(mesh->mTangents);
		vector.x = mesh->mTangents[i].x;
		vector.y = mesh->mTangents[i].y;
		vector.z = mesh->mTangents[i].z;
		vertex.Tangent = vector;

		assert(mesh->mBitangents);
		vector.x = mesh->mBitangents[i].x;
		vector.y = mesh->mBitangents[i].y;
		vector.z = mesh->mBitangents[i].z;
		vertex.Bitangent = vector;

		vertices.push_back(vertex);
	}

	// process indices: mesh -> face -> indices
	for (int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	// process materials/textures
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		// get diffuse textures
		vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
		textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		// get specular textures
		vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
		textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		// get shininess
		material->Get(AI_MATKEY_SHININESS, shininess);
		// get ambient
		aiColor3D color;
		if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
			materialComponent.push_back(glm::vec3(color.r, color.g, color.b));
		else
			cout << "Cannot load ambient component\n";
		// get diffuse
		if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
			materialComponent.push_back(glm::vec3(color.r, color.g, color.b));
		else
			cout << "Cannot load diffuse component\n";
		// get specular
		if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
			materialComponent.push_back(glm::vec3(color.r, color.g, color.b));
		else
			cout << "Cannot load specular component\n";
	}

	return Mesh(vertices, indices, textures, materialComponent, shininess);
}

// get and bind all the textures
GLuint TextureFromFile(const char* path, const string& directory)
{
	string fileName = string(path);
	fileName = directory + '/' + fileName;

	GLuint textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(fileName.c_str(), &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format{};
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		//for (int i = 0; i < 100; i++)
		//	cout << "data[" << to_string(i) << "] = " << int(data[i]) << endl;

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

	return textureID;
}

vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
{
	vector<Texture> textures;
	for (int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		bool skip = false;
		for (int j = 0; j < textures_loaded.size(); j++)
		{
			if (strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
			{
				textures.push_back(textures_loaded[j]);
				skip = true;
				break;
			}
		}
		if (!skip)
		{
			Texture texture;
			texture.id = TextureFromFile(str.C_Str(), directory);
			texture.type = typeName;
			texture.path = str.C_Str();
			textures.push_back(texture);
			textures_loaded.push_back(texture);
		}
	}
	return textures;
}