#pragma once
#include <numbers>
#include <fstream>
#include <vector>
#include <xaudio2.h>

class Function {};

struct Vector2 {
	float x, y;
};

struct Vector3 {
	float x, y, z;
};

struct Vector4 {
	float x, y, z, w;
};

struct Matrix3x3 {
	float m[3][3];
};

struct Matrix4x4 {
	float m[4][4];
};

struct AABB {
	Vector3 min;
	Vector3 max;

	void Fix() {
		if (min.x > max.x) std::swap(min.x, max.x);
		if (min.y > max.y) std::swap(min.y, max.y);
		if (min.z > max.z) std::swap(min.z, max.z);
	}
};

struct LineVertex {
	Vector3 position;
	Vector4 color;
};

struct WorldTransform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	//float padding[4];
};
struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct MaterialData {
	std::string textureFilePath;
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	int32_t lightingMode;
	float halfLambertPower;
	float padding;
	Matrix4x4 uvTransform;
};

struct Particle {
	WorldTransform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
};

struct Emitter {
	WorldTransform transform;
	uint32_t count;
	float frequency;
	float frequencyTime;
};

struct AccelerationField {
	Vector3 acceleration;
	AABB area;
};

struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};

struct ChunkHeader {
	char id[4];
	int32_t size;
};

struct RiffHeader {
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk {
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

struct SoundData {
	WAVEFORMATEX wfex;
	BYTE* pBuffer;
	unsigned int bufferSize;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

struct SphereMesh {
	std::vector<VertexData> vertices;
};


Matrix4x4 MakeIdentity4x4(Matrix4x4& matrix);
Matrix4x4 MakeScaleMatrix(Vector3 scale);
Matrix4x4 Multiply(Matrix4x4 a, Matrix4x4 b);
Matrix4x4 MakeRotatedXYZMatrix(Matrix4x4 x, Matrix4x4 y, Matrix4x4 z);
Matrix4x4 MakeRotateXMatrix(float radian);
Matrix4x4 MakeRotateYMatrix(float radian);
Matrix4x4 MakeRotateZMatrix(float radian);
Matrix4x4 MakeAffineMatrix(Vector3 scale, Vector3 rotate, Vector3 translate);
Matrix4x4 MakeTranslateMatrix(Vector3 translate);
Matrix4x4 MakeUVTransformMatrix(const WorldTransform& uv);
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
Matrix4x4 Inverse(Matrix4x4 m);
Matrix4x4 MakeBillboardWorld(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Matrix4x4& billboard, bool useBillboard);
Vector4 Transform(const Vector4& v, const Matrix4x4& m);


bool IsCollision(const AABB& aabb, const Vector3& point);