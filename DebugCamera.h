#pragma once
#include <cmath>
#include <dinput.h>

class DebugCamera
{
public:

	struct Vector3 {
		float x;
		float y;
		float z;
	};

	struct WorldTransform {
		Vector3 scale;
		Vector3 rotate;
		Vector3 translate;
	};

	struct Matrix4x4 {
		float m[4][4];
	};

	void Initialize();
	void Update();
	void SetMouseDelta(float dx, float dy) {
		mouseDeltaX_ = dx * 0.0005f;
		mouseDeltaY_ = dy * 0.0005f;
	}
	void SetMouseRollDelta(float dx) {
		mouseDeltaZ_ = dx;
	}
	void SetMouseWheelDelta(float wheel) {
		mouseWheelDelta_ = wheel;
	}

	WorldTransform GetTransform() const { return transform_; }
	Matrix4x4 GetViewMatrix() const { return viewMatrix_; }

private:
	WorldTransform transform_ = {};

	Matrix4x4 matRot_;
	Matrix4x4 matRotDelta_;
	Matrix4x4 viewMatrix_;

	const float kWindowWidth = 1280.0f;
	const float kWindowHeight = 720.0f;

	Vector3 moveSpeed =		{ 0.15f, 0.15f, 0.15f }; //move multiplier
	Vector3 rotate =		{ 0.0f, 0.0f, 0.0f }; //default rotate speed
	Vector3 rotateSpeed =	{ 0.5f, 0.5f, 0.5f }; //rotate multiplier

	float yaw_ = 0.0f;
	float pitch_ = 0.0f;
	float pivot_ = 0.0f;

	float mouseDeltaX_ = 0.0f;
	float mouseDeltaY_ = 0.0f;
	float mouseDeltaZ_ = 0.0f;
	float mouseWheelDelta_ = 0.0f;
	

    Vector3 RotateVector(const Vector3& vec, const Matrix4x4& rotMat) {
        Vector3 result;
        result.x = vec.x * rotMat.m[0][0] + vec.y * rotMat.m[1][0] + vec.z * rotMat.m[2][0];
        result.y = vec.x * rotMat.m[0][1] + vec.y * rotMat.m[1][1] + vec.z * rotMat.m[2][1];
        result.z = vec.x * rotMat.m[0][2] + vec.y * rotMat.m[1][2] + vec.z * rotMat.m[2][2];
        return result;
    }

    Matrix4x4 MakeIdentityMatrix() {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        return result;
    }

    Matrix4x4 MakeTranslateMatrix(Vector3 translate) {
        Matrix4x4 result = MakeIdentityMatrix();
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
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

    Matrix4x4 MakeRotateXMatrix(float radian) {
        Matrix4x4 result;
        result.m[0][0] = 1; result.m[0][1] = 0; result.m[0][2] = 0; result.m[0][3] = 0;
        result.m[1][0] = 0; result.m[1][1] = std::cos(radian); result.m[1][2] = std::sin(radian); result.m[1][3] = 0;
        result.m[2][0] = 0; result.m[2][1] = -std::sin(radian); result.m[2][2] = std::cos(radian); result.m[2][3] = 0;
        result.m[3][0] = 0; result.m[3][1] = 0; result.m[3][2] = 0; result.m[3][3] = 1;
        return result;
    }

    Matrix4x4 MakeRotateYMatrix(float radian) {
        Matrix4x4 result;
        result.m[0][0] = std::cos(radian); result.m[0][1] = 0; result.m[0][2] = -std::sin(radian); result.m[0][3] = 0;
        result.m[1][0] = 0; result.m[1][1] = 1; result.m[1][2] = 0; result.m[1][3] = 0;
        result.m[2][0] = std::sin(radian); result.m[2][1] = 0; result.m[2][2] = std::cos(radian); result.m[2][3] = 0;
        result.m[3][0] = 0; result.m[3][1] = 0; result.m[3][2] = 0; result.m[3][3] = 1;
        return result;
    }

    Matrix4x4 MakeRotateZMatrix(float radian) {
        Matrix4x4 result;
        result.m[0][0] = std::cos(radian); result.m[0][1] = std::sin(radian); result.m[0][2] = 0; result.m[0][3] = 0;
        result.m[1][0] = -std::sin(radian); result.m[1][1] = std::cos(radian); result.m[1][2] = 0; result.m[1][3] = 0;
        result.m[2][0] = 0; result.m[2][1] = 0; result.m[2][2] = 1; result.m[2][3] = 0;
        result.m[3][0] = 0; result.m[3][1] = 0; result.m[3][2] = 0; result.m[3][3] = 1;
        return result;
    }


};


/*		----- Standard Mouse Axes in DirectInput ----------
 
Axis		Member			Meaning						Use

X			lX				Horizontal movement			Yaw(left/right)
		
Y			lY				Vertical movement			Pitch(up/down)

Z			lZ				Mouse Wheel					Zoom/Scroll
	
Button 0	rgbButtons[0]	Left Button					Activate Look

Button 1	rgbButtons[1]	Right Button				Activate Roll

Button 2	rbgButtons[2]	Middle Button/ Wheel Click	Alternative Action

*/









