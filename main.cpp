#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include <vector>
#include <strsafe.h>
#include <dbghelp.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cmath>
#include <xaudio2.h>
#include "DebugCamera.h"
//#define DIRECTINPUT_VERSION	0x800
//#include <dinput.h>
#include <wrl.h>

#include "Input.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif


#pragma comment(lib, "xaudio2.lib")

#pragma comment(lib, "dxcompiler.lib")

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Dbghelp.lib")

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#pragma comment(lib, "DirectXTex.lib")

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

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

bool useDebugCamera = false;

enum BlendMode {
	kBlendModeNone,
	kBlendModeNormal,
	kBlendModeAdd,
	kBlendModeSubtract,
	kBlendModeMultiply,
	kBlendModeScreen,
	kCountOfBlendMode,
};

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
		else if (identifier == "f") {
			//Triangle only (use something else for other shapes)
			VertexData triangle[3];
			int validVertices = 0;
			
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;

				if (!(s >> vertexDefinition)) break;   // not enough tokens

				//[position/UV/Vector]
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
				if (elementIndices[0] == 0 || elementIndices[0] > positions.size()) {
					continue;   // skip this vertex
				}
				// Load vertices, normals, texture coordinates, and material data from the OBJ file
				Vector4 position = positions[elementIndices[0] - 1];
				position.x *= 1.0f;

				Vector2 texcoord = { 0.0f, 0.0f };
				if (elementIndices[1] > 0 && elementIndices[1] <= texcoords.size()) {
					texcoord = texcoords[elementIndices[1] - 1];
					texcoord.y = 1.0f - texcoord.y; // flip V
				}

				Vector3 normal = { 0.0f, 0.0f, 1.0f };
				if (elementIndices[2] > 0 && elementIndices[2] <= normals.size()) {
					normal = normals[elementIndices[2] - 1];
					normal.x *= -1.0f; // keep your existing flip
				}
				VertexData vertex = { position, texcoord, normal };
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = vertex;
			}
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
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

WorldTransform transform = {
	{ 1.0f, 1.0f, 1.0f }, // scale
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 0.0f, 0.0f, 0.0f }  // translate
};

WorldTransform cameraTransform = {
	{ 1.0f, 1.0f, 1.0f }, // scale
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 0.0f, 0.0f, -5.0f } // translate (move camera back along z-axis)
};

WorldTransform uvTransformSprite = { 
	{ 1.0f, 1.0f, 1.0f }, // scale
	{ 0.0f, 0.0f, 0.0f }, // rotate
	{ 0.0f, 0.0f, 0.0f },  // translate
};

BYTE gKeys[256] = {};
DIMOUSESTATE gMouseState = {};
//LPDIRECTINPUT8 gDirectInput = nullptr;
//LPDIRECTINPUTDEVICE8 gKeyboard = nullptr;
//LPDIRECTINPUTDEVICE8 gMouse = nullptr;

//
//void InitializeInput(HWND hwnd) {
//	DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&gDirectInput, nullptr);
//
//	// Keyboard
//	gDirectInput->CreateDevice(GUID_SysKeyboard, &gKeyboard, nullptr);
//	gKeyboard->SetDataFormat(&c_dfDIKeyboard);
//	gKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
//
//	// Mouse
//	gDirectInput->CreateDevice(GUID_SysMouse, &gMouse, nullptr);
//	gMouse->SetDataFormat(&c_dfDIMouse);
//	gMouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
//}


// Forward declarations to fix C3861: 'ConvertString' identifier not found
std::wstring ConvertString(const std::string& str);
std::string ConvertString(const std::wstring& str);

void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}

void Log(const std::wstring& message) {
	Log(ConvertString(message));
}

void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

//std::string texturePath = "assets/texture.png";


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

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}


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

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	return EXCEPTION_EXECUTE_HANDLER;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

//Shader compilation example (Before main loop)
IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,

	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)

	{ ////-----------CompileShader function start-----------/////
		//1. hlsl file read

	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

IDxcBlobEncoding* shaderSource = nullptr;
HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
//assert(SUCCEEDED(hr));
if (FAILED(hr)) {
	Log(std::format("Failed to load shader file: {}\n", ConvertString(filePath)));
	Log(std::format("HRESULT = 0x{:08X}\n", static_cast<unsigned int>(hr)));
}

DxcBuffer shaderSourceBuffer;
shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
shaderSourceBuffer.Size = shaderSource->GetBufferSize();
shaderSourceBuffer.Encoding = DXC_CP_UTF8;
//2. compile
LPCWSTR arguments[] = {
	filePath.c_str(),//entry point (hlsl file name)
	L"-E", L"main",//entry point path. Fundamentally does not go outside main
	L"-T", profile,//Shader profile settings
	L"-Zi", L"-Qembed_debug",//debug information is buried
	L"-Od",		//optimization disabled
	L"-Zpr",	//memory out (priority)
};

IDxcResult* shaderResult = nullptr;
hr = dxcCompiler->Compile(
	&shaderSourceBuffer,
	arguments,
	_countof(arguments),
	includeHandler,
	IID_PPV_ARGS(&shaderResult)
);
assert(SUCCEEDED(hr));

HRESULT compileStatus = E_FAIL;
shaderResult->GetStatus(&compileStatus);

//3. check for no warnings or error
IDxcBlobUtf8* shaderError = nullptr;
shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
	Log("=== Shader Compiler Output ===");
	Log(shaderError->GetStringPointer());
	shaderError->Release();
}

if (FAILED(compileStatus)) {
	Log("=== Shader Compilation FAILED ===\n");
	return nullptr;
}
//4. compile results return
IDxcBlob* shaderBlob = nullptr;
hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
assert(SUCCEEDED(hr) && shaderBlob != nullptr && shaderBlob->GetBufferSize() > 0);

Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
shaderSource->Release();
shaderResult->Release();
if (shaderError) shaderError->Release();

return shaderBlob;

} ////-----------CompileShader function end-----------/////


// Helper function to create an Upload Heap Buffer
Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const size_t sizeInBytes)
{ 
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;     
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;     

	// Resource description for buffer
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// FIXED BUG HERE: Buffers cannot be Depth Stencils!
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	resource->SetName(L"BufferResource");

	assert(SUCCEEDED(hr));
	if (FAILED(hr)) {
		Log("Failed to create buffer resource!\n");
		return nullptr;
	}
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata) {

	//1. metadata base resource settings
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//2. Heap settings
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;     
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;     

	//3. Create Resource
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		Log("Failed to create buffer resource!\n");
		return nullptr;
	}
	resource->SetName(L"TextureResource");

	return resource;
};

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

[[nodiscard]]
Microsoft::WRL::ComPtr <ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
	intermediateResource->SetName(L"intermediateResource");
	UpdateSubresources(commandList, texture, intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;
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

	SetUnhandledExceptionFilter(ExportDump);

	CoInitializeEx(0, COINIT_MULTITHREADED);

	/////-----SOUND DATA---------
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;

	HRESULT hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

	hr = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(hr));

	SoundData soundData1 = SoundLoadWave("Resources/Alarm01.wav");

	DebugCamera debugCamera;
	debugCamera.Initialize();

	Input* input = nullptr;
	input = new Input();
	input->Initialize();

	///------SOUND DATA END----------

	//Debug Layer
//ifdef _DEBUG
//	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
//	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
//		debugController->EnableDebugLayer();
//		//debugController->SetEnableGPUBasedValidation(TRUE);
//	}
//endif

	WNDCLASS wc{};

	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);

	OutputDebugStringA("Hello,DirectX!\n");

	Log(std::format("now:{:%Y/%m/%d %H:%M:%S}\n", localTime));

	std::filesystem::create_directory("logs");

	Log(logStream, std::format("start time (local time): {:%Y/%m/%d %H:%M:%S}", localTime));

	RECT wrc = { 0,0,kClientWidth,kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(
		wc.lpszClassName,		//利用するクラス名
		L"LE2C_21_ファルコン_エブラハム",		//タイトルバーの文字　（何でもよい)
		WS_OVERLAPPEDWINDOW,	//よく見るウインドウスタイル
		CW_USEDEFAULT,			//表示X座標　（Windowsに任せる）
		CW_USEDEFAULT,			//表示Y座標　（Windowsに任せる）
		wrc.right - wrc.left,	//ウインドウ横幅
		wrc.bottom - wrc.top,	//ウインドウ縦幅
		nullptr,				//親ウインドウハンドル
		nullptr,				//メニューハンドル
		wc.hInstance,			//インスタンスハンドル
		nullptr);				//オプション

	//ウインドウの表示する
	ShowWindow(hwnd, SW_SHOW);
	//ShowCursor(TRUE);

	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	//HRESULTはWindows系のエラーコードであり、
	//関数が成功したかどうかをSUCCEEDEDマクロで判定できる

	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	assert(SUCCEEDED(hr));

	//使用するアダプタ用の変数。最初にはnullptrを入れておく
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log(std::format(L"Use Adapter:{}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = {
		"12.2", "12.1", "12.0"
	};

	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {
			Log(std::format("Feature level: {}\n", featureLevelStrings[i]));
			break;
		}
	}

	assert(device != nullptr);

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		infoQueue->PushStorageFilter(&filter);
		infoQueue->Release();
	}

#endif

	//----////////DirectX Initialize process
	//----
	//----///DirectInput Setup
	//----IDirectInput8* directInput = nullptr;
	//----hr = DirectInput8Create(wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	//----assert(SUCCEEDED(hr));
	//----
	//----//Keyboard
	//----IDirectInputDevice8* keyboard = nullptr;
	//----hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	//----assert(SUCCEEDED(hr));
	//----hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	//----assert(SUCCEEDED(hr));
	//----hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	//----assert(SUCCEEDED(hr));
	//----
	//----// Mouse
	//----IDirectInputDevice8* mouse = nullptr;
	//----hr = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	//----assert(SUCCEEDED(hr));
	//----hr = mouse->SetDataFormat(&c_dfDIMouse);
	//----assert(SUCCEEDED(hr));
	//----hr = mouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	//----assert(SUCCEEDED(hr));
	//----
	//----ShowCursor(TRUE);
	//----ClipCursor(NULL);


	//dxCompiler initialization (Before main loop)
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	// not used, but create default include handler for later use
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	//Command queue
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	//Command Allocator
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	//Command List
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	///------------Load Textures ----------------


	DirectX::ScratchImage mipImages = LoadTexture("Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata0 = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource0 = CreateTextureResource(device, metadata0);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource0 = UploadTextureData(textureResource0.Get(), mipImages, device.Get(), commandList.Get());

	DirectX::ScratchImage mipImages1 = LoadTexture("Resources/monsterBall.png");
	const DirectX::TexMetadata& metadata1 = mipImages1.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource1 = CreateTextureResource(device, metadata1);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource1 = UploadTextureData(textureResource1.Get(), mipImages1, device.Get(), commandList.Get());

	DirectX::ScratchImage mipImages2 = LoadTexture("Resources/sample.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, metadata2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = UploadTextureData(textureResource2.Get(), mipImages2, device.Get(), commandList.Get());

	DirectX::ScratchImage mipImages3 = LoadTexture("Resources/checkerboard.png");
	const DirectX::TexMetadata& metadata3 = mipImages3.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource3 = CreateTextureResource(device, metadata3);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource3 = UploadTextureData(textureResource3.Get(), mipImages3, device.Get(), commandList.Get());

	DirectX::ScratchImage mipImages4 = LoadTexture("Resources/grid.png");
	const DirectX::TexMetadata& metadata4 = mipImages4.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource4 = CreateTextureResource(device, metadata4);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource4 = UploadTextureData(textureResource4.Get(), mipImages4, device.Get(), commandList.Get());


	//--------------Load Texture end --------------

	//Depth Stencil Buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
											   
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	//Swap Chain
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	//create by passing a command queue and window handle
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

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

	//-------------------------------
	//D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	//descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	//descriptorRangeForInstancing[0].NumDescriptors = 1;
	//descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	//descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	//
	//rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	//rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	//rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);
	//-----------------------------
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

	//Index (combine 2 triangles)
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
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
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	wvpResource->SetName(L"wvpResource");
	Matrix4x4* wvpData = nullptr;
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	MakeIdentity4x4(*wvpData);	//Identity matrix for WVP (World-View-Projection)

	//Material Resource (For simplicity, using a single color as material data. In a real application, this would likely be more complex and involve textures, multiple parameters, etc.)
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = CreateBufferResource(device, sizeof(Material));
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
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device, sizeof(Material));
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
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceLight = CreateBufferResource(device, sizeof(DirectionalLight));
	materialResourceLight->SetName(L"materialResourceLight");
	DirectionalLight* directionalLightData = nullptr;
	materialResourceLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 2.0f };
	directionalLightData->intensity = 1.0f;
	float halfLambertPower = 2.0f;

	//Transformation Matrix for Instancing
	const uint32_t kNumInstance = 10;

	ModelData modelDataInstance;
	modelDataInstance.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//top left
	modelDataInstance.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//top right
	modelDataInstance.vertices.push_back({ .position = {1.0f, -1.0f, 0.0, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//bottom left
	modelDataInstance.vertices.push_back({ .position = {1.0f, -1.0f, 0.0, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//bottom left
	modelDataInstance.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//top right
	modelDataInstance.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} });	//bottom right
	//modelDataParticle.material.textureFilePatch = "./resources/uvChecker.png";

	Microsoft::WRL::ComPtr<ID3D12Resource> instanceVertexResource =
		CreateBufferResource(device, sizeof(TransformationMatrix) * modelDataInstance.vertices.size());

	D3D12_VERTEX_BUFFER_VIEW instanceVertexBufferView{};
	instanceVertexBufferView.BufferLocation = instanceVertexResource->GetGPUVirtualAddress();
	instanceVertexBufferView.SizeInBytes = UINT(sizeof(TransformationMatrix) * modelDataInstance.vertices.size());
	instanceVertexBufferView.StrideInBytes = sizeof(TransformationMatrix);
	{
		TransformationMatrix* mapped = nullptr;
		instanceVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
		std::memcpy(mapped, modelDataInstance.vertices.data(),
			sizeof(TransformationMatrix)* modelDataInstance.vertices.size());
		instanceVertexResource->Unmap(0, nullptr);
	}

	WorldTransform transforms[kNumInstance];
	for (uint32_t index = 0; index < kNumInstance; ++index) {
		transforms[index].scale = { 1.0f, 1.0f, 1.0f };
		transforms[index].rotate = { 0.0f, 0.0f, 0.0f };
		transforms[index].translate = { index * 0.1f, index * 0.1f, index * 0.1f };
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> instanceWVPResource =
		CreateBufferResource(device, sizeof(TransformationMatrix) * kNumInstance);
	TransformationMatrix* instancingData = nullptr;
	instanceWVPResource->Map(0, nullptr, reinterpret_cast<void**>(&instancingData));

	for (uint32_t index = 0; index < kNumInstance; ++index) {
		instancingData[index].WVP = MakeIdentity4x4(instancingData[index].WVP);
		instancingData[index].World = MakeIdentity4x4(instancingData[index].World);
	}


	//Serialize and make into binary
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//Create Root Signature from binary
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
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
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
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
	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);
	assert(vertexShaderBlob && pixelShaderBlob);
	

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	//
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//depthStencilDesc.StencilEnable = false;

	/*------------PSO end--------------*/

	//Create PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
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

		hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&graphicsPipelineStates[i]));
		assert(SUCCEEDED(hr));
	}
	if (FAILED(hr)) {
		Log(std::format("PSO作成失敗！ HRESULT = 0x{:08X}\n", static_cast<unsigned int>(hr)));
		__debugbreak();   // ここで停止
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

	

	//Viewport and Scissor
	D3D12_VIEWPORT viewport{};
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	///////
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);
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
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));
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
		modelWVPResource[i] = CreateBufferResource(device, sizeof(TransformationMatrix));
		modelWVPResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&modelTransformData[i]));
	}

	//-------------------Make model data *end*

	//Pull Resources from Swap Chain (create rtv)
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0])); // hr =
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1])); // hr =
	assert(SUCCEEDED(hr));

	//rtv settings
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

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
	srvDesc4.Texture2D.MipLevels = UINT(metadata3.mipLevels);

	//instancing SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(TransformationMatrix);
	

	//Process screen Back Buffer to Front Buffer (Screen Swap)
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU0 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU0 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);
	device->CreateShaderResourceView(textureResource0.Get(), &srvDesc0, textureSrvHandleCPU0);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU1 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU1 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
	device->CreateShaderResourceView(textureResource1.Get(), &srvDesc1, textureSrvHandleCPU1);
	
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU3 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
	device->CreateShaderResourceView(textureResource3.Get(), &srvDesc3, textureSrvHandleCPU3);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU4 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 5);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU4 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 5);
	device->CreateShaderResourceView(textureResource4.Get(), &srvDesc4, textureSrvHandleCPU4);

	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
	device->CreateShaderResourceView(instanceWVPResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);
	//
	int currentBlendMode = kBlendModeNormal;
	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

	bool isLight = false;

	bool showSprite = true;

	bool showModel1 = true;
	bool showModel2 = false;

	//////////----------select model and texture data -------------------
	int selectModel[2] = { 0, 1 };
	int modelTexture[2][8] = {
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
	};
	WorldTransform transformModel[2] = {
		{{ 1.0f, 1.0f, 1.0f}, { 0.0f, 0.0f, 0.0f }, {2.0f, 0.0f, 5.0f}},
		{{ 1.0f, 1.0f, 1.0f}, { 0.0f, 0.0f, 0.0f }, {-2.0f, 0.0f, 5.0f}},
	};

	const char* modelNames[] = { "plane.obj", "multiMesh.obj", "axis.obj", "multiMaterial.obj" , "Sphere.obj", "utah_teapot.obj", "bunny.obj", "suzanne.obj"};
	const char* textureNames[] = { "uvChecker", "monsterball", "sample", "checkerboard", "grid"};

	D3D12_GPU_DESCRIPTOR_HANDLE textureHandles[5] = {
		textureSrvHandleGPU0,
		textureSrvHandleGPU1,
		textureSrvHandleGPU2,
		textureSrvHandleGPU3,
		textureSrvHandleGPU4,

	};

	const int kModelCount = 8;
	
	ModelData modelData[kModelCount]; 
	modelData[0] = LoadObjFile("Resources", "plane.obj");			// [0] 2D PLANE
	modelData[1] = LoadObjFile("Resources", "multiMesh.obj");		// [1] MULTI-MESH
	modelData[2] = LoadObjFile("Resources", "axis.obj");			// [2] AXIS
	modelData[3] = LoadObjFile("Resources", "multiMaterial.obj");	// [3] MULTI MATERIAL
	modelData[5] = LoadObjFile("Resources", "utah_teapot.obj");		// [5] UTAH_TEAPOT
	modelData[6] = LoadObjFile("Resources", "bunny.obj");			// [6] STANFORD_BUNNY
	modelData[7] = LoadObjFile("Resources", "suzanne.obj");			// [7] SUZANNE
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

		modelVertexResources[i] = CreateBufferResource(
			device, sizeof(VertexData) * vertexCount);

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

	
	////Fence (Before main loop)
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	//
	//Fence signal
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);
	//

	hr = commandList->Close();
	assert(SUCCEEDED(hr));
	
	//GPU
	ID3D12CommandList* commandLists[] = { commandList.Get()};
	commandQueue->ExecuteCommandLists(1, commandLists);
	swapChain->Present(1, 0);
	
	fenceValue++;
	
	commandQueue->Signal(fence.Get(), fenceValue);
	
	//Fence value for GPU
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
	
		WaitForSingleObject(fenceEvent, INFINITE);
	}

#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();
#endif


	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));

	Log("Complete create D3D12Device!!!\n");

	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
#endif

			//-----// ====== Sound and Camera Input ======== //
			//-----static BYTE prevKeys[256] = {};
			//-----keyboard->Acquire();
			//-----keyboard->GetDeviceState(sizeof(gKeys), gKeys);
			//-----
			//-----// Toggle between Debug and Regular camera (Press SPACE)
			//-----bool spaceKeyPressed = (gKeys[DIK_SPACE] & 0x80) && !(prevKeys[DIK_SPACE] & 0x80);
			//-----memcpy(prevKeys, gKeys, sizeof(prevKeys));
			//-----
			//-----if (spaceKeyPressed) {  // Key just pressed
			//-----	//useDebugCamera = !useDebugCamera;
			//-----	isSoundPlaying = !isSoundPlaying;
			//-----	SoundPlayWave(xAudio2.Get(), soundData1, isSoundPlaying);
			//-----}
			//-----
			//-----mouse->Acquire();
			//-----HRESULT hr = mouse->GetDeviceState(sizeof(DIMOUSESTATE), &gMouseState);


			//-----float mouseDeltaX = 0;
			//-----float mouseDeltaY = 0;
			//-----float mouseDeltaZ = (float)gMouseState.lZ; //wheel
			//-----
			//-----bool isLeftClick = false;
			//-----bool isRightClick = false;
			//-----
			//-----if (SUCCEEDED(hr)) {
			//-----	mouseDeltaX = (float)gMouseState.lX;
			//-----	mouseDeltaY = (float)gMouseState.lY;
			//-----	isLeftClick = (gMouseState.rgbButtons[0] & 0x80) != 0;
			//-----	isRightClick = (gMouseState.rgbButtons[1] & 0x80) != 0;
			//-----}
			//-----
			//-----if (useDebugCamera) {
			//-----	if (isLeftClick) {
			//-----		debugCamera.SetMouseDelta(mouseDeltaX, mouseDeltaY);      // Yaw + Pitch
			//-----	}
			//-----	if (isRightClick) {
			//-----		debugCamera.SetMouseRollDelta(mouseDeltaX);          // Roll with horizontal drag
			//-----	}
			//-----	if (mouseDeltaZ != 0) {
			//-----		debugCamera.SetMouseWheelDelta(mouseDeltaZ);      // Optional: forward movement
			//-----	}
			//-----	debugCamera.Update();
			//-----}
			

			// === Update ===
			//transform.rotate.y += 0.03f;  

			Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			Matrix4x4 viewMatrixSprite;
			MakeIdentity4x4(viewMatrixSprite);

			////WVP Matrix を作って書き込む
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
			*transformationMatrixDataSprite = worldViewProjectionMatrixSprite;

			//camera
			Matrix4x4 viewMatrix;
			if (useDebugCamera) {
				DebugCamera::Matrix4x4 debugView = debugCamera.GetViewMatrix();
				for (int i = 0; i < 4; ++i)
					for (int j = 0; j < 4; ++j)
						viewMatrix.m[i][j] = debugView.m[i][j];
			}
			else {
				Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
				viewMatrix = Inverse(cameraMatrix);
			}

			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f,
				float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);

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

			//if (Input::GetInstance()->TriggerKey(DIK_SPACE))
			//SoundPlayWave(xAudio2.Get(), soundData1);

			
			//BYTE key[256] = {};
			//if (key[DIK_0]) {
			//	OutputDebugStringA("Hit 0\n");
			//}

			//key input (Trigger) process
			//bool is the key being pressed? (uint8_t [key #]
			//bool the moment a key is pressed (uint8_t [key #]
			//prevents negative values and limitted to [0 - 255]

		//instancing
		for (uint32_t index = 0; index < kNumInstance; ++index) {
			Matrix4x4 worldMatrix = MakeAffineMatrix(transforms[index].scale, transforms[index].rotate, transforms[index].translate);

			Matrix4x4 viewProjectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
			instancingData[index].WVP = worldViewProjectionMatrix;
			instancingData[index].World = worldMatrix;
		}

#ifdef USE_IMGUI
			//ImGui::ShowDemoWindow();

			ImGui::Begin("Editor");
			//ImGui::Checkbox("Show 2D Sprite", &showSprite);
			//ImGui::Checkbox("Show Model 1", &showModel1);
			//ImGui::Checkbox("Show Model 2", &showModel2);
			ImGui::Separator();
			ImGui::Text("UV Transform (Shared to Model and Sprite)");
			// UV Transform
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			if (ImGui::Button("Reset UV Transform")) {
				uvTransformSprite.scale = { 1.0f, 1.0f, 1.0f };
				uvTransformSprite.rotate = { 0.0f, 0.0f, 0.0f };
				uvTransformSprite.translate = { 0.0f, 0.0f, 0.0f };
			}
			ImGui::Separator();
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
			ImGui::Checkbox("Enable Model 1", &showModel1);

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
				ImGui::PopID();
			}
			ImGui::Separator();
			/////// ----- Model 2 (only when active) -----
			/////ImGui::Checkbox("Enable Model 2", &showModel2);
			/////
			/////if (showModel2) {
			/////	ImGui::PushID(1);
			/////	ImGui::Text("Model 2");
			/////	ImGui::Combo("Select Model", &selectModel[1], modelNames, IM_ARRAYSIZE(modelNames));
			/////	ImGui::Combo("Texture", &modelTexture[1][selectModel[1]], textureNames, IM_ARRAYSIZE(textureNames));
			/////	ImGui::DragFloat3("Scale", &transformModel[1].scale.x, 0.01f, 0.1f, 10.0f);
			/////	ImGui::DragFloat3("Rotation", &transformModel[1].rotate.x, 0.03f, -10.0f, 10.0f);
			/////	ImGui::DragFloat3("Position", &transformModel[1].translate.x, 0.01f, -10.0f, 10.0f);
			/////	if (ImGui::Button("Reset Transform")) {
			/////		transformModel[1] = { {1,1,1}, {0,0,0}, {-2,0,5} };
			/////	}
			/////	ImGui::PopID();
			/////}
			/////ImGui::Separator();
			ImGui::Checkbox("enableLighting", &isLight);
			if (isLight) {
				materialSpriteData->enableLighting = 1;
			}
			else {
				materialSpriteData->enableLighting = 0;
			}
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

			
			ImGui::Begin("Blend Settings");
			ImGui::ColorEdit4("Clear Color", clearColor);
			const char* blendModeNames[] = { "None", "Normal", "Add", "Subtract", "Multiply", "Screen"};
			ImGui::Combo("Blend Mode", &currentBlendMode , blendModeNames, IM_ARRAYSIZE(blendModeNames));
			if (ImGui::Button("Reset Blend Mode")) {
				currentBlendMode = kBlendModeNormal;
				clearColor[0] = 0.1f;
				clearColor[1] = 0.25f;
				clearColor[2] = 0.5f;
				clearColor[3] = 1.0f;
			}

			//satety clamp
			if (currentBlendMode < 0) currentBlendMode = 0;
			if (currentBlendMode >= kCountOfBlendMode) currentBlendMode = kCountOfBlendMode - 1;
			
			ImGui::End();

			ImGui::Render();
#endif
			// === Render ===
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// Transition to Render Target
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			commandList->ResourceBarrier(1, &barrier);

			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);

			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetPipelineState(graphicsPipelineStates[currentBlendMode].Get());

			//set descriptor heap (required for ImGui)
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get()};
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			// Material and Texture are shared
			commandList->SetGraphicsRootConstantBufferView(1, materialResourceSprite->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(3, materialResourceLight->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootDescriptorTable(2, textureHandles[modelTexture[selectModel]]);
			

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
				int idx = selectModel[0];
				if (modelData[idx].vertices.size() > 0 && modelVertexResources[idx]) {
					commandList->SetGraphicsRootConstantBufferView(0, modelWVPResource[0]->GetGPUVirtualAddress());
					commandList->SetGraphicsRootDescriptorTable(2, textureHandles[modelTexture[0][idx]]);
					commandList->IASetVertexBuffers(0, 1, &modelVertexBufferViews[idx]);
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->DrawInstanced(UINT(modelData[idx].vertices.size()), 1, 0, 0);
				}
			}

			OutputDebugStringA(std::format("Model2 idx={} verts={} tex={}\n",
				selectModel[1],
				modelData[selectModel[1]].vertices.size(),
				modelTexture[1][selectModel[1]]).c_str());

			if (showModel2) {
				int idx = selectModel[1];
				if (modelData[idx].vertices.size() > 0 && modelVertexResources[idx]) {
					commandList->SetGraphicsRootConstantBufferView(0, modelWVPResource[1]->GetGPUVirtualAddress());
					commandList->SetGraphicsRootDescriptorTable(2, textureHandles[modelTexture[1][idx]]);
					commandList->IASetVertexBuffers(0, 1, &modelVertexBufferViews[idx]);
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->DrawInstanced(UINT(modelData[idx].vertices.size()), 1, 0, 0); //change 1 to instanceCount (Particle )
				}
			}

			//// 3. Draw Sprite - Orthographic (2D overlay)
			if (showSprite) {
				commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixResourceSprite->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, textureHandles[modelTexture[0][selectModel[0]]]);
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
				commandList->IASetIndexBuffer(&indexBufferViewSprite);
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->DrawInstanced(6, 1, 0, 0);
			}

			//// 4. instancing (lots of flat polygons)
			//for (number of Particles) {
			//	commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);
			//	commandList->DrawInstanced(UINT(modelData.vertices.size()), kNumInstance, 0, 0);
			//}
			//commandList->SetGraphicsRootSignature(rootSignature.Get());
			//commandList->SetPipelineState(graphicsPipelineStates[currentBlendMode].Get());
			//
			//commandList->SetGraphicsRootConstantBufferView(0, instancingResource->GetGPUVirtualAddress());
			//commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);	//structured buffer
			//commandList->SetGraphicsRootDescriptorTable(2, textureHandles[0]);	//texture
			//commandList->SetGraphicsRootConstantBufferView(3, materialResourceLight->GetGPUVirtualAddress());
			//
			commandList->IASetVertexBuffers(0, 1, &instanceVertexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			for (uint32_t i = 0; i < kNumInstance; ++i) {
				D3D12_GPU_VIRTUAL_ADDRESS addr = instanceWVPResource->GetGPUVirtualAddress() + i * sizeof(TransformationMatrix);

				commandList->SetGraphicsRootConstantBufferView(0, addr);
				commandList->SetGraphicsRootDescriptorTable(2, textureHandles[0]);
				commandList->DrawInstanced(6, 1, 0, 0);
			}
			//commandList->DrawInstanced(UINT(modelDataInstance.vertices.size()), kNumInstance, 0, 0);	//6 vertices + #of Instances
			//
			//for (int i = 0; i < numTextures; ++i) {
			//	OutputDebugStringA(std::format("textureHandles[{}] = 0x{:X}\n",
			//		i, textureHandles[i].ptr).c_str());
			//}

			
		#ifdef USE_IMGUI
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
		#endif

			// Transition back to Present
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			commandList->ResourceBarrier(1, &barrier);

			commandList->Close();

			// Execute
			ID3D12CommandList* commandLists[] = { commandList.Get()};
			commandQueue->ExecuteCommandLists(1, commandLists);

			swapChain->Present(1, 0);

			// Fence synchronization
			fenceValue++;
			commandQueue->Signal(fence.Get(), fenceValue);

			if (fence->GetCompletedValue() < fenceValue) {
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// Reset command list
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr));

		}
	}

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
	
	//Close Handle / Close Window
	CloseHandle(fenceEvent);

	SoundUnload(&soundData1);
	if (pSourceVoice) {
		pSourceVoice->Stop(0);
		pSourceVoice->DestroyVoice();
		pSourceVoice = nullptr;
	}

	xAudio2.Reset();


	if (commandQueue && fence) {
		fenceValue++;
		commandQueue->Signal(fence.Get(), fenceValue);
		if (fence->GetCompletedValue() < fenceValue) {
			fence->SetEventOnCompletion(fenceValue, fenceEvent);
			WaitForSingleObject(fenceEvent, INFINITE);
		}
	}

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

	textureResource0.Reset();
	textureResource1.Reset();
	textureResource2.Reset();
	textureResource3.Reset();
	textureResource4.Reset();

	depthStencilResource.Reset();
	swapChainResources[0].Reset();
	swapChainResources[1].Reset();

	srvDescriptorHeap.Reset();
	rtvDescriptorHeap.Reset();
	dsvDescriptorHeap.Reset();

	for (int i = 0; i < kCountOfBlendMode; ++i) {
		graphicsPipelineStates[i].Reset();
	}
	rootSignature.Reset();

	commandList.Reset();
	commandAllocator.Reset();
	commandQueue.Reset();
	fence.Reset();

	swapChain.Reset();          
	device.Reset();             // last

	// ---- Input ----
	//----if (mouse) { mouse->Unacquire(); mouse->Release(); mouse = nullptr; }
	//----if (keyboard) { keyboard->Unacquire(); keyboard->Release(); keyboard = nullptr; }
	//----if (directInput) { directInput->Release(); directInput = nullptr; }

	// DXC objects 
	if (includeHandler) includeHandler->Release();
	if (dxcCompiler)    dxcCompiler->Release();
	if (dxcUtils)       dxcUtils->Release();
	if (vertexShaderBlob) vertexShaderBlob->Release();
	if (pixelShaderBlob)  pixelShaderBlob->Release();
	if (signatureBlob)    signatureBlob->Release();
	if (errorBlob)        errorBlob->Release();

	delete input;


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
	CloseWindow(hwnd);

	return 0;
}

