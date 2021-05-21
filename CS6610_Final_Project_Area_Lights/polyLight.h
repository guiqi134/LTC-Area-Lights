#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <Eigen/Dense>
#include <vector>

using namespace glm;

enum class LightType
{
	Rectangle,
	Cylinder,
	Disk,
	Sphere
};

// TODO: create setDefault method for all light types
class AreaLight
{
public:
	LightType type;
	vec3 color;
	vec3 center;
	GLfloat intensity;
	vector<vec3> points; // key variable pass to the shader

	AreaLight(LightType type, vec3 color, vec3 center, GLfloat intensity) : type(type), color(color), center(center), intensity(intensity) { }
	virtual void updatePoints() = 0;
	virtual void draw() { };

};

// rectangle and disk light have same properties, so I combind them
class RectDiskLight : public AreaLight
{
public:
	vec3 dirX;
	vec3 dirY;
	GLfloat halfX;
	GLfloat halfY;

	RectDiskLight(LightType type, vec3 color, vec3 center, GLfloat intensity, vec3 dirX, vec3 dirY, GLfloat halfX, GLfloat halfY)
		: dirX(dirX), dirY(dirY), halfX(halfX), halfY(halfY), AreaLight(type, color, center, intensity)
	{
		updatePoints();
	}

	// Call after each change of light shape
	void updatePoints()
	{
		vec3 ex = halfX * dirX;
		vec3 ey = halfY * dirY;

		points.clear();
		points.push_back(center - ex - ey);
		points.push_back(center + ex - ey);
		points.push_back(center + ex + ey);
		points.push_back(center - ex + ey);
	}
};


class CylinderLight : public AreaLight
{
public:
	vec3 tangent; // the line direction
	GLfloat length;
	GLfloat radius;

	CylinderLight(vec3 color, vec3 center, GLfloat intensity, vec3 tangent, GLfloat length, GLfloat radius)
		: tangent(tangent), length(length), radius(radius), AreaLight(LightType::Cylinder, color, center, intensity)
	{
		updatePoints();
	}

	virtual void updatePoints()
	{
		points.clear();
		points.push_back(center - 0.5f * length * tangent);
		points.push_back(center + 0.5f * length * tangent);
	}
};

inline void outputVec3(vec3 a)
{
	std::cout << a[0] << " " << a[1] << " " << a[2] << endl;
}

inline void outputVec4(vec4 a)
{
	std::cout << a[0] << " " << a[1] << " " << a[2] << " " << a[3] << endl;
}

// both for ellipsoid and sphere light
class SphereLight : public AreaLight
{
public:
	vec3 dirX;
	vec3 dirY;
	vec3 dirZ;
	GLfloat lengthX;
	GLfloat lengthY;
	GLfloat lengthZ;

	SphereLight(vec3 color, vec3 center, GLfloat intensity, vec3 dirX, vec3 dirY, vec3 dirZ, GLfloat lengthX, GLfloat lengthY, GLfloat lengthZ)
		: dirX(dirX), dirY(dirY), dirZ(dirZ), lengthX(lengthX), lengthY(lengthY), lengthZ(lengthZ), AreaLight(LightType::Sphere, color, center, intensity)
	{
		updatePoints();
	}

	// Reference: Analytical calculation of the solid angle subtended by an arbitrarily positioned ellipsoid
	// Be careful the method in the paper assumes the origin to be (0, 0, 0)
	virtual void updatePoints()
	{
		auto origin = glm::vec3(center.x, 0.0f, center.z);
		auto local_center = center - origin;

		auto A = mat3(normalize(dirX), normalize(dirY), normalize(dirZ));
		auto diagonal = mat3(
			lengthX, 0.0f, 0.0f,
			0.0f, lengthY, 0.0f,
			0.0f, 0.0f, lengthZ
		);

		// ellipsoid to sphere
		auto M = A * diagonal * transpose(A);
		auto Minv = inverse(M);
		auto Pb = Minv * local_center;

		// sphere to disk
		auto theta = asin(1 / length(Pb));
		auto Pc = cos(theta) * cos(theta) * Pb;
		auto radius = tan(theta) * length(Pc);
		vec3 C1, C2;
		buildOrthonormalBasis(normalize(Pc), C1, C2);

		// disk to ellipse
		auto Pd = M * Pc;
		auto D1_ = M * radius * C1;
		auto D2_ = M * radius * C2;

		// ellipse principal axes
		Eigen::Matrix2f Q;
		Q << dot(D1_, D1_), dot(D1_, D2_), dot(D1_, D2_), dot(D2_, D2_);
		Eigen::SelfAdjointEigenSolver<Eigen::Matrix2f> eigensolver(Q);
		if (eigensolver.info() != Eigen::Success) abort();
		auto eigenvalues = eigensolver.eigenvalues();
		auto eigenvectors = eigensolver.eigenvectors();
		//cout << "Here is the matrix Q:\n" << Q << endl;
		//cout << "The eigenvalues of Q are:\n" << eigenvalues << endl;
		//cout << "Here's a matrix whose columns are eigenvectors of Q \n"
		//	<< "corresponding to these eigenvalues:\n"
		//	<< eigenvectors << endl;

		auto D1 = eigenvectors(0, 0) * D1_ + eigenvectors(1, 0) * D2_;
		auto D2 = eigenvectors(0, 1) * D1_ + eigenvectors(1, 1) * D2_;
		D1 = normalize(D1);
		D2 = normalize(D2);
		auto d1 = sqrt(eigenvalues(0));
		auto d2 = sqrt(eigenvalues(1));

		// update points
		auto ex = D1 * d1;
		auto ey = D2 * d2;

		points.clear();
		points.push_back(center - ex - ey);
		points.push_back(center + ex - ey);
		points.push_back(center + ex + ey);
		points.push_back(center - ex + ey);
	}
private:
	void buildOrthonormalBasis(const vec3 n, vec3& b1, vec3& b2)
	{
		if (n.z < -0.9999999f)
		{
			b1 = vec3(0.0, -1.0, 0.0);
			b2 = vec3(-1.0, 0.0, 0.0);
			return;
		}
		float a = 1.0f / (1.0f + n.z);
		float b = -n.x * n.y * a;
		b1 = vec3(1.0f - n.x * n.x * a, b, -n.x);
		b2 = vec3(b, 1.0f - n.y * n.y * a, -n.y);
	}
};

struct AreaLightList
{
	std::vector<AreaLight*> areaLights;

	AreaLightList() = default;
	AreaLightList(AreaLight* areaLight) { areaLights.push_back(areaLight); }
};