#include "ヘッダー ファイル/engine/math/Function.h"

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

bool IsCollision(const AABB& aabb, const Vector3& point) {
	return	(point.x >= aabb.min.x && point.x <= aabb.max.x) &&
			(point.y >= aabb.min.y && point.y <= aabb.max.y) &&
			(point.z >= aabb.min.z && point.z <= aabb.max.z);
}
