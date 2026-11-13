#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
//#include <d3d12.h>
//#include <dxgi1_6.h>
//#include <cassert>
#include <vector>
//#include <strsafe.h>
//#include <dbghelp.h>
//#include <dxgidebug.h>
//#include <dxcapi.h>
//#include <cmath>
#include <xaudio2.h>
#include <random>
#include <numbers>

//#define DIRECTINPUT_VERSION	0x800
//#include <dinput.h>
#include "DebugCamera.h"
#include <wrl.h>
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
//extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif


#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
//#pragma comment(lib, "Dbghelp.lib")


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

float pi = 3.14f;
float particleLifeTime = 2.0f;

bool useDebugCamera = false;
bool useBillboard = true;
bool useInstancing = true;
bool useEmitter = false;
bool randomColor = false;
bool fadeOut = false;

D3D12_BLEND_DESC CreateBlendDesc(int mode) {
	D3D12_BLEND_DESC desc{};
	desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.RenderTarget[0].BlendEnable = TRUE;

	// Default alpha channel
	desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	switch (mode)
	{
	case kBlendModeNone:
		desc.RenderTarget[0].BlendEnable = FALSE;
		break;

	case kBlendModeNormal:          // normal alpha blending
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;

	case kBlendModeAdd:             // additive
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;

	case kBlendModeSubtract:        // subtractive
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;

	case kBlendModeMultiply:        // multiply
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		break;

	case kBlendModeScreen:          // screen
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;
	}

	return desc;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {

	//1.中で必要となる変数の宣言
	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;

	//2.ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());
	
	//3.実際にファイルを読み、ModelDataを構築していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifierに応じた処理
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		}
		else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		}
		//supports triangles, quads and genreal convex n-gons
		else if (identifier == "f") {
			std::vector<VertexData> faceVerts;
			std::string vertexDefinition;

			while (s >> vertexDefinition) {
				// position/uv/normal 
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3] = { 0,0,0 };

				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');
					if (!index.empty()) {
						try {
							elementIndices[element] = static_cast<uint32_t>(std::stoi(index));
						}
						catch (...) {
							elementIndices[element] = 0;
						}
					}
				}

				// skip invalid position index
				if (elementIndices[0] == 0 || elementIndices[0] > positions.size()) {
					continue;
				}

				Vector4 position = positions[elementIndices[0] - 1];

				Vector2 texcoord = { 0.0f, 0.0f };
				if (elementIndices[1] > 0 && elementIndices[1] <= texcoords.size()) {
					texcoord = texcoords[elementIndices[1] - 1];
					texcoord.y = 1.0f - texcoord.y;	// flip v
				}

				Vector3 normal = { 0.0f, 0.0f, 1.0f };
				if (elementIndices[2] > 0 && elementIndices[2] <= normals.size()) {
					normal = normals[elementIndices[2] - 1];
					normal.x *= -1.0f;	// keep existing flip
				}

				faceVerts.push_back({ position,texcoord,normal });
			}
			// triangle fan (0,i,i+1)
			for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
				
				VertexData v0 = faceVerts[0];
				VertexData v1 = faceVerts[i];
				VertexData v2 = faceVerts[i + 1];

				// Front face (keep your existing reversed winding if you need it)
				modelData.vertices.push_back(v0);
				modelData.vertices.push_back(v2);
				modelData.vertices.push_back(v1);
				
				// Back face: opposite winding + flipped normals
				VertexData b0 = v0; b0.normal.x = -b0.normal.x; b0.normal.y = -b0.normal.y; b0.normal.z = -b0.normal.z;
				VertexData b1 = v1; b1.normal.x = -b1.normal.x; b1.normal.y = -b1.normal.y; b1.normal.z = -b1.normal.z;
				VertexData b2 = v2; b2.normal.x = -b2.normal.x; b2.normal.y = -b2.normal.y; b2.normal.z = -b2.normal.z;

				modelData.vertices.push_back(b0);
				modelData.vertices.push_back(b1);  // opposite order of front
				modelData.vertices.push_back(b2);
				
			}
		}
	}
	//4.ModelDataを返す
	return modelData;
}

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	
	//1.中で必要となる変数の宣言
	ModelData modelData;
	MaterialData materialData;
	std::string line;

	//2.ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	//3.実際にファイルを読み、ModelDataを構築していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;

			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
		else if (identifier == "mtllib") {
			std::string materialFilename;
			s >> materialFilename;

			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	//4.ModelDataを返す
	return materialData;
}

SoundData SoundLoadWave(const char* filename) {
	
	//1. File open
	std::ifstream file;

	file.open(filename, std::ios_base::binary);

	assert(file.is_open());

	//2. Read .wav data (convert audio files to .wav format beforehand)
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));

	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0 && "Not a valid Wave file");
	}
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0 && "Not a valid Wave file");
	}

	FormatChunk format = {};
	ChunkHeader data = {};
	while (file.read((char*)&data, sizeof(ChunkHeader))) {
		if (strncmp(data.id, "fmt", 3) == 0) {  // "fmt "
			format.chunk = data;
			assert(format.chunk.size <= sizeof(format.fmt));
			file.read((char*)&format.fmt, format.chunk.size);
		}
		else if (strncmp(data.id, "data", 4) == 0) {
			break;  // Found data chunk
		}
		else {
			// Skip unknown chunk
			file.seekg(data.size, std::ios_base::cur);
		}
	}
	if (strncmp(data.id, "data", 4) != 0) {
		assert(0 && "data chunk not found");
	}

	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	file.close();

	//3. File close

	//4. return read sound data
	

	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

void SoundUnload(SoundData* soundData) {
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

IXAudio2SourceVoice* pSourceVoice = nullptr;
bool isSoundPlaying = false;

void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData, bool play) {
	HRESULT hr;

	//1. Create source voice
	if (!pSourceVoice) {
		//IXAudio2SourceVoice* pSourceVoice = nullptr;
		hr = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
		assert(SUCCEEDED(hr));
	}

	//2. Reproduced wave data settings
	if (play) {
		pSourceVoice->Stop(0);
		pSourceVoice->FlushSourceBuffers();

		XAUDIO2_BUFFER buf{};
		buf.pAudioData = soundData.pBuffer;
		buf.AudioBytes = soundData.bufferSize;
		buf.Flags = XAUDIO2_END_OF_STREAM;

		//3. Start produced data
		hr = pSourceVoice->SubmitSourceBuffer(&buf);
		hr = pSourceVoice->Start();
	}
	else {
		pSourceVoice->Stop(0);
		pSourceVoice->FlushSourceBuffers();
	}
}



Matrix4x4 MakeIdentity4x4(Matrix4x4& matrix) {
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			matrix.m[i][j] = (i == j) ? 1.0f : 0.0f;
		}
	}
	return matrix;
}

Matrix4x4 MakeScaleMatrix(Vector3 scale) {
	Matrix4x4 result = MakeIdentity4x4(result);
	result.m[0][0] = scale.x;
	result.m[1][1] = scale.y;
	result.m[2][2] = scale.z;
	return result;
}

Matrix4x4 Multiply(Matrix4x4 a, Matrix4x4 b) {
	Matrix4x4 result;
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			result.m[row][column] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				result.m[row][column] += a.m[row][k] * b.m[k][column];
			}
		}
	}
	return result;
}

Matrix4x4 MakeRotatedXYZMatrix(Matrix4x4 x, Matrix4x4 y, Matrix4x4 z) {
	Matrix4x4 result = Multiply(x, Multiply(y, z));
	return result;
}

Matrix4x4 MakeRotateXMatrix(float radian) {
	Matrix4x4 result;
	result.m[0][0] = 1;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = std::sin(radian);
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = -std::sin(radian);
	result.m[2][2] = std::cos(radian);
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}

Matrix4x4 MakeRotateYMatrix(float radian) {
	Matrix4x4 result;
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = 0;
	result.m[0][2] = -std::sin(radian);
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = 1;
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = std::sin(radian);
	result.m[2][1] = 0;
	result.m[2][2] = std::cos(radian);
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}

Matrix4x4 MakeRotateZMatrix(float radian) {
	Matrix4x4 result;
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = std::sin(radian);
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = -std::sin(radian);
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}

Matrix4x4 MakeAffineMatrix(Vector3 scale, Vector3 rotate, Vector3 translate) {
	Matrix4x4 rotatedX = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotatedY = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotatedZ = MakeRotateZMatrix(rotate.z);

	Matrix4x4 rotatedXYZMatrix = MakeRotatedXYZMatrix(rotatedX, rotatedY, rotatedZ);

	Matrix4x4 result;
	result.m[0][0] = scale.x * rotatedXYZMatrix.m[0][0];
	result.m[0][1] = scale.x * rotatedXYZMatrix.m[0][1];
	result.m[0][2] = scale.x * rotatedXYZMatrix.m[0][2];
	result.m[0][3] = 0;

	result.m[1][0] = scale.y * rotatedXYZMatrix.m[1][0];
	result.m[1][1] = scale.y * rotatedXYZMatrix.m[1][1];
	result.m[1][2] = scale.y * rotatedXYZMatrix.m[1][2];
	result.m[1][3] = 0;

	result.m[2][0] = scale.z * rotatedXYZMatrix.m[2][0];
	result.m[2][1] = scale.z * rotatedXYZMatrix.m[2][1];
	result.m[2][2] = scale.z * rotatedXYZMatrix.m[2][2];
	result.m[2][3] = 0;

	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	result.m[3][3] = 1;

	return result;
}

Matrix4x4 MakeTranslateMatrix(Vector3 translate) {
	Matrix4x4 result;
	result.m[0][0] = 1;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;
	result.m[1][0] = 0;
	result.m[1][1] = 1;
	result.m[1][2] = 0;
	result.m[1][3] = 0;
	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;
	result.m[2][3] = 0;
	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	result.m[3][3] = 1;
	return result;
}

Matrix4x4 MakeUVTransformMatrix(const WorldTransform& uv) {
	
	Matrix4x4 scaleMatrix = MakeScaleMatrix(uv.scale);
	//Matrix4x4 rotateXMatrix = MakeRotateXMatrix(uv.rotate.x);
	//Matrix4x4 rotateYMatrix = MakeRotateYMatrix(uv.rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(uv.rotate.z);
	//Matrix4x4 rotationMatrix = Multiply(Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(uv.translate);
	Matrix4x4 uvTransform = Multiply(Multiply(scaleMatrix, rotateZMatrix), translateMatrix);
	return uvTransform;
}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result;
	float f = tanf(fovY / 2.0f);
	float cotangent = 1.0f / f;
	result.m[0][0] = cotangent / aspectRatio;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;
	result.m[1][0] = 0;
	result.m[1][1] = cotangent;
	result.m[1][2] = 0;
	result.m[1][3] = 0;
	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = (farClip) / (farClip - nearClip);
	result.m[2][3] = 1;
	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = (farClip * (-1 * nearClip)) / (farClip - nearClip);
	result.m[3][3] = 0;
	return result;
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result;
	result.m[0][0] = 2.0f / (right - left);
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;
	result.m[1][0] = 0;
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[1][2] = 0;
	result.m[1][3] = 0;
	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[2][3] = 0;
	result.m[3][0] = (right + left) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = (nearClip) / (nearClip - farClip);
	result.m[3][3] = 1;
	return result;
}

Matrix4x4 Inverse(Matrix4x4 m) {
	Matrix4x4 inverse;

	float determinant =

		(m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]) +
		(m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]) +
		(m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]) -

		(m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]) -
		(m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]) -
		(m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]) -

		(m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]) -
		(m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]) -
		(m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]) +

		(m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]) +
		(m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]) +
		(m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]) +

		(m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]) +
		(m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]) +
		(m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]) -

		(m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]) -
		(m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]) -
		(m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]) -

		(m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]) -
		(m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]) -
		(m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]) +

		(m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]) +
		(m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]) +
		(m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0]);


	float invDet = (1.0f / fabsf(determinant));

	// row 0
	inverse.m[0][0] = invDet * ((m.m[1][1] * m.m[2][2] * m.m[3][3]) + (m.m[1][2] * m.m[2][3] * m.m[3][1]) + (m.m[1][3] * m.m[2][1] * m.m[3][2]) -
		(m.m[1][3] * m.m[2][2] * m.m[3][1]) - (m.m[1][2] * m.m[2][1] * m.m[3][3]) - (m.m[1][1] * m.m[2][3] * m.m[3][2]));

	inverse.m[0][1] = invDet * (-(m.m[0][1] * m.m[2][2] * m.m[3][3]) - (m.m[0][2] * m.m[2][3] * m.m[3][1]) - (m.m[0][3] * m.m[2][1] * m.m[3][2]) +
		(m.m[0][3] * m.m[2][2] * m.m[3][1]) + (m.m[0][2] * m.m[2][1] * m.m[3][3]) + (m.m[0][1] * m.m[2][3] * m.m[3][2]));

	inverse.m[0][2] = invDet * ((m.m[0][1] * m.m[1][2] * m.m[3][3]) + (m.m[0][2] * m.m[1][3] * m.m[3][1]) + (m.m[0][3] * m.m[1][1] * m.m[3][2]) -
		(m.m[0][3] * m.m[1][2] * m.m[3][1]) - (m.m[0][2] * m.m[1][1] * m.m[3][3]) - (m.m[0][1] * m.m[1][3] * m.m[3][2]));

	inverse.m[0][3] = invDet * (-(m.m[0][1] * m.m[1][2] * m.m[2][3]) - (m.m[0][2] * m.m[1][3] * m.m[2][1]) - (m.m[0][3] * m.m[1][1] * m.m[2][2]) +
		(m.m[0][3] * m.m[1][2] * m.m[2][1]) + (m.m[0][2] * m.m[1][1] * m.m[2][3]) + (m.m[0][1] * m.m[1][3] * m.m[2][2]));

	// row 1
	inverse.m[1][0] = invDet * (-(m.m[1][0] * m.m[2][2] * m.m[3][3]) - (m.m[1][2] * m.m[2][3] * m.m[3][0]) - (m.m[1][3] * m.m[2][0] * m.m[3][2]) +
		(m.m[1][3] * m.m[2][2] * m.m[3][0]) + (m.m[1][2] * m.m[2][0] * m.m[3][3]) + (m.m[1][0] * m.m[2][3] * m.m[3][2]));

	inverse.m[1][1] = invDet * ((m.m[0][0] * m.m[2][2] * m.m[3][3]) + (m.m[0][2] * m.m[2][3] * m.m[3][0]) + (m.m[0][3] * m.m[2][0] * m.m[3][2]) -
		(m.m[0][3] * m.m[2][2] * m.m[3][0]) - (m.m[0][2] * m.m[2][0] * m.m[3][3]) - (m.m[0][0] * m.m[2][3] * m.m[3][2]));

	inverse.m[1][2] = invDet * (-(m.m[0][0] * m.m[1][2] * m.m[3][3]) - (m.m[0][2] * m.m[1][3] * m.m[3][0]) - (m.m[0][3] * m.m[1][0] * m.m[3][2]) +
		(m.m[0][3] * m.m[1][2] * m.m[3][0]) + (m.m[0][2] * m.m[1][0] * m.m[3][3]) + (m.m[0][0] * m.m[1][3] * m.m[3][2]));

	inverse.m[1][3] = invDet * ((m.m[0][0] * m.m[1][2] * m.m[2][3]) + (m.m[0][2] * m.m[1][3] * m.m[2][0]) + (m.m[0][3] * m.m[1][0] * m.m[2][2]) -
		(m.m[0][3] * m.m[1][2] * m.m[2][0]) - (m.m[0][2] * m.m[1][0] * m.m[2][3]) - (m.m[0][0] * m.m[1][3] * m.m[2][2]));

	// row 2
	inverse.m[2][0] = invDet * ((m.m[1][0] * m.m[2][1] * m.m[3][3]) + (m.m[1][1] * m.m[2][3] * m.m[3][0]) + (m.m[1][3] * m.m[2][0] * m.m[3][1]) -
		(m.m[1][3] * m.m[2][1] * m.m[3][0]) - (m.m[1][1] * m.m[2][0] * m.m[3][3]) - (m.m[1][0] * m.m[2][3] * m.m[3][1]));

	inverse.m[2][1] = invDet * (-(m.m[0][0] * m.m[2][1] * m.m[3][3]) - (m.m[0][1] * m.m[2][3] * m.m[3][0]) - (m.m[0][3] * m.m[2][0] * m.m[3][1]) +
		(m.m[0][3] * m.m[2][1] * m.m[3][0]) + (m.m[0][1] * m.m[2][0] * m.m[3][3]) + (m.m[0][0] * m.m[2][3] * m.m[3][1]));

	inverse.m[2][2] = invDet * ((m.m[0][0] * m.m[1][1] * m.m[3][3]) + (m.m[0][1] * m.m[1][3] * m.m[3][0]) + (m.m[0][3] * m.m[1][0] * m.m[3][1]) -
		(m.m[0][3] * m.m[1][1] * m.m[3][0]) - (m.m[0][1] * m.m[1][0] * m.m[3][3]) - (m.m[0][0] * m.m[1][3] * m.m[3][1]));

	inverse.m[2][3] = invDet * (-(m.m[0][0] * m.m[1][1] * m.m[2][3]) - (m.m[0][1] * m.m[1][3] * m.m[2][0]) - (m.m[0][3] * m.m[1][0] * m.m[2][1]) +
		(m.m[0][3] * m.m[1][1] * m.m[2][0]) + (m.m[0][1] * m.m[1][0] * m.m[2][3]) + (m.m[0][0] * m.m[1][3] * m.m[2][1]));

	// row 3
	inverse.m[3][0] = invDet * (-(m.m[1][0] * m.m[2][1] * m.m[3][2]) - (m.m[1][1] * m.m[2][2] * m.m[3][0]) - (m.m[1][2] * m.m[2][0] * m.m[3][1]) +
		(m.m[1][2] * m.m[2][1] * m.m[3][0]) + (m.m[1][1] * m.m[2][0] * m.m[3][2]) + (m.m[1][0] * m.m[2][2] * m.m[3][1]));

	inverse.m[3][1] = invDet * ((m.m[0][0] * m.m[2][1] * m.m[3][2]) + (m.m[0][1] * m.m[2][2] * m.m[3][0]) + (m.m[0][2] * m.m[2][0] * m.m[3][1]) -
		(m.m[0][2] * m.m[2][1] * m.m[3][0]) - (m.m[0][1] * m.m[2][0] * m.m[3][2]) - (m.m[0][0] * m.m[2][2] * m.m[3][1]));

	inverse.m[3][2] = invDet * (-(m.m[0][0] * m.m[1][1] * m.m[3][2]) - (m.m[0][1] * m.m[1][2] * m.m[3][0]) - (m.m[0][2] * m.m[1][0] * m.m[3][1]) +
		(m.m[0][2] * m.m[1][1] * m.m[3][0]) + (m.m[0][1] * m.m[1][0] * m.m[3][2]) + (m.m[0][0] * m.m[1][2] * m.m[3][1]));

	inverse.m[3][3] = invDet * ((m.m[0][0] * m.m[1][1] * m.m[2][2]) + (m.m[0][1] * m.m[1][2] * m.m[2][0]) + (m.m[0][2] * m.m[1][0] * m.m[2][1]) -
		(m.m[0][2] * m.m[1][1] * m.m[2][0]) - (m.m[0][1] * m.m[1][0] * m.m[2][2]) - (m.m[0][0] * m.m[1][2] * m.m[2][1]));

	return inverse;
}

Matrix4x4 MakeBillboardWorld(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Matrix4x4& billboard, bool useBillboard) {
	Matrix4x4 scaleMat = MakeScaleMatrix(scale);
	
	// local spin (spin in place)
	Matrix4x4 rotX = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotY = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotZ = MakeRotateZMatrix(rotate.z);
	Matrix4x4 localRot = Multiply(rotX, Multiply(rotY, rotZ));

	Matrix4x4 translateMat = MakeTranslateMatrix(translate);
	
	if (useBillboard) {
		// Scale -> LocalRotation -> Billboard -> Translate
		return Multiply(Multiply(Multiply(scaleMat, localRot), billboard), translateMat);
	}
	return MakeAffineMatrix(scale, rotate, translate);
}

Vector4 Transform(const Vector4& v, const Matrix4x4& m) {
	Vector4 result;
	result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + 1.0f * m.m[3][0];
	result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + 1.0f * m.m[3][1];
	result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + 1.0f * m.m[3][2];

	result.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + 1.0f * m.m[3][3];
	//assert(w != 0.0f);
	result.x /= result.w;
	result.y /= result.w;
	result.z /= result.w;

	return result;
}

Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate, const Vector4& baseColor) {
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	Vector3 randomTranslate{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };

	Particle particle;
	particle.transform.scale = { 1.0f, 1.0f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	particle.transform.translate = { 
		randomTranslate.x + distribution(randomEngine), 
		randomTranslate.y + distribution(randomEngine), 
		randomTranslate.z + distribution(randomEngine) 
	};
	
	particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	
	if (randomColor){
		std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
		particle.color = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine), 1.0f };
	}
	else {
		particle.color = baseColor;
	}

	particle.lifeTime = particleLifeTime;
	particle.currentTime = 0.0f;

	return particle;
}

std::list<Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine, const Vector4& baseColor) {
	std::list<Particle> particles;
	for (uint32_t i = 0; i < emitter.count; ++i) {
		particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate, baseColor));
	}
	return particles;
}

WorldTransform transform = {
	{ 1.0f, 1.0f, 1.0f }, // scale
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 0.0f, 0.0f, 0.0f }  // translate
};

WorldTransform cameraTransform = {
	{ 1.0f, 1.0f, 1.0f }, // scale
	{ std::numbers::pi_v<float> / 3.0f, std::numbers::pi_v<float>, 0.0f }, // rotate
	{ 0.0f, 10.0f, 10.0f } // translate (move camera back along z-axis)
};

WorldTransform uvTransformSprite = { 
	{ 1.0f, 1.0f, 1.0f }, // scale
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 0.0f, 0.0f, 0.0f },  // translate
};

const float kDeltaTime = 1.0f / 60.0f;
std::random_device seedGenerator;
std::mt19937 randomEngine(seedGenerator());

BYTE gKeys[256] = {};
DIMOUSESTATE gMouseState = {};
//LPDIRECTINPUT8 gDirectInput = nullptr;
//LPDIRECTINPUTDEVICE8 gKeyboard = nullptr;
//LPDIRECTINPUTDEVICE8 gMouse = nullptr;


/// /chrono system clock

std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };

std::string dateString = std::format("{:%Y%m%d %H%M%S}", localTime);

std::string logFilePath = std::string("logs/") + dateString + ".log";

std::ofstream logStream(logFilePath);

///// <chrono> end	/////

///// <ConvertString.cpp> start	/////


LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, UINT width, UINT height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;      
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;       

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;
}

//Healper function to create Descriptor Heap
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
	const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

SphereMesh DrawSphere(const uint32_t subdivision, float radius) {
	SphereMesh mesh;

	const uint32_t kSubdivision = subdivision;  ////
	const float kLatEvery = pi / float(kSubdivision); /////
	const float kLonEvery = (2.0f * pi) / float(kSubdivision); /////

	mesh.vertices.reserve(kSubdivision * kSubdivision * 6);

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -pi / 2.0f + kLatEvery * latIndex;
		float latNext = lat + kLatEvery;

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

			float lon = lonIndex * kLonEvery;
			float lonNext = lon + kLonEvery;

			float u = (float)lonIndex / (float)kSubdivision;
			float v = 1.0f - (float)latIndex / (float)kSubdivision;
			float vNext = 1.0f - (float)(latIndex + 1) / (float)kSubdivision;
			float uNext = (float)(lonIndex + 1) / (float)kSubdivision;

			VertexData vertexData[6] = {}; //a,b,c, c,b,d
			vertexData[0].position.x = radius * cos(lat) * cosf(lon);
			vertexData[0].position.y = radius * sin(lat);
			vertexData[0].position.z = radius * cos(lat) * sin(lon);
			vertexData[0].position.w = 1.0f;
			vertexData[0].texcoord = { u, v }; //a
			vertexData[0].normal = { vertexData[0].position.x, vertexData[0].position.y, vertexData[0].position.z };
			
			vertexData[1].position.x = radius * cos(latNext) * cosf(lon);
			vertexData[1].position.y = radius * sin(latNext);
			vertexData[1].position.z = radius * cos(latNext) * sin(lon);
			vertexData[1].position.w = 1.0f;
			vertexData[1].texcoord = { u, vNext };  //b
			vertexData[1].normal = { vertexData[1].position.x, vertexData[1].position.y, vertexData[1].position.z };
			
			vertexData[2].position.x = radius * cos(lat) * cosf(lonNext);
			vertexData[2].position.y = radius * sin(lat);
			vertexData[2].position.z = radius * cos(lat) * sin(lonNext);
			vertexData[2].position.w = 1.0f;
			vertexData[2].texcoord = { uNext, v }; //c
			vertexData[2].normal = { vertexData[2].position.x, vertexData[2].position.y, vertexData[2].position.z };

			vertexData[3].position = vertexData[2].position;
			vertexData[3].texcoord = vertexData[2].texcoord;
			vertexData[3].normal = { vertexData[3].position.x, vertexData[3].position.y, vertexData[3].position.z };

			vertexData[4].position = vertexData[1].position;
			vertexData[4].texcoord = vertexData[1].texcoord;
			vertexData[4].normal = { vertexData[4].position.x, vertexData[4].position.y, vertexData[4].position.z };

			vertexData[5].position.x = radius * cos(latNext) * cosf(lonNext);
			vertexData[5].position.y = radius * sin(latNext);
			vertexData[5].position.z = radius * cos(latNext) * sin(lonNext);
			vertexData[5].position.w = 1.0f;
			vertexData[5].texcoord = { uNext, vNext}; //d
			vertexData[5].normal = { vertexData[5].position.x, vertexData[5].position.y, vertexData[5].position.z };

			//vertices
			for (int i = 0; i < 6; ++i) {
				mesh.vertices.push_back(vertexData[i]);
			}
			// Normalize 
			for (auto& v : mesh.vertices) {
				float length = sqrtf(v.normal.x * v.normal.x +
					v.normal.y * v.normal.y +
					v.normal.z * v.normal.z);
				if (length > 0.0f) {
					v.normal.x /= length;
					v.normal.y /= length;
					v.normal.z /= length;
				}
			}
		}
	}
	return mesh;
}


DirectX::ScratchImage LoadTexture(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	return mipImages;
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	//assert(false && "assertのテストだよ");

	//SetUnhandledExceptionFilter(ExportDump);

	CoInitializeEx(0, COINIT_MULTITHREADED);

	WinApp* winApp = nullptr;
	winApp = new WinApp();
	winApp->Initialize();

	Input* input = nullptr;
	input = new Input();
	input->Initialize(winApp);

	DebugCamera debugCamera;
	debugCamera.Initialize();

	DirectXCommon* dxCommon = nullptr;
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	///-----SOUND DATA---------
	ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice = nullptr;
	HRESULT hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr));
	hr = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(hr));

	SoundData soundData1 = SoundLoadWave("Resources/Alarm01.wav");

	///------SOUND DATA END----------


	////////////////////////////////std::vector<LineVertex> lines;
	////////////////////////////////
	////////////////////////////////const float size = 10.0f;
	////////////////////////////////const float step = 1.0f;
	////////////////////////////////const float y = 0.0f;
	////////////////////////////////
	////////////////////////////////Vector4 gray = { 0.4f, 0.4f, 0.4f, 1.0f };
	////////////////////////////////Vector4 red = { 1.0f, 0.0f, 0.0f, 1.0f }; // X
	////////////////////////////////Vector4 blue = { 0.0f, 0.0f, 1.0f, 1.0f }; // Z
	////////////////////////////////Vector4 green = { 0.0f, 1.0f, 0.0f, 1.0f }; // Y (optional)
	////////////////////////////////
	////////////////////////////////// Grid lines parallel to X (varying Z)
	////////////////////////////////for (float z = -size; z <= size; z += step) {
	////////////////////////////////	Vector4 c = (fabsf(z) < 0.001f) ? red : gray;
	////////////////////////////////	lines.push_back({ {-size, y, z}, c });
	////////////////////////////////	lines.push_back({ { size, y, z}, c });
	////////////////////////////////}
	////////////////////////////////
	////////////////////////////////// Grid lines parallel to Z (varying X)
	////////////////////////////////for (float x = -size; x <= size; x += step) {
	////////////////////////////////	Vector4 c = (fabsf(x) < 0.001f) ? blue : gray;
	////////////////////////////////	lines.push_back({ {x, y, -size}, c });
	////////////////////////////////	lines.push_back({ {x, y,  size}, c });
	////////////////////////////////}
	////////////////////////////////
	////////////////////////////////// Optional Y axis
	////////////////////////////////lines.push_back({ {0, -size, 0}, green });
	////////////////////////////////lines.push_back({ {0,  size, 0}, green });


	//Debug Layer
//ifdef _DEBUG
//	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
//	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
//		debugController->EnableDebugLayer();
//		//debugController->SetEnableGPUBasedValidation(TRUE);
//	}
//endif


	/////------------Load Textures ----------------


	DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata0 = mipImages.GetMetadata();
	ComPtr<ID3D12Resource> textureResource0 = dxCommon->CreateTextureResource(metadata0);
	ComPtr<ID3D12Resource> intermediateResource0 = dxCommon->UploadTextureData(textureResource0.Get(), mipImages);

	DirectX::ScratchImage mipImages1 = LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata1 = mipImages1.GetMetadata();
	ComPtr<ID3D12Resource> textureResource1 = dxCommon->CreateTextureResource(metadata1);
	ComPtr<ID3D12Resource> intermediateResource1 = dxCommon->UploadTextureData(textureResource1.Get(), mipImages1);

	DirectX::ScratchImage mipImages2 = LoadTexture("Resources/sample.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	ComPtr<ID3D12Resource> textureResource2 = dxCommon->CreateTextureResource(metadata2);
	ComPtr<ID3D12Resource> intermediateResource2 = dxCommon->UploadTextureData(textureResource2.Get(), mipImages2);

	DirectX::ScratchImage mipImages3 = LoadTexture("Resources/checkerboard.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	ComPtr<ID3D12Resource> textureResource3 = dxCommon->CreateTextureResource(metadata3);
	ComPtr<ID3D12Resource> intermediateResource3 = dxCommon->UploadTextureData(textureResource3.Get(), mipImages3);

	DirectX::ScratchImage mipImages4 = LoadTexture("Resources/grid.png");
	const DirectX::TexMetadata& metadata4 = mipImages4.GetMetadata();
	ComPtr<ID3D12Resource> textureResource4 = dxCommon->CreateTextureResource(metadata4);
	ComPtr<ID3D12Resource> intermediateResource4 = dxCommon->UploadTextureData(textureResource4.Get(), mipImages4);

	DirectX::ScratchImage mipImages5 = LoadTexture("Resources/fence.png");
	const DirectX::TexMetadata& metadata5 = mipImages5.GetMetadata();
	ComPtr<ID3D12Resource> textureResource5 = dxCommon->CreateTextureResource(metadata5);
	ComPtr<ID3D12Resource> intermediateResource5 = dxCommon->UploadTextureData(textureResource5.Get(), mipImages5);

	DirectX::ScratchImage mipImages6 = LoadTexture("Resources/particle.png");
	const DirectX::TexMetadata& metadata6 = mipImages6.GetMetadata();
	ComPtr<ID3D12Resource> textureResource6 = dxCommon->CreateTextureResource(metadata6);
	ComPtr<ID3D12Resource> intermediateResource6 = dxCommon->UploadTextureData(textureResource6.Get(), mipImages6);

	////--------------Load Texture end --------------

	
	/*---------------PSO (Pipeline State Object)---------------*/
//Root Signature
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RegisterSpace = 0;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1; // _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1; //b1

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	//---------------------------------------------------------
	//	Particle Root Signature
	//----------------------------------------------------------
	D3D12_DESCRIPTOR_RANGE particleRangeVS[1] = {};
	particleRangeVS[0].BaseShaderRegister = 0;
	particleRangeVS[0].NumDescriptors = 1;
	particleRangeVS[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleRangeVS[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE particleRangePS[1] = {};
	particleRangePS[0].BaseShaderRegister = 0;
	particleRangePS[0].NumDescriptors = 1;
	particleRangePS[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleRangePS[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER particleRootParams[3] = {};
	// [0] VS : StructuredBuffer<<TransformationMatrix> t0
	particleRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	particleRootParams[0].DescriptorTable.pDescriptorRanges = particleRangeVS;
	particleRootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	// [1] PS : Material CBV b0
	particleRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	particleRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParams[1].Descriptor.ShaderRegister = 0;
	// [2] PS Texture t0
	particleRootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParams[2].DescriptorTable.pDescriptorRanges = particleRangePS;
	particleRootParams[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC particleSamplers[1] = {};
	particleSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	particleSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	particleSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	particleSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	particleSamplers[0].ShaderRegister = 0;
	particleSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC particleRootSigDesc{};
	particleRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	particleRootSigDesc.pParameters = particleRootParams;
	particleRootSigDesc.NumParameters = _countof(particleRootParams);
	particleRootSigDesc.pStaticSamplers = particleSamplers;
	particleRootSigDesc.NumStaticSamplers = _countof(particleSamplers);

	ID3DBlob* particleSigBlob = nullptr;
	ID3DBlob* particleErrBlob = nullptr;
	hr = D3D12SerializeRootSignature(&particleRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &particleSigBlob, &particleErrBlob);
	assert(SUCCEEDED(hr));

	//Microsoft::WRL::ComPtr<ID3D12RootSignature> particleRootSignature;
	hr = dxCommon->GetDevice()->CreateRootSignature(0, particleSigBlob->GetBufferPointer(), particleSigBlob->GetBufferSize(), IID_PPV_ARGS(&dxCommon->particleRootSignature));
	assert(SUCCEEDED(hr));
	 
	//----------------------------------------------------------
	// Line Vertex buffer

	//////////////////////// 1. Define 1 Root Parameter: a CBV at register b0 (Vertex Shader visibility)
	//////////////////////D3D12_ROOT_PARAMETER lineRootParameters[1]{};
	//////////////////////lineRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	//////////////////////lineRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	//////////////////////lineRootParameters[0].Descriptor.ShaderRegister = 0; // b0
	//////////////////////lineRootParameters[0].Descriptor.RegisterSpace = 0;
	//////////////////////
	//////////////////////// 2. Describe the Root Signature (MUST allow Input Assembler for vertex buffers)
	//////////////////////D3D12_ROOT_SIGNATURE_DESC lineRootSigDesc{};
	//////////////////////lineRootSigDesc.NumParameters = _countof(lineRootParameters);
	//////////////////////lineRootSigDesc.pParameters = lineRootParameters;
	//////////////////////lineRootSigDesc.NumStaticSamplers = 0;
	//////////////////////lineRootSigDesc.pStaticSamplers = nullptr;
	//////////////////////lineRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	//////////////////////
	//////////////////////// 3. Serialize and Create
	//////////////////////ComPtr<ID3DBlob> signatureBlob;
	//////////////////////ComPtr<ID3DBlob> errorBlob;
	//////////////////////HRESULT hr = D3D12SerializeRootSignature(
	//////////////////////	&lineRootSigDesc,
	//////////////////////	D3D_ROOT_SIGNATURE_VERSION_1,
	//////////////////////	&signatureBlob,
	//////////////////////	&errorBlob
	//////////////////////);
	//////////////////////if (FAILED(hr)) {
	//////////////////////	if (errorBlob) {
	//////////////////////		OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
	//////////////////////	}
	//////////////////////	assert(false);
	//////////////////////}
	//////////////////////
	//////////////////////hr = device->CreateRootSignature(
	//////////////////////	0,
	//////////////////////	signatureBlob->GetBufferPointer(),
	//////////////////////	signatureBlob->GetBufferSize(),
	//////////////////////	IID_PPV_ARGS(&lineRootSignature)
	//////////////////////);
	//////////////////////assert(SUCCEEDED(hr));
	//////////////////////
	//////////////////////
	//////////////////////// Input Layout matching LineVertex { Vector3 pos; Vector4 color; }
	//////////////////////D3D12_INPUT_ELEMENT_DESC inputElements[] = {
	//////////////////////	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//////////////////////	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//////////////////////};
	//////////////////////
	//////////////////////D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	//////////////////////psoDesc.pRootSignature = lineRootSignature.Get();
	//////////////////////psoDesc.InputLayout = { inputElements, _countof(inputElements) };
	//////////////////////psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	//////////////////////
	//////////////////////
	//////////////////////
	//////////////////////const UINT byteSize = UINT(lines.size() * sizeof(LineVertex));
	//////////////////////
	//////////////////////ComPtr<ID3D12Resource> lineVB = dxCommon->CreateBufferResource(byteSize);
	//////////////////////
	//////////////////////LineVertex* mapped = nullptr;
	//////////////////////lineVB->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	//////////////////////memcpy(mapped, lines.data(), byteSize);
	//////////////////////lineVB->Unmap(0, nullptr);
	//////////////////////
	//////////////////////D3D12_VERTEX_BUFFER_VIEW lineVBV{};
	//////////////////////lineVBV.BufferLocation = lineVB->GetGPUVirtualAddress();
	//////////////////////lineVBV.SizeInBytes = byteSize;
	//////////////////////lineVBV.StrideInBytes = sizeof(LineVertex);
	//////////////////////
	//////////////////////UINT lineVertexCount = UINT(lines.size());


	//---------------------------------------------------------

	//Index (combine 2 triangles)
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);
	indexResourceSprite->SetName(L"indexResourceSprite");
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
	
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;
	
	//WVP Resource (For use for one size)
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	wvpResource->SetName(L"wvpResource");
	Matrix4x4* wvpData = nullptr;
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	MakeIdentity4x4(*wvpData);	//Identity matrix for WVP (World-View-Projection)
	
	//Material Resource (For simplicity, using a single color as material data. In a real application, this would likely be more complex and involve textures, multiple parameters, etc.)
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource->SetName(L"materialResource");
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; //-----Color of the Triangle--------//
	materialData->uvTransform = { //---UV Transform of the Triangle----//
			1.0f, 0.0f, 0.0f, 
			0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 1.0f
	};
	
	//Material Resource for Lighting 
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = dxCommon->CreateBufferResource(sizeof(Material));
	materialResourceSprite->SetName(L"materialResourceSprite");
	Material* materialSpriteData = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialSpriteData));
	materialSpriteData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; //Lighting color
	materialSpriteData->enableLighting = 0;
	materialSpriteData->lightingMode = 1;
	materialSpriteData->halfLambertPower = 2.0f;
	//materialSpriteData->uvTransform = {
	//	 1.0f, 1.0f, 1.0f,
	//	 0.0f, 0.0f, 0.0f,
	//	 0.0f, 0.0f, 0.0f 
	//};
	
	//Directional light values
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceLight = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	materialResourceLight->SetName(L"materialResourceLight");
	DirectionalLight* directionalLightData = nullptr;
	materialResourceLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 2.0f };
	directionalLightData->intensity = 1.0f;
	float halfLambertPower = 2.0f;

	

	ModelData modelDataInstance;
	modelDataInstance.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//top left
	modelDataInstance.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//top right
	modelDataInstance.vertices.push_back({ .position = {1.0f, -1.0f, 0.0, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//bottom left
	modelDataInstance.vertices.push_back({ .position = {1.0f, -1.0f, 0.0, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//bottom left
	modelDataInstance.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//top right
	modelDataInstance.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//bottom right
	//modelDataInstance.material.textureFilePatch = "./resources/uvChecker.png";

	Microsoft::WRL::ComPtr<ID3D12Resource> instanceVertexResource =
		dxCommon->CreateBufferResource(sizeof(VertexData) * modelDataInstance.vertices.size());

	D3D12_VERTEX_BUFFER_VIEW instanceVertexBufferView{};
	instanceVertexBufferView.BufferLocation = instanceVertexResource->GetGPUVirtualAddress();
	instanceVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelDataInstance.vertices.size());
	instanceVertexBufferView.StrideInBytes = sizeof(VertexData);
	{
		VertexData* mapped = nullptr;
		instanceVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		std::memcpy(mapped, modelDataInstance.vertices.data(),
			sizeof(VertexData)* modelDataInstance.vertices.size());
		instanceVertexResource->Unmap(0, nullptr);
	}

	std::list<Particle> particles;
	Emitter emitter{};
	emitter.count = 3;
	emitter.frequency = 0.5f;
	emitter.frequencyTime = 0.0f;
	emitter.transform.translate = { 0.0f, 0.0f, 0.f };
	emitter.transform.rotate = { 0.0f, 0.0f, 0.0f };
	emitter.transform.scale = { 1.0f, 1.0f, 1.0f };

	particles.splice(particles.end(), Emit(emitter, randomEngine, materialSpriteData->color));
	///	particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
	
	const uint32_t kNumInstanceMax = 100;
	uint32_t numInstance = 10;
	
	bool isUpdate = false;
	float spacing = 0.1f;
	Vector3 offset = { 0.2f, 0.3f, 0.0f };
	///for (uint32_t index = 0; index < kNumInstanceMax; ++index) {
	///	//particles[index].transform.scale = { 1.0f, 1.0f, 1.0f };
	///	//particles[index].transform.rotate = { 0.0f, 0.0f, 0.0f };
	///	//particles[index].transform.translate = { index * spacing, index * spacing, index * spacing };
	///	
	///	particles.push_back(MakeNewParticle(randomEngine, emitter.transform.translate));
	///
	///	std::uniform_real_distribution<float> distTime(1.0f, 3.0f);
	///	particles[index].lifeTime = distTime(randomEngine);
	///	particles[index].currentTime = 0;
	///}
	///
	///std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	///for (uint32_t index = 0; index < kNumInstanceMax; ++index) {
	///	particles[index].transform.translate = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	///	particles[index].velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
	///}


	Microsoft::WRL::ComPtr<ID3D12Resource> instanceWVPResource =
		dxCommon->CreateBufferResource(sizeof(ParticleForGPU) * kNumInstanceMax);
	ParticleForGPU* instancingData = nullptr;
	instanceWVPResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));

	for (uint32_t index = 0; index < numInstance; ++index) {
		instancingData[index].WVP = MakeIdentity4x4(instancingData[index].WVP);
		instancingData[index].World = MakeIdentity4x4(instancingData[index].World);
	}

	//Serialize and make into binary
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	//if (FAILED(hr)) {
	//	Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
	//	assert(false);
	//}
	//Create Root Signature from binary
	//Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = dxCommon->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&dxCommon->rootSignature));
	assert(SUCCEEDED(hr));

	//Input Layout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//Blend State
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;
	//blendDesc.RenderTarget[0].BlendEnable = TRUE;
	//blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	//blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	//blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	//blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	//blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	//blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	//Rasterizer State
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;		//D3D12_CULL_MODE_BACK;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Shader and Compile
	// Object3d (Model 1)
	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(L"Object3d.VS.hlsl", L"vs_6_0");
	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(L"Object3d.PS.hlsl", L"ps_6_0");
	//assert(vertexShaderBlob != nullptr);
	//assert(pixelShaderBlob != nullptr);
	assert(vertexShaderBlob && pixelShaderBlob);

	// Particle (Model 2)
	ComPtr<IDxcBlob> particleVSBlob = dxCommon->CompileShader(L"Particle.VS.hlsl", L"vs_6_0");
	ComPtr<IDxcBlob> particlePSBlob = dxCommon->CompileShader(L"Particle.PS.hlsl", L"ps_6_0");
	assert(particleVSBlob && particlePSBlob);

	//////////D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	//////////dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//////////dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	//////////dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	//////////device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	////////////
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = false;

	/*------------PSO end--------------*/

	//Create PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = dxCommon->rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;

	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };

	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };

	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //_SRGB
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = UINT_MAX; //D3D12_DEFAULT_SAMPLE_MASK;

	//Depth Stencil State
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStates[kCountOfBlendMode];
	for (int i = 0; i < kCountOfBlendMode; ++i) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = graphicsPipelineStateDesc;
		psoDesc.BlendState = CreateBlendDesc(i);
	
		// Opaque writes depth; all blend modes do not
		D3D12_DEPTH_STENCIL_DESC ds = depthStencilDesc;
		ds.DepthWriteMask = (i == kBlendModeNone)
			? D3D12_DEPTH_WRITE_MASK_ALL
			: D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState = ds;
	
		hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dxCommon->graphicsPipelineStates[i]));
		assert(SUCCEEDED(hr));
	}
	//if (FAILED(hr)) {
	//	Log(std::format("PSO作成失敗！ HRESULT = 0x{:08X}\n", static_cast<unsigned int>(hr)));
	//	__debugbreak();   // ここで停止
	//}
	
	//ParticlePipelineStates
	Microsoft::WRL::ComPtr<ID3D12PipelineState> particlePipelineStates[kCountOfBlendMode];
	for (int i = 0; i < kCountOfBlendMode; ++i) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = graphicsPipelineStateDesc;	//copy base
		psoDesc.pRootSignature = dxCommon->particleRootSignature.Get();
		psoDesc.VS = { particleVSBlob->GetBufferPointer(), particleVSBlob->GetBufferSize() };
		psoDesc.PS = { particlePSBlob->GetBufferPointer(), particlePSBlob->GetBufferSize() };
		psoDesc.BlendState = CreateBlendDesc(i);
	
		D3D12_DEPTH_STENCIL_DESC ds = depthStencilDesc;
		ds.DepthWriteMask = (i == kBlendModeNone)
			? D3D12_DEPTH_WRITE_MASK_ALL
			: D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState = ds;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	
		hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dxCommon->particlePipelineStates[i]));
		assert(SUCCEEDED(hr));
	}

	//Vertex Resource
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeof(VertexData) * 6;
	vertexResourceDesc.Height = 1;
	
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	///////
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = dxCommon->CreateBufferResource(sizeof(VertexData) * 6);
	vertexResourceSprite->SetName(L"vertexResourceSprite");

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[0].normal.x = vertexDataSprite[0].position.x;
	vertexDataSprite[0].normal.y = vertexDataSprite[0].position.y;
	vertexDataSprite[0].normal.z = vertexDataSprite[0].position.z;

	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[1].normal.x = vertexDataSprite[1].position.x;
	vertexDataSprite[1].normal.y = vertexDataSprite[1].position.y;
	vertexDataSprite[1].normal.z = vertexDataSprite[1].position.z;

	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[2].normal.x = vertexDataSprite[2].position.x;
	vertexDataSprite[2].normal.y = vertexDataSprite[2].position.y;
	vertexDataSprite[2].normal.z = vertexDataSprite[2].position.z;

	vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[3].normal.x = vertexDataSprite[3].position.x;
	vertexDataSprite[3].normal.y = vertexDataSprite[3].position.y;
	vertexDataSprite[3].normal.z = vertexDataSprite[3].position.z;

	vertexDataSprite[4].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[4].normal.x = vertexDataSprite[4].position.x;
	vertexDataSprite[4].normal.y = vertexDataSprite[4].position.y;
	vertexDataSprite[4].normal.z = vertexDataSprite[4].position.z;

	vertexDataSprite[5].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[5].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[5].normal.x = vertexDataSprite[5].position.x;
	vertexDataSprite[5].normal.y = vertexDataSprite[5].position.y;
	vertexDataSprite[5].normal.z = vertexDataSprite[5].position.z;

	////transform 
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResourceSprite->SetName(L"transformationMatrixResourceSprite");
	Matrix4x4* transformationMatrixDataSprite = nullptr;
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	*transformationMatrixDataSprite = MakeIdentity4x4(*transformationMatrixDataSprite);

	WorldTransform transformSprite{ { 1.0f, 1.0f, 1.0f}, { 0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f} };

	//-------------------Make model data *start*

	
	// Create dedicated WVP constant buffer for sphere
	Microsoft::WRL::ComPtr<ID3D12Resource> modelWVPResource[2];
	TransformationMatrix* modelTransformData[2] = {};

	for (int i = 0; i < 2; ++i) {
		modelWVPResource[i] = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
		modelWVPResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&modelTransformData[i]));
	}

	//-------------------Make model data *end*

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc0{};
	srvDesc0.Format = metadata0.format;
	srvDesc0.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc0.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc0.Texture2D.MipLevels = UINT(metadata0.mipLevels);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc1{};
	srvDesc1.Format = metadata1.format;
	srvDesc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc1.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc1.Texture2D.MipLevels = UINT(metadata1.mipLevels);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata3.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc3.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc4{};
	srvDesc4.Format = metadata4.format;
	srvDesc4.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc4.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc4.Texture2D.MipLevels = UINT(metadata4.mipLevels);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc5{};
	srvDesc5.Format = metadata5.format;
	srvDesc5.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc5.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc5.Texture2D.MipLevels = UINT(metadata5.mipLevels);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc6{};
	srvDesc6.Format = metadata6.format;
	srvDesc6.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc6.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc6.Texture2D.MipLevels = UINT(metadata6.mipLevels);

	//instancing SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstanceMax;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	

	////Process screen Back Buffer to Front Buffer (Screen Swap)
	
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU0 = dxCommon->GetSRVCPUDescriptorHandle(1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU0 = dxCommon->GetSRVGPUDescriptorHandle(1);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource0.Get(), &srvDesc0, textureSrvHandleCPU0);
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU1 = dxCommon->GetSRVCPUDescriptorHandle(2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU1 = dxCommon->GetSRVGPUDescriptorHandle(2);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource1.Get(), &srvDesc1, textureSrvHandleCPU1);
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetSRVCPUDescriptorHandle(3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetSRVGPUDescriptorHandle(3);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = dxCommon->GetSRVCPUDescriptorHandle(4);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = dxCommon->GetSRVGPUDescriptorHandle(4);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU4 = dxCommon->GetSRVCPUDescriptorHandle(5);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU4 = dxCommon->GetSRVGPUDescriptorHandle(5);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource4.Get(), &srvDesc4, textureSrvHandleCPU4);
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU5 = dxCommon->GetSRVCPUDescriptorHandle(6);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU5 = dxCommon->GetSRVGPUDescriptorHandle(6);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource5.Get(), &srvDesc5, textureSrvHandleCPU5);
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU6 = dxCommon->GetSRVCPUDescriptorHandle(7);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU6 = dxCommon->GetSRVGPUDescriptorHandle(7);
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource6.Get(), &srvDesc6, textureSrvHandleCPU6);
	
	// Instance structured buffer SRV for Particle VS (use free descriptor index 7; avoid texture slots)
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = dxCommon->GetSRVCPUDescriptorHandle(8);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = dxCommon->GetSRVGPUDescriptorHandle(8);
	dxCommon->GetDevice()->CreateShaderResourceView(instanceWVPResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);
	//
	int currentBlendMode = kBlendModeNormal;
	//float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

	bool isLight = false;

	bool showSprite = false;

	bool showModel1 = false;
	bool showModel2 = true;

	//////////----------select model and texture data -------------------
	int selectModel[2] = { 0, 0 };
	int modelTexture[2][8] = {
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
	};

	WorldTransform transformModel[2] = {
		{{ 1.0f, 1.0f, 1.0f}, { 0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 5.0f}},
		{{ 1.0f, 1.0f, 1.0f}, { 0.0f, 0.0f, 0.0f }, {-2.0f, 0.0f, 5.0f}},
	};

	const char* modelNames[] = { "plane.obj", "multiMesh.obj", "axis.obj", "multiMaterial.obj" , "Sphere.obj", "utah_teapot.obj", "bunny.obj", "suzanne.obj", "fence.obj"};
	const char* textureNames[] = { "uvChecker", "monsterball", "sample", "checkerboard", "grid", "fence", "particle" };

	D3D12_GPU_DESCRIPTOR_HANDLE textureHandles[7] = {
		textureSrvHandleGPU0,
		textureSrvHandleGPU1,
		textureSrvHandleGPU2,
		textureSrvHandleGPU3,
		textureSrvHandleGPU4,
		textureSrvHandleGPU5,
		textureSrvHandleGPU6,
	};

	const int kModelCount = 9;
	
	ModelData modelData[kModelCount]; 
	modelData[0] = LoadObjFile("Resources", "plane.obj");			// [0] 2D PLANE
	modelData[1] = LoadObjFile("Resources", "multiMesh.obj");		// [1] MULTI-MESH
	modelData[2] = LoadObjFile("Resources", "axis.obj");			// [2] AXIS
	modelData[3] = LoadObjFile("Resources", "multiMaterial.obj");	// [3] MULTI MATERIAL
	modelData[5] = LoadObjFile("Resources", "utah_teapot.obj");		// [5] UTAH_TEAPOT
	modelData[6] = LoadObjFile("Resources", "bunny.obj");			// [6] STANFORD_BUNNY
	modelData[7] = LoadObjFile("Resources", "suzanne.obj");			// [7] SUZANNE
	modelData[8] = LoadObjFile("Resources", "fence.obj");			// [8] FENCE
		{
			SphereMesh sphere = DrawSphere(16, 1);					// [4] SPHERE
			modelData[4].vertices = std::move(sphere.vertices);
		}
	
	
	Microsoft::WRL::ComPtr<ID3D12Resource> modelVertexResources[kModelCount];
	D3D12_VERTEX_BUFFER_VIEW modelVertexBufferViews[kModelCount];

	for (int i = 0; i < kModelCount; ++i) {

		// Never create a zero-size buffer
		size_t vertexCount = modelData[i].vertices.size();
		if (vertexCount == 0) {
			// keep the resource null / invalid so we can skip drawing later
			modelVertexResources[i].Reset();
			modelVertexBufferViews[i] = {};
			continue;
		}

		modelVertexResources[i] = dxCommon->CreateBufferResource(sizeof(VertexData) * vertexCount);

		modelVertexBufferViews[i].BufferLocation =
			modelVertexResources[i]->GetGPUVirtualAddress();
		modelVertexBufferViews[i].SizeInBytes =
			UINT(sizeof(VertexData) * modelData[i].vertices.size());
		modelVertexBufferViews[i].StrideInBytes = sizeof(VertexData);

		VertexData* mapped = nullptr;
		modelVertexResources[i]->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		std::memcpy(mapped, modelData[i].vertices.data(),
			sizeof(VertexData) * modelData[i].vertices.size());
		modelVertexResources[i]->Unmap(0, nullptr);
	}
	////////////////-----------------------------------------------

	dxCommon->ExecuteAndWait();
	

	//Log("Complete create D3D12Device!!!\n");

	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {

	//while (true){
	//	if (winApp->ProcessMessage()) { break; }
	//
		input->Update();
		if (input->TriggerKey(DIK_0)) {
			OutputDebugStringA("Hit 0\n");
		}
		

		//camera
		Matrix4x4 viewMatrix;
		Matrix4x4 cameraMatrix;

		if (useDebugCamera) {
			DebugCamera::Matrix4x4 debugView = debugCamera.GetViewMatrix();
			for (int i = 0; i < 4; ++i)
				for (int j = 0; j < 4; ++j)
					viewMatrix.m[i][j] = debugView.m[i][j];
		}
		else {
			cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			viewMatrix = Inverse(cameraMatrix);
		}

		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f,
			float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);


		Matrix4x4 backToFrontMatrix = MakeIdentity4x4(backToFrontMatrix);

		backToFrontMatrix.m[0][0] = viewMatrix.m[0][0];
		backToFrontMatrix.m[0][1] = viewMatrix.m[1][0];
		backToFrontMatrix.m[0][2] = viewMatrix.m[2][0];

		backToFrontMatrix.m[1][0] = viewMatrix.m[0][1];
		backToFrontMatrix.m[1][1] = viewMatrix.m[1][1];
		backToFrontMatrix.m[1][2] = viewMatrix.m[2][1];

		backToFrontMatrix.m[2][0] = viewMatrix.m[0][2];
		backToFrontMatrix.m[2][1] = viewMatrix.m[1][2];
		backToFrontMatrix.m[2][2] = viewMatrix.m[2][2];

		// === Update ===
		//transform.rotate.y += 0.03f;  



		Matrix4x4 viewMatrixSprite;
		MakeIdentity4x4(viewMatrixSprite);

		////WVP Matrix を作って書き込む
		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
		Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
		*transformationMatrixDataSprite = worldViewProjectionMatrixSprite;

		

		// ========== NEW: update both models ==========
		for (int i = 0; i < 2; ++i) {
			Matrix4x4 world = MakeAffineMatrix(
				transformModel[i].scale,
				transformModel[i].rotate,
				transformModel[i].translate);

			modelTransformData[i]->WVP = Multiply(world, Multiply(viewMatrix, projectionMatrix));
			modelTransformData[i]->World = world;
		}
		
		//UV Transform
		
		materialSpriteData->uvTransform = MakeUVTransformMatrix(uvTransformSprite);



		//////////////////////////////////////////////////////////////////////////////
		//  Safety Clamp (to avoid crash)
		//////////////////////////////////////////////////////////////////////////////
		emitter.count = (std::max)(0u, (std::min)(emitter.count, kNumInstanceMax));
		numInstance = (std::max)(0u, (std::min)(numInstance, kNumInstanceMax));

		if (isUpdate && useEmitter) {
			//////////////////////////////////////////////////////////////////
			// 1. Update Particles
			/////////////////////////////////////////////////////////////////
		
			for (auto it = particles.begin(); it != particles.end();){
				Particle& p = *it;

				p.transform.translate.x += p.velocity.x * kDeltaTime;
				p.transform.translate.y += p.velocity.y * kDeltaTime;
				p.transform.translate.z += p.velocity.z * kDeltaTime;
				p.currentTime += kDeltaTime;

				if (p.currentTime >= p.lifeTime) {
					it = particles.erase(it);
				}
				else {
					++it;
				}
			}

			/////////////////////////////////////////////////////////////////
			//  2. Emit new particles
			////////////////////////////////////////////////////////////////

			emitter.frequencyTime += kDeltaTime;
			if (emitter.frequency <= emitter.frequencyTime) {
				particles.splice(particles.end(), Emit(emitter, randomEngine, materialSpriteData->color));
				//emitter.frequencyTime -= emitter.frequency; /// either (interchangeable)
				emitter.frequencyTime = 0.0f;				  /// or (interchangeable)
			}
			
		}
		/////////////////////////////////////////////////////////////////
		//  3. Existing particles into instance buffer
		/////////////////////////////////////////////////////////////////

		uint32_t drawCount = 0;
		if (useEmitter){
			for (const Particle& p : particles){

				if (drawCount >= kNumInstanceMax) break;
				//if (numInstance < kNumInstanceMax) {

				float alpha = 1.0f;
				if (fadeOut) {
					alpha = 1.0f - p.currentTime / p.lifeTime;
					if (alpha < 0.0f) alpha = 0.0f;
				}
				

				Matrix4x4 local = MakeBillboardWorld(p.transform.scale, p.transform.rotate, p.transform.translate, backToFrontMatrix, useBillboard);

				Matrix4x4 parent = MakeAffineMatrix(
					transformModel[1].scale,
					transformModel[1].rotate,
					transformModel[1].translate);

				Matrix4x4 world = Multiply(local, parent);

				instancingData[drawCount].World = world;
				instancingData[drawCount].WVP = Multiply(world, Multiply(viewMatrix, projectionMatrix));
				if (randomColor) {
					instancingData[drawCount].color = {
						p.color.x,
						p.color.y,
						p.color.z,
						alpha
					};
				}
				else {
					instancingData[drawCount].color = {
						materialSpriteData->color.x,
						materialSpriteData->color.y,
						materialSpriteData->color.z,
						alpha
					};
				}
				
				++drawCount;
			}
				
		}
		else if (useInstancing){
			for (uint32_t i = 0; i < numInstance && i < kNumInstanceMax; ++i) {
				WorldTransform wt = transformModel[1];
				wt.translate.x += i * offset.x;
				wt.translate.y += i * offset.y;
				wt.translate.z += i * offset.z;

				Matrix4x4 world = MakeBillboardWorld(wt.scale, wt.rotate, wt.translate, backToFrontMatrix, useBillboard);
				
				instancingData[i].World = world;
				instancingData[i].WVP = Multiply(world, Multiply(viewMatrix, projectionMatrix));
				
				if (randomColor) {
					//std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
					instancingData[i].color = {
						instancingData[i].color.x,
						instancingData[i].color.y,
						instancingData[i].color.z,
						1.0f
					};
				}
				else {
					instancingData[i].color = materialSpriteData->color;
				}
				++drawCount;
			}
		}
		
		////////

#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			//ImGui::ShowDemoWindow();

			ImGui::Begin("UV Editor (SHARED to Model and Sprite)");
			// UV Transform
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			if (ImGui::Button("Reset UV Transform")) {
				uvTransformSprite.scale = { 1.0f, 1.0f, 1.0f };
				uvTransformSprite.rotate = { 0.0f, 0.0f, 0.0f };
				uvTransformSprite.translate = { 0.0f, 0.0f, 0.0f };
			}
			ImGui::End();

			ImGui::Begin("Sprite and Model Editor");
			ImGui::Checkbox("Enable Sprite", &showSprite);
			// --------- Sprite (visible when enabled) -------
			if (showSprite) {
				ImGui::Text("2D Sprite");
				ImGui::DragFloat3("Scale", &transformSprite.scale.x, 0.01f, 0.1f, 10.0f);
				ImGui::DragFloat3("Rotation", &transformSprite.rotate.x, 0.03f, -10.0f, 10.0f);
				if (ImGui::Button("Reset Transform")) {
					transformSprite = { {1,1,1}, {0,0,0}, {0,0,0} };
				}
			}
			ImGui::Separator();
			// ----- Model 1 (always visible) -----
			ImGui::Checkbox("Enable Model 1 (Lighting)", &showModel1);

			if (showModel1) {
				ImGui::PushID(0);
				ImGui::Text("Model 1");
				ImGui::Combo("Select Model", &selectModel[0], modelNames, IM_ARRAYSIZE(modelNames));
				ImGui::Combo("Texture", &modelTexture[0][selectModel[0]], textureNames, IM_ARRAYSIZE(textureNames));
				ImGui::DragFloat3("Scale", &transformModel[0].scale.x, 0.01f, 0.1f, 10.0f);
				ImGui::DragFloat3("Rotation", &transformModel[0].rotate.x, 0.03f, -10.0f, 10.0f);
				ImGui::DragFloat3("Position", &transformModel[0].translate.x, 0.01f, -10.0f, 10.0f);
				if (ImGui::Button("Reset Transform")) {
					transformModel[0] = { {1,1,1}, {0,0,0}, {2,0,5} };
				}
				ImGui::Checkbox("enableLighting", &isLight);
				if (isLight) {
					materialSpriteData->enableLighting = 1;

					const char* lightingModes[] = { "Lambert", "Half-Lambert" };
					ImGui::Combo("Lighting Mode", &materialSpriteData->lightingMode, lightingModes, IM_ARRAYSIZE(lightingModes));

					//if (materialSpriteData->lightingMode == 1) {
					//	ImGui::DragFloat("HalfLambert Power", &materialSpriteData->halfLambertPower, 0.05f, 0.5f, 8.0f);
					//}
					ImGui::ColorEdit4("Material Color", &materialSpriteData->color.x);

					ImGui::ColorEdit4("LightColor", &directionalLightData->color.x);
					ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x, 0.01f, -10.0f, 10.0f);
					ImGui::DragFloat("Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 3.0f);
					if (ImGui::Button("Reset Lighting")) {
						materialSpriteData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
						directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
						directionalLightData->direction = { 0.0f, -1.0f, 2.0f };
						directionalLightData->intensity = 1.0f;
						materialSpriteData->lightingMode = 1;
						materialSpriteData->halfLambertPower = 2.0f;
					}
				}
				else {
					materialSpriteData->enableLighting = 0;
				}
				ImGui::PopID();
			}
			ImGui::Separator();
			/////// ----- Model 2 (only when active) -----
			ImGui::Checkbox("Enable Model 2 (Particle/Instancing)", &showModel2);
			
			if (showModel2) {
				ImGui::PushID(1);
				ImGui::Text("Model 2");
				ImGui::Combo("Select Model", &selectModel[1], modelNames, IM_ARRAYSIZE(modelNames));
				ImGui::Combo("Texture", &modelTexture[1][selectModel[1]], textureNames, IM_ARRAYSIZE(textureNames));
				ImGui::ColorEdit4("Material Color", &materialSpriteData->color.x);
				
				ImGui::Text("Model 2 Transform");
				ImGui::DragFloat3("Scale", &transformModel[1].scale.x, 0.01f, 0.1f, 10.0f);
				ImGui::DragFloat3("Rotation", &transformModel[1].rotate.x, 0.03f, -10.0f, 10.0f);
				ImGui::DragFloat3("Position", &transformModel[1].translate.x, 0.01f, -10.0f, 10.0f);
				if (ImGui::Button("Reset Transform and Color")) {
					for (uint32_t index = 0; index < kNumInstanceMax; ++index) {
						transformModel[1] = {
							{1.0f, 1.0f, 1.0f},
							{0.0f, 0.0f, 0.0f},
							{-2.0f, 0.0f, 5.0f}
						};

						emitter.frequencyTime = 0.0f;
						particles.splice(particles.end(), Emit(emitter, randomEngine, materialSpriteData->color));
						materialSpriteData->color = { 1.0f, 1.0f, 1.0f, 1.0f };

						numInstance = 1;
					}
				}
				
				if (useEmitter) {
					ImGui::Text("Particle Creation Mode : Emitter");
					if (ImGui::Checkbox("Use Instancing", &useInstancing)) {
						useEmitter = false;
					}
					ImGui::Checkbox("Update", &isUpdate);
					ImGui::Checkbox("Fade Out", &fadeOut);
				}
				if (useInstancing) {
					ImGui::Text("Particle Creation Mode : Instancing");
					if (ImGui::Checkbox("Use Emitter", &useEmitter)) {
						useInstancing = false;
					}
				}
				ImGui::Checkbox("Use Billboard", &useBillboard);
				ImGui::Checkbox("Random Color", &randomColor);
				
				if (useInstancing && !useEmitter)
				{
					ImGui::InputInt("Instance Count", (int*)&numInstance, 1, 10);
					if (numInstance < 0) numInstance = 0;
					if (numInstance > (int)kNumInstanceMax) numInstance = kNumInstanceMax;
					if (ImGui::Button("Reset Instancing")) {
						numInstance = 10;
						offset = { 0.2f, 0.3f, 0.0f };
						
						if (randomColor) {
							std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
							materialSpriteData->color = {
								colorDist(randomEngine),
								colorDist(randomEngine),
								colorDist(randomEngine),
								1.0f
							};
						}
						else {
							instancingData[numInstance].color = materialSpriteData->color;
						}
					}
					ImGui::DragFloat3("Instance offset", &offset.x, 0.01f, -5.0f, 5.0f);
				}
				
				if (useEmitter)
				{
					static int desiredCount = 10;
					ImGui::InputInt("Particle Count", &desiredCount, 1, 10);
					if (desiredCount < 0) desiredCount = 0;
					if (desiredCount > (int)kNumInstanceMax) desiredCount = (int)kNumInstanceMax;

					int current = (int)particles.size();

					if (current < desiredCount) {
						Emitter temp = emitter;
						temp.count = desiredCount - current;
						particles.splice(particles.end(), Emit(temp, randomEngine, materialSpriteData->color));
					}
					else if (current > desiredCount) {
						int toRemove = current - desiredCount;
						for (int i = 0; i < toRemove; ++i) {
							if (!particles.empty()) {
								particles.pop_front(); // or pop_back
							}
						}
					}
					ImGui::DragFloat("Particle LifeTIme", &particleLifeTime, 0.01f, 0.05f, 20.0f);

					if (ImGui::Button("Reset Particles")) {	///add particles and erase particles that have exceeded their lifespan
						desiredCount = 10;
						drawCount = 0;
						particles.clear();
						particleLifeTime = 2.0f;
						particles.splice(particles.end(), Emit(emitter, randomEngine, materialSpriteData->color));
					}
					if (ImGui::Button("Clear Particles")) {
						particles.clear();
						desiredCount = 0;
					}
					
					ImGui::InputInt("Emit Count", (int*)&emitter.count);
					if (emitter.count < 0) emitter.count = 0;
					if (emitter.count > (int)kNumInstanceMax) emitter.count = kNumInstanceMax;
					if (ImGui::Button("Emit Once (Current Emit Count)")) {
						particles.splice(particles.end(), Emit(emitter, randomEngine, materialSpriteData->color));
					}
					if (ImGui::Button("Reset Emit Count")) {
						emitter.count = 3;
					}
					if (ImGui::Button("Clear Emit")) {
						emitter.count = 0;
					}
					ImGui::DragFloat("Frequency", &emitter.frequency, 0.01f, 0.05f, 5.0f);
					ImGui::DragFloat3("Emitter Position", &emitter.transform.translate.x, 0.01f);
					
				}
				
				ImGui::PopID();
			}
			
			ImGui::Separator();
			
			
			//ImGui::Separator();
			//ImGui::Text("Debug Camera");
			//ImGui::Checkbox("useDebugCamera", &useDebugCamera);
			//ImGui::Separator();
			//ImGui::Text("Camera Mode: %s", useDebugCamera ? "DEBUG CAMERA" : "Regular Camera");
			//if (ImGui::Button("Reset Debug Camera")) {
			//	debugCamera.Initialize();
			//}
			//ImGui::Separator();
			//ImGui::Text("DEBUG CAMERA CONTROLS (MOUSE/KEYBOARD)");
			//ImGui::Text("SPACE = Toggle Sound\nW = forward\nS = backward\nA = left\nD = right\nQ = down\nE = up\nMouse Scroll = forward/backward\nLeft Click = drag camera\nRight Click = rotate (Z-axis)");

			ImGui::End();

			ImGui::Begin("Camera Settings");
			ImGui::DragFloat3("Camera Rotate", &cameraTransform.rotate.x, 0.03f, -10.0f, 10.0f);
			ImGui::DragFloat3("Camera Position", &cameraTransform.translate.x, 0.01f, -20.0f, 20.0f);
			if (ImGui::Button("Reset Camera")) {
				cameraTransform.rotate = {1.047f, 3.142f, 0.0f};
				cameraTransform.translate = { 0.0f, 10.0f, 10.0f };
			}


			ImGui::End();

			
			ImGui::Begin("Blend Settings");
			ImGui::ColorEdit4("Background Color", dxCommon->clearColor);
			const char* blendModeNames[] = { "None", "Normal", "Add", "Subtract", "Multiply", "Screen"};
			ImGui::Combo("Blend Mode", &currentBlendMode , blendModeNames, IM_ARRAYSIZE(blendModeNames));
			if (ImGui::Button("Reset Blend Mode")) {
				currentBlendMode = kBlendModeNormal;
				dxCommon->clearColor[0] = 0.1f;
				dxCommon->clearColor[1] = 0.25f;
				dxCommon->clearColor[2] = 0.5f;
				dxCommon->clearColor[3] = 1.0f;
			}

			//satety clamp
			if (currentBlendMode < 0) currentBlendMode = 0;
			if (currentBlendMode >= kCountOfBlendMode) currentBlendMode = kCountOfBlendMode - 1;
			
			ImGui::End();

			ImGui::Render();
#endif

			dxCommon->PreDraw();

			ID3D12GraphicsCommandList* cmd = dxCommon->GetCommandList();

			assert(dxCommon->rootSignature);
			assert(dxCommon->particleRootSignature);
			assert(dxCommon->graphicsPipelineStates[currentBlendMode]);
			assert(dxCommon->particlePipelineStates[currentBlendMode]);


			// ---- safety clamps (put this once, before the if(showModel1) / if(showModel2) )
			const int numModels = IM_ARRAYSIZE(modelNames);  /* size of modelData / modelNames */
			const int numTextures = IM_ARRAYSIZE(textureNames); /* size of textureHandles / textureNames */

			auto Clamp = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

			selectModel[0] = Clamp(selectModel[0], 0, numModels - 1);
			selectModel[1] = Clamp(selectModel[1], 0, numModels - 1);

			modelTexture[0][selectModel[0]] = Clamp(modelTexture[0][selectModel[0]], 0, numTextures - 1);
			modelTexture[1][selectModel[1]] = Clamp(modelTexture[1][selectModel[1]], 0, numTextures - 1);


			//// 2. ModelData
			if (showModel1) {
				cmd->SetGraphicsRootSignature(dxCommon->rootSignature.Get());
				cmd->SetPipelineState(dxCommon->graphicsPipelineStates[currentBlendMode].Get());

				cmd->SetGraphicsRootConstantBufferView(1, materialResourceSprite->GetGPUVirtualAddress());
				cmd->SetGraphicsRootConstantBufferView(3, materialResourceLight->GetGPUVirtualAddress());
				
				int idx = selectModel[0];
				if (modelData[idx].vertices.size() > 0 && modelVertexResources[idx]) {
					cmd->SetGraphicsRootConstantBufferView(0, modelWVPResource[0]->GetGPUVirtualAddress());
					cmd->SetGraphicsRootDescriptorTable(2, textureHandles[modelTexture[0][idx]]);
					cmd->IASetVertexBuffers(0, 1, &modelVertexBufferViews[idx]);
					cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					cmd->DrawInstanced(UINT(modelData[idx].vertices.size()), 1, 0, 0);
				}
			}

			OutputDebugStringA(std::format("Model2 idx={} verts={} tex={}\n",
				selectModel[1],
				modelData[selectModel[1]].vertices.size(),
				modelTexture[1][selectModel[1]]).c_str());

			if (showModel2 && drawCount > 0) {
				// Particle root : [0] = VS structured buffer, [1] = PS material, [2] = PS texture
				cmd->SetGraphicsRootSignature(dxCommon->particleRootSignature.Get());
				cmd->SetPipelineState(dxCommon->particlePipelineStates[currentBlendMode].Get());
				
				cmd->SetGraphicsRootDescriptorTable(0, instancingSrvHandleGPU);
				cmd->SetGraphicsRootConstantBufferView(1, materialResourceSprite->GetGPUVirtualAddress());

				int idx = selectModel[1];
				if (modelData[idx].vertices.size() > 0 && modelVertexResources[idx]) {
					cmd->SetGraphicsRootDescriptorTable(2, textureHandles[modelTexture[1][idx]]);
					cmd->IASetVertexBuffers(0, 1, &modelVertexBufferViews[idx]);
					cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					cmd->DrawInstanced(UINT(modelData[idx].vertices.size()), drawCount, 0, 0); //change 1 to kNumInstance (Particle )
				}
			}
			// Switch back to Object3d for sprite
			cmd->SetGraphicsRootSignature(dxCommon->rootSignature.Get());
			cmd->SetPipelineState(dxCommon->graphicsPipelineStates[currentBlendMode].Get());
			cmd->SetGraphicsRootConstantBufferView(1, materialResourceSprite->GetGPUVirtualAddress());
			cmd->SetGraphicsRootConstantBufferView(3, materialResourceLight->GetGPUVirtualAddress());

			//// 3. Draw Sprite - Orthographic (2D overlay)
			if (showSprite) {
				cmd->SetGraphicsRootConstantBufferView(0, transformationMatrixResourceSprite->GetGPUVirtualAddress());
				cmd->SetGraphicsRootDescriptorTable(2, textureHandles[modelTexture[0][selectModel[0]]]);
				cmd->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
				cmd->IASetIndexBuffer(&indexBufferViewSprite);
				cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				cmd->DrawInstanced(6, 1, 0, 0);
			}

			//////////////////////////cmd->SetGraphicsRootSignature(lineRootSignature.Get());
			//////////////////////////cmd->SetPipelineState(linePipelineState.Get());
			//////////////////////////
			//////////////////////////// identity world → just view * projection
			//////////////////////////Matrix4x4 wvp = Multiply(viewMatrix, projectionMatrix);
			//////////////////////////// upload wvp to a small constant buffer and bind it to root parameter 0
			//////////////////////////
			//////////////////////////cmd->IASetVertexBuffers(0, 1, &lineVBV);
			//////////////////////////cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			//////////////////////////cmd->DrawInstanced(lineVertexCount, 1, 0, 0);
			
		#ifdef USE_IMGUI
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());
		#endif

			dxCommon->PostDraw();
		}
	}

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
	
	////Close Handle / Close Window
	//CloseHandle(fenceEvent);

	SoundUnload(&soundData1);
	if (pSourceVoice) {
		pSourceVoice->Stop(0);
		pSourceVoice->DestroyVoice();
		pSourceVoice = nullptr;
	}

	xAudio2.Reset();

	// ---- Explicit ComPtr::Reset (order roughly reverse of creation) ----
	for (int i = 0; i < kModelCount; ++i)
		modelVertexResources[i].Reset();

	for (int i = 0; i < 2; ++i)
		modelWVPResource[i].Reset();

	vertexResourceSprite.Reset();
	indexResourceSprite.Reset();
	transformationMatrixResourceSprite.Reset();
	wvpResource.Reset();
	materialResource.Reset();
	materialResourceSprite.Reset();
	materialResourceLight.Reset();

	intermediateResource0.Reset();
	intermediateResource1.Reset();
	intermediateResource2.Reset();
	intermediateResource3.Reset();
	intermediateResource4.Reset();
	intermediateResource5.Reset();

	textureResource0.Reset();
	textureResource1.Reset();
	textureResource2.Reset();
	textureResource3.Reset();
	textureResource4.Reset();
	textureResource5.Reset();

	dxCommon->rootSignature.Reset();
	dxCommon->particleRootSignature.Reset();


//#ifdef _DEBUG
//	Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
//	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
//		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
//		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
//		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
//		debug->Release();
//	}
//#endif
//
//#ifdef _DEBUG
//	debugController->Release();
//#endif
//#ifdef _DEBUG
//	Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
//	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
//		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
//	}
//#endif

	CoUninitialize();
	CloseWindow(winApp->GetHwnd());

	delete input;
	delete dxCommon;
	delete winApp;

	return 0;
}

